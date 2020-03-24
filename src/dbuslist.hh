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

#include "list.hh"
#include "de_tahifi_lists.h"
#include "ramlist.hh"
#include "cache_segment.hh"
#include "i18nstring.hh"
#include "context_map.hh"
#include "messages.h"
#include "rnfcall_get_range.hh"
#include "dbus_async.hh"
#include "de_tahifi_lists_item_kinds.hh"
#include "logged_lock.hh"

/*!
 * \addtogroup dbus_list Lists with contents filled directly from D-Bus
 * \ingroup list
 */
/*!@{*/

namespace List
{

/*!
 * Base class for asynchronous D-Bus query contexts.
 *
 * Typically, derived classes will make use of an #DBus::AsyncCall object to
 * handle a bare asynchronous D-Bus method call for doing something with a
 * remote list. To be able to do anything with the result of the method call,
 * they will also add in some extra data and state to handle the specific
 * method call in various contexts.
 */
class QueryContext_
{
  protected:
    AsyncListIface &result_receiver_;
    const unsigned short caller_id_;

    explicit QueryContext_(AsyncListIface &list, unsigned short caller_id):
        result_receiver_(list),
        caller_id_(caller_id)
    {}

    QueryContext_(const QueryContext_ &) = default;

  public:
    QueryContext_ &operator=(const QueryContext_ &) = delete;

    virtual ~QueryContext_() = default;

    /*!
     * Start running asynchronous D-Bus operation.
     *
     * \returns
     *     True if the result of the operation is available at the time this
     *     function returns, false if the asynchronous operation is in
     *     progress. Note that a return value of true does \e not indicate
     *     success.
     */
    virtual bool run_async(DBus::AsyncResultAvailableFunction &&result_available) = 0;

    /*!
     * Wait for result, error, or cancelation of asynchronous D-Bus operation.
     *
     * \param result
     *     Result of the asynchronous operation. A useful value is always
     *     returned in this parameter, regardless of the outcome of the
     *     function call.
     *
     * \returns
     *     True if the operation finished successfully and a result is
     *     available, false otherwise. In case of failure, the function cleans
     *     up the asynchronous operation.
     *
     * \note
     *     This function may throw a #List::DBusListException in case of
     *     failure.
     */
    virtual bool synchronize(DBus::AsyncResult &result) = 0;

    /*!
     * Cancel asynchronous operation, if any.
     *
     * \returns
     *     True if there is an operation in progress (and it was attempted to
     *     be canceled), false if there is no operation in progress.
     */
    virtual DBus::CancelResult cancel(bool will_be_restarted) = 0;

  protected:
    /*!
     * Cancel asynchronous operation, if any, and wait for it to happen.
     *
     * \note
     *     Implementations of the #List::QueryContext_ interface must override
     *     this function. An empty implementation must simply return
     *     #DBus::CancelResult::CANCELED.
     *
     * \note
     *     Implementations must call their version of \c cancel_sync() from
     *     their destructor.
     *
     * \note
     *     Regular code should favor #List::QueryContext_::cancel() over this
     *     function.
     */
    virtual DBus::CancelResult cancel_sync() = 0;
};

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
        SYNC_WRAPPER,
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
                                   ID::List list_id, unsigned int line,
                                   I18n::String &&title):
        QueryContext_(result_receiver, caller_id),
        proxy_(proxy),
        parameters_({list_id, line, std::move(title)})
    {}

    /*!
     * Constructor for restarting the call with different parameters.
     */
    explicit QueryContextEnterList(const QueryContextEnterList &src,
                                   ID::List list_id, unsigned int line,
                                   I18n::String &&title):
        QueryContext_(src),
        proxy_(src.proxy_),
        parameters_({list_id, line, std::move(title)})
    {}

    ~QueryContextEnterList()
    {
        cancel_sync();
    }

    CallerID get_caller_id() const { return static_cast<CallerID>(caller_id_); }

    bool run_async(DBus::AsyncResultAvailableFunction &&result_available) final override;
    bool synchronize(DBus::AsyncResult &result) final override;

    static List::AsyncListIface::OpResult
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
        catch(const List::DBusListException &e)
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
 * How to modify the window of cached items when scrolling through the list.
 */
struct CacheModifications
{
    /* for all updates */
    bool is_filling_from_scratch_;
    unsigned int new_first_line_;

    /* for partial updates */
    bool is_shift_down_;
    unsigned int shift_distance_;

    CacheModifications(const CacheModifications &) = default;
    CacheModifications &operator=(const CacheModifications &) = default;

    explicit CacheModifications():
        is_filling_from_scratch_(false),
        new_first_line_(0),
        is_shift_down_(false),
        shift_distance_(0)
    {}

    void set(int new_first_line)
    {
        is_filling_from_scratch_ = true;
        new_first_line_ = new_first_line;
        is_shift_down_ = false;
        shift_distance_ = 0;
    }

    void set(unsigned int new_first_line,
             bool is_shift_down, unsigned int shift_distance)
    {
        is_filling_from_scratch_ = false;
        new_first_line_ = new_first_line;
        is_shift_down_ = is_shift_down;
        shift_distance_ = shift_distance;
    }
};

/*!
 * Context for getting a D-Bus list item asynchronously.
 */
class QueryContextGetItem: public DBusRNF::ContextData
{
  public:
    unsigned int cache_list_replace_index_;

    explicit QueryContextGetItem(unsigned int replace_index,
                                 ContextData::NotificationFunction &&notify):
        DBusRNF::ContextData(std::move(notify)),
        cache_list_replace_index_(replace_index)
    {}
};

/*!
 * A list filled from D-Bus, with only fractions of the list held in RAM.
 */
class DBusList: public ListIface, public AsyncListIface
{
  public:
    typedef List::Item *(*const NewItemFn)(const char *name, ListItemKind kind,
                                           const char *const *names);

    using AsyncWatcher =
        std::function<void(OpEvent, OpResult, std::shared_ptr<QueryContext_>)>;

  private:
    const std::string list_iface_name_;
    DBusRNF::CookieManagerIface &cm_;
    tdbuslistsNavigation *const dbus_proxy_;

    struct AsyncDBusData
    {
        LoggedLock::RecMutex lock_;

        AsyncWatcher event_watcher_;

        std::shared_ptr<QueryContextEnterList> enter_list_query_;
        std::shared_ptr<DBusRNF::GetRangeCallBase> get_range_query_;

        bool is_canceling_;

        AsyncDBusData():
            is_canceling_(false)
        {
            LoggedLock::configure(lock_, "DBusListAsyncData", MESSAGE_LEVEL_DEBUG);
        }

        /*!
         * Stop entering list.
         *
         * Must be called while holding #List::DBusList::AsyncDBusData::lock_.
         */
        DBus::CancelResult cancel_enter_list_query()
        {
            auto local_ref(enter_list_query_);

            if(local_ref == nullptr)
                return DBus::CancelResult::NOT_RUNNING;

            const auto ret = local_ref->cancel_sync();

            switch(ret)
            {
              case DBus::CancelResult::NOT_RUNNING:
              case DBus::CancelResult::BLOCKED_RECURSIVE_CALL:
                break;

              case DBus::CancelResult::CANCELED:
                enter_list_query_ = nullptr;
                break;
            }

            return ret;
        }

        /*!
         * Stop loading items.
         *
         * Must be called while holding #List::DBusList::AsyncDBusData::lock_.
         */
        DBus::CancelResult cancel_get_range_query()
        {
            LOGGED_LOCK_CONTEXT_HINT;
            std::lock_guard<LoggedLock::RecMutex> lock(lock_);

            auto local_ref(std::move(get_range_query_));

            if(local_ref == nullptr)
                return DBus::CancelResult::NOT_RUNNING;

            return local_ref->abort_request()
                ? DBus::CancelResult::CANCELED
                : DBus::CancelResult::NOT_RUNNING;
        }

        DBus::CancelResult cancel_all()
        {
            LOGGED_LOCK_CONTEXT_HINT;
            std::lock_guard<LoggedLock::RecMutex> lock(lock_);

            if(is_canceling_)
                return DBus::CancelResult::BLOCKED_RECURSIVE_CALL;

            is_canceling_ = true;

            DBus::CancelResult ret_enter_list = cancel_enter_list_query();
            DBus::CancelResult ret_get_item = cancel_get_range_query();

            is_canceling_ = false;

            if(ret_enter_list == DBus::CancelResult::CANCELED ||
               ret_get_item == DBus::CancelResult::CANCELED)
                return DBus::CancelResult::CANCELED;

            if(ret_enter_list == DBus::CancelResult::NOT_RUNNING &&
               ret_get_item == DBus::CancelResult::NOT_RUNNING)
                return DBus::CancelResult::NOT_RUNNING;

            return DBus::CancelResult::BLOCKED_RECURSIVE_CALL;
        }

        std::string get_description_get_item();
    };

    AsyncDBusData async_dbus_data_;

    /*!
     * List contexts known by the list broker we are going to talk to.
     *
     * So that we can decide which D-Bus method to call when requesting a range
     * of items from a list.
     */
    const List::ContextMap &list_contexts_;

    /*!
     * Window size.
     */
    const unsigned int number_of_prefetched_items_;

    /*!
     * Callback that constructs a #List::Item from raw list data.
     */
    const NewItemFn new_item_fn_;

    /*!
     * Total number of items as reported over D-Bus.
     *
     * This gets updated by #List::DBusList::enter_list().
     */
    unsigned int number_of_items_;

    /*!
     * Simple POD structure for storing a little window of the list.
     */
    struct CacheData
    {
      public:
        ID::List list_id_;
        unsigned int first_item_line_;
        RamList items_;
        CacheSegment valid_segment_;

        CacheData(const CacheData &) = delete;
        CacheData &operator=(const CacheData &) = delete;

        explicit CacheData(const std::string &parent_list_iface_name, const char *which):
            first_item_line_(0),
            items_(std::move(parent_list_iface_name + " segment " + which)),
            valid_segment_(0, 0)
        {}

        const List::Item *operator[](unsigned int line) const
        {
            log_assert(line >= first_item_line_);
            log_assert(line < first_item_line_ + items_.get_number_of_items());

            return items_.get_item(line - first_item_line_);
        }

        void clone(const CacheData &src)
        {
            list_id_ = src.list_id_;
            first_item_line_ = src.first_item_line_;
            items_.clear();
            valid_segment_.line_ = src.valid_segment_.line_;
            valid_segment_.count_ = 0;
        }

        void move_from(CacheData &other)
        {
            list_id_ = other.list_id_;
            first_item_line_ = other.first_item_line_;
            items_.move_from(other.items_);
            valid_segment_ = other.valid_segment_;
            other.valid_segment_.count_ = 0;
        }

        void clear_for_line(ID::List list_id, unsigned int line)
        {
            list_id_ = list_id;
            first_item_line_ = line;
            items_.clear();
            valid_segment_.line_ = line;
            valid_segment_.count_ = 0;
        }
    };

    CacheData window_;

    CacheData window_stash_;
    bool window_stash_is_in_use_;

  public:
    DBusList(const DBusList &) = delete;
    DBusList &operator=(const DBusList &) = delete;

    explicit DBusList(std::string &&list_iface_name,
                      DBusRNF::CookieManagerIface &cm,
                      tdbuslistsNavigation *nav_proxy,
                      const List::ContextMap &list_contexts,
                      unsigned int prefetch, NewItemFn new_item_fn):
        list_iface_name_(std::move(list_iface_name)),
        cm_(cm),
        dbus_proxy_(nav_proxy),
        list_contexts_(list_contexts),
        number_of_prefetched_items_(prefetch),
        new_item_fn_(new_item_fn),
        number_of_items_(0),
        window_(list_iface_name_, "window"),
        window_stash_(list_iface_name_, "stash"),
        window_stash_is_in_use_(false)
    {}

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
    void register_watcher(const AsyncWatcher &event_handler)
    {
        async_dbus_data_.event_watcher_ = event_handler;
    }

    void clone_state(const DBusList &src);

    void push_cache_state()
    {
        log_assert(!window_stash_is_in_use_);
        window_stash_is_in_use_ = true;
        window_stash_.clone(window_);
    }

    void pop_cache_state()
    {
        log_assert(window_stash_is_in_use_);
        window_.move_from(window_stash_);
        window_stash_is_in_use_ = false;
    }

    std::string get_description_get_item()
    {
        return async_dbus_data_.get_description_get_item();
    }

    const std::string &get_list_iface_name() const override { return list_iface_name_; }
    const std::string &get_async_list_iface_name() const override { return list_iface_name_; }

    unsigned int get_number_of_items() const override;
    bool empty() const override;

    void enter_list(ID::List list_id, unsigned int line) override;
    OpResult enter_list_async(ID::List list_id, unsigned int line,
                              QueryContextEnterList::CallerID caller,
                              I18n::String &&dynamic_title)
    {
        return enter_list_async(list_id, line, static_cast<unsigned short>(caller),
                                std::move(dynamic_title));
    }

    const Item *get_item(unsigned int line) const override;

    OpResult get_item_async(unsigned int line, const Item *&item) override;

    OpResult get_item_async_set_hint(unsigned int line, unsigned int count,
                                     DBusRNF::StatusWatcher &&status_watcher,
                                     HintItemDoneNotification &&hinted_fn) override;

    bool cancel_all_async_calls() final override;

    ID::List get_list_id() const override { return window_.list_id_; }
    void list_invalidate(ID::List list_id, ID::List replacement_id);

    const ContextInfo &get_context_info_by_list_id(ID::List id) const;

    const ContextInfo &get_context_info() const
    {
        return get_context_info_by_list_id(window_.list_id_);
    }

    DBusRNF::CookieManagerIface &get_cookie_manager() const { return cm_; }

    tdbuslistsNavigation *get_dbus_proxy() const { return dbus_proxy_; }

  private:
    CacheSegmentState get_cache_segment_state(const CacheSegment &segment,
                                              unsigned int &size_of_cached_segment,
                                              unsigned int &size_of_loading_segment) const;

    bool is_line_cached(unsigned int line) const
    {
        return window_.valid_segment_.contains_line(line);
    }

    bool is_line_loading(unsigned int line) const
    {
        if(async_dbus_data_.get_range_query_ != nullptr)
            return async_dbus_data_.get_range_query_->loading_segment_.contains_line(line);
        else
            return false;
    }

    bool is_position_unchanged(ID::List list_id, unsigned int line) const;

    bool can_scroll_to_line(unsigned int line, unsigned int prefetch_hint,
                            CacheModifications &cm,
                            unsigned int &fetch_head, unsigned int &fetch_count,
                            unsigned int &cache_list_replace_index) const;

    /*!
     * Trigger asynchronous fetching of cached window segment.
     *
     * Must be called while holding #List::DBusList::AsyncDBusData::lock_ of
     * the embedded #List::DBusList::async_dbus_data_ structure.
     * */
    OpResult load_segment_in_background(const CacheSegment &prefetch_segment,
                                        int keep_cache_entries,
                                        unsigned int current_number_of_loading_items,
                                        DBusRNF::StatusWatcher &&status_watcher,
                                        HintItemDoneNotification &&hinted_fn);

    /*!
     * Little helper that calls the event watcher.
     */
    void notify_watcher(OpEvent event, OpResult result,
                        std::shared_ptr<QueryContext_> ctx)
    {
        if(async_dbus_data_.event_watcher_ != nullptr)
            async_dbus_data_.event_watcher_(event, result, std::move(ctx));
    }

    OpResult enter_list_async(ID::List list_id, unsigned int line,
                              unsigned short caller_id,
                              I18n::String &&dynamic_title) override;

    /*!
     * Modify visible part of list represented by #List::DBusList::CacheData.
     *
     * Must be called after starting a #List::QueryContextGetItem operation and
     * before notifying the watcher. The effect will be that the visible part
     * of the list is updated before its contents are available. This allows
     * providing instantly visible feedback to the user.
     */
    void apply_cache_modifications(const CacheModifications &cm);

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
     * #List::DBusList::AsyncDBusData::lock_ of the embedded
     * #List::DBusList::async_dbus_data_ structure.
     */
    void async_done_notification(DBus::AsyncCall_ &async_call);

    /*!
     * Internal function called by #async_done_notification().
     *
     * Must be called while holding #List::DBusList::AsyncDBusData::lock_ of
     * the embedded #List::DBusList::async_dbus_data_ structure.
     */
    void enter_list_async_handle_done(std::shared_ptr<QueryContextEnterList> q);

    /*!
     * Internal function notified by #List::QueryContextGetItem.
     *
     * Must be called while holding #List::DBusList::AsyncDBusData::lock_ of
     * the embedded #List::DBusList::async_dbus_data_ structure.
     */
    void get_item_result_available_notification(
            HintItemDoneNotification &&hinted_fn,
            std::shared_ptr<DBusRNF::GetRangeCallBase> call);
};

}

/*!@}*/

#endif /* !DBUSLIST_HH */
