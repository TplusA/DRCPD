/*
 * Copyright (C) 2015--2017, 2019, 2020  T+A elektroakustik GmbH & Co. KG
 *
 * This file is part of DRCPD.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301, USA.
 */

#ifndef DBUSLIST_HH
#define DBUSLIST_HH

#include "dbuslist_viewport.hh"
#include "dbuslist_query_context.hh"
#include "de_tahifi_lists.h"
#include "context_map.hh"

#include <unordered_map>

/*!
 * \addtogroup dbus_list Lists with contents filled directly from D-Bus
 * \ingroup list
 */
/*!@{*/

namespace List
{

/*!
 * Context for entering a D-Bus list asynchronously.
 */
class QueryContextEnterList: public QueryContext_
{
  private:
    tdbuslistsNavigation *proxy_;

  public:
    enum class CallerID
    {
        ENTER_ROOT,
        ENTER_CHILD,
        ENTER_PARENT,
        ENTER_CONTEXT_ROOT,
        ENTER_ANYWHERE,
        RELOAD_LIST,
        CRAWLER_RESET_POSITION,
        CRAWLER_FIRST_ENTRY,
        CRAWLER_DESCEND,
        CRAWLER_ASCEND,
    };

    const struct
    {
        ID::List list_id_;
        unsigned int line_;
        I18n::String title_;
        const DBusListViewport *associated_viewport_;
    }
    parameters_;

    using AsyncListNavCheckRange =
        DBus::AsyncCall<tdbuslistsNavigation, guint, Busy::Source::CHECKING_LIST_RANGE>;

    std::shared_ptr<AsyncListNavCheckRange> async_call_;

    QueryContextEnterList(const QueryContextEnterList &) = delete;
    QueryContextEnterList &operator=(const QueryContextEnterList &) = delete;

    explicit QueryContextEnterList(AsyncListIface &result_receiver,
                                   unsigned short caller_id,
                                   tdbuslistsNavigation *proxy,
                                   const DBusListViewport *associated_viewport,
                                   ID::List list_id, unsigned int line,
                                   I18n::String &&title):
        QueryContext_(result_receiver, caller_id),
        proxy_(proxy),
        parameters_({list_id, line, std::move(title), associated_viewport})
    {}

    /*!
     * Constructor for restarting the call with different parameters.
     */
    explicit QueryContextEnterList(const QueryContextEnterList &src,
                                   const DBusListViewport *associated_viewport,
                                   ID::List list_id, unsigned int line,
                                   I18n::String &&title):
        QueryContext_(src),
        proxy_(src.proxy_),
        parameters_({list_id, line, std::move(title), associated_viewport})
    {}

    ~QueryContextEnterList()
    {
        cancel_sync();
    }

    CallerID get_caller_id() const { return static_cast<CallerID>(caller_id_); }

    bool run_async(DBus::AsyncResultAvailableFunction &&result_available) final override;
    bool synchronize(DBus::AsyncResult &result) final override;

    static AsyncListIface::OpResult
    restart_if_necessary(std::shared_ptr<QueryContextEnterList> &ctx,
                         ID::List invalidated_list_id, ID::List replacement_id);

    DBus::CancelResult cancel(bool will_be_restarted = false) final override
    {
        if(async_call_ != nullptr)
            return async_call_->cancel(will_be_restarted);
        else
            return DBus::CancelResult::NOT_RUNNING;
    }

    DBus::CancelResult cancel_sync() final override
    {
        const DBus::CancelResult ret = cancel();

        switch(ret)
        {
          case DBus::CancelResult::CANCELED:
            break;

          case DBus::CancelResult::BLOCKED_RECURSIVE_CALL:
          case DBus::CancelResult::NOT_RUNNING:
            return ret;
        }

        try
        {
            DBus::AsyncResult dummy;

            synchronize(dummy);
        }
        catch(const DBusListException &e)
        {
            BUG("Got list exception while synchronizing enter-list cancel: %s",
                e.what());
        }
        catch(const std::exception &e)
        {
            BUG("Got std exception while synchronizing enter-list cancel: %s",
                e.what());
        }
        catch(...)
        {
            BUG("Got unknown exception while synchronizing enter-list cancel");
        }

        return ret;
    }

  private:
    static void put_result(DBus::AsyncResult &async_ready,
                           AsyncListNavCheckRange::PromiseType &promise,
                           tdbuslistsNavigation *p, GAsyncResult *async_result,
                           GErrorWrapper &error, ID::List list_id);
};

/*!
 * A list filled from D-Bus, with only fractions of the list held in RAM.
 */
class DBusList: public ListIface, public AsyncListIface
{
  public:
    using EnterWatcher =
        std::function<void(OpResult, std::shared_ptr<QueryContextEnterList>)>;

    using ViewportsAndFetchersMap =
        std::unordered_map<std::shared_ptr<DBusListViewport>,
                           std::shared_ptr<DBusListSegmentFetcher>>;

  private:
    mutable LoggedLock::RecMutex lock_;

    const std::string list_iface_name_;
    DBusRNF::CookieManagerIface &cm_;
    tdbuslistsNavigation *const dbus_proxy_;

    ID::List list_id_;

    ViewportsAndFetchersMap viewports_and_fetchers_;

    struct EnterListData
    {
        EnterWatcher enter_watcher_;

        std::shared_ptr<QueryContextEnterList> query_;
        bool is_canceling_;

        explicit EnterListData(): is_canceling_(false) {}

        /*!
         * Stop entering list.
         *
         * Must be called while holding #List::DBusList::lock_.
         */
        DBus::CancelResult cancel_enter_list_query()
        {
            auto local_ref(query_);

            if(local_ref == nullptr)
                return DBus::CancelResult::NOT_RUNNING;

            const auto ret = local_ref->cancel_sync();

            switch(ret)
            {
              case DBus::CancelResult::NOT_RUNNING:
              case DBus::CancelResult::BLOCKED_RECURSIVE_CALL:
                break;

              case DBus::CancelResult::CANCELED:
                query_ = nullptr;
                break;
            }

            return ret;
        }

        DBus::CancelResult
        cancel_all(ViewportsAndFetchersMap &viewports_and_fetchers)
        {
            if(is_canceling_)
                return DBus::CancelResult::BLOCKED_RECURSIVE_CALL;

            is_canceling_ = true;

            DBus::CancelResult ret_enter_list = cancel_enter_list_query();
            DBus::CancelResult ret_get_item = DBus::CancelResult::NOT_RUNNING;

            for(auto &vf : viewports_and_fetchers)
                if(vf.second != nullptr &&
                   vf.second->cancel_op() == DBus::CancelResult::CANCELED)
                    // cppcheck-suppress useStlAlgorithm
                    ret_get_item = DBus::CancelResult::CANCELED;

            is_canceling_ = false;

            if(ret_enter_list == DBus::CancelResult::CANCELED ||
               ret_get_item == DBus::CancelResult::CANCELED)
                return DBus::CancelResult::CANCELED;

            if(ret_enter_list == DBus::CancelResult::NOT_RUNNING &&
               ret_get_item == DBus::CancelResult::NOT_RUNNING)
                return DBus::CancelResult::NOT_RUNNING;

            return DBus::CancelResult::BLOCKED_RECURSIVE_CALL;
        }
    };

    EnterListData enter_list_data_;

    /*!
     * List contexts known by the list broker we are going to talk to.
     *
     * So that we can decide which D-Bus method to call when requesting a range
     * of items from a list.
     */
    const ContextMap &list_contexts_;

    /*!
     * Callback that constructs a #List::Item from raw list data.
     */
    const DBusListViewport::NewItemFn new_item_fn_;

    /*!
     * Total number of items as reported over D-Bus.
     *
     * This gets updated by #List::DBusList::enter_list().
     */
    unsigned int number_of_items_;

  public:
    DBusList(const DBusList &) = delete;
    DBusList &operator=(const DBusList &) = delete;

    explicit DBusList(std::string &&list_iface_name,
                      DBusRNF::CookieManagerIface &cm,
                      tdbuslistsNavigation *nav_proxy,
                      const ContextMap &list_contexts,
                      const DBusListViewport::NewItemFn &new_item_fn):
        list_iface_name_(std::move(list_iface_name)),
        cm_(cm),
        dbus_proxy_(nav_proxy),
        list_contexts_(list_contexts),
        new_item_fn_(new_item_fn),
        number_of_items_(0)
    {
        LoggedLock::configure(lock_, "DBusList", MESSAGE_LEVEL_DEBUG);
    }

    /*!
     * Register a callback that is called on certain events.
     *
     * The callback is called whenever an asynchronous operation completes in
     * \e any way, successful or not. The function may want to make use of the
     * query context that led to the event, in which case it must downcast the
     * #List::QueryContext_ to something else (such as
     * #List::QueryContextEnterList).)
     *
     * Note that the callback may be called from different contexts, depending
     * on the event. The callback function must make sure to provide correct
     * synchronization.
     */
    void register_enter_list_watcher(EnterWatcher &&event_handler)
    {
        enter_list_data_.enter_watcher_ = std::move(event_handler);
    }

    auto mk_viewport(unsigned int prefetch, const char *which) const
    {
        return std::make_shared<DBusListViewport>(list_iface_name_, prefetch, which);
    }

    std::string get_get_range_op_description(const DBusListViewport &viewport) const;
    const std::string &get_list_iface_name() const override { return list_iface_name_; }
    const std::string &get_async_list_iface_name() const override { return list_iface_name_; }

    unsigned int get_number_of_items() const override;
    bool empty() const override;

    void enter_list(ID::List list_id) override;

    OpResult enter_list_async(const ListViewportBase *associated_viewport,
                              ID::List list_id, unsigned int line,
                              unsigned short caller_id,
                              I18n::String &&dynamic_title) override
    {
        return enter_list_async(
                    static_cast<const DBusListViewport *>(associated_viewport),
                    list_id, line,
                    static_cast<QueryContextEnterList::CallerID>(caller_id),
                    std::move(dynamic_title));
    }

    OpResult enter_list_async(const DBusListViewport *associated_viewport,
                              ID::List list_id, unsigned int line,
                              QueryContextEnterList::CallerID caller_id,
                              I18n::String &&dynamic_title);

    const Item *get_item(std::shared_ptr<ListViewportBase> vp,
                         unsigned int line) override
    {
        return get_item(std::static_pointer_cast<DBusListViewport>(vp), line);
    }

    const Item *get_item(std::shared_ptr<DBusListViewport> vp,
                         unsigned int line);

    OpResult get_item_async(std::shared_ptr<ListViewportBase> vp,
                            unsigned int line, const Item *&item) override
    {
        return get_item_async(std::static_pointer_cast<DBusListViewport>(vp),
                              line, item);
    }

    OpResult get_item_async(std::shared_ptr<DBusListViewport> vp,
                            unsigned int line, const Item *&item);

    OpResult get_item_async_set_hint(std::shared_ptr<ListViewportBase> vp,
                                     unsigned int line, unsigned int count,
                                     DBusRNF::StatusWatcher &&status_watcher,
                                     HintItemDoneNotification &&hinted_fn) override
    {
        return get_item_async_set_hint(
                    std::static_pointer_cast<DBusListViewport>(vp),
                    line, count, std::move(status_watcher), std::move(hinted_fn));
    }

    OpResult get_item_async_set_hint(std::shared_ptr<DBusListViewport> vp,
                                     unsigned int line, unsigned int count,
                                     DBusRNF::StatusWatcher &&status_watcher,
                                     HintItemDoneNotification &&hinted_fn);

    bool cancel_all_async_calls() final override;

    ID::List get_list_id() const override { return list_id_; }
    void list_invalidate(ID::List list_id, ID::List replacement_id);

    const ContextInfo &get_context_info_by_list_id(ID::List id) const;

    const ContextInfo &get_context_info() const
    {
        return get_context_info_by_list_id(list_id_);
    }

    DBusRNF::CookieManagerIface &get_cookie_manager() const { return cm_; }

    tdbuslistsNavigation *get_dbus_proxy() const { return dbus_proxy_; }

    void detach_viewport(std::shared_ptr<DBusListViewport> vp);

  private:
    void add_referrer(std::shared_ptr<DBusListViewport> vp);

    std::shared_ptr<DBusRNF::GetRangeCallBase> mk_get_range_rnf_call(
            ID::List list_id, bool with_meta_data,
            Segment &&segment, std::unique_ptr<QueryContextGetItem> ctx,
            DBusRNF::StatusWatcher &&watcher) const;

    bool is_position_unchanged(ID::List list_id, unsigned int line) const;

    /*!
     * Little helper that calls the enter-list event watcher.
     */
    void notify_watcher(OpResult result,
                        std::shared_ptr<QueryContextEnterList> ctx)
    {
        if(enter_list_data_.enter_watcher_ != nullptr)
            enter_list_data_.enter_watcher_(result, std::move(ctx));
    }

    /*!
     * Callback that is called when an asynchronous D-Bus operation finishes.
     *
     * This function is called for each successfully started async operation
     * invoked through the #List::AsyncListIface interface. It is called in
     * case of successful completion, canceled operations, and failure.
     *
     * Note that this function may be called from a different context than
     * functions such as #List::AsyncListIface::enter_list_async(). This
     * function takes care of proper synchronization by locking
     * #List::DBusList::lock_ of the embedded #List::DBusList::enter_list_data_
     * structure.
     */
    void async_done_notification(DBus::AsyncCall_ &async_call);

    /*!
     * Internal function called by #async_done_notification().
     *
     * Must be called while holding #List::DBusList::lock_.
     */
    void enter_list_async_handle_done(std::shared_ptr<QueryContextEnterList> q);

    /*!
     * Internal function notified by #List::QueryContextGetItem.
     *
     * Must be called while holding #List::DBusList::lock_.
     */
    void get_item_result_available_notification(DBusListSegmentFetcher &fetcher,
                                                HintItemDoneNotification &&hinted_fn);
};

}

/*!@}*/

#endif /* !DBUSLIST_HH */
