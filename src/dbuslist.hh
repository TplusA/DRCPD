/*
 * Copyright (C) 2015, 2016  T+A elektroakustik GmbH & Co. KG
 *
 * This file is part of DRCPD.
 *
 * DRCPD is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 3 as
 * published by the Free Software Foundation.
 *
 * DRCPD is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with DRCPD.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef DBUSLIST_HH
#define DBUSLIST_HH

#include <memory>
#include <functional>
#include <tuple>

#include "list.hh"
#include "lists_dbus.h"
#include "ramlist.hh"
#include "context_map.hh"
#include "messages.h"
#include "dbuslist_exception.hh"
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

  public:
    QueryContext_(const QueryContext_ &) = delete;
    QueryContext_ &operator=(const QueryContext_ &) = delete;

    virtual ~QueryContext_()
    {
        cancel_sync();
    }

    /*!
     * Start running asynchronous D-Bus operation.
     *
     * \returns
     *     True if the result of the operation is available at the time this
     *     function returns, false if the asynchronous operation is in
     *     progress. Note that a return value of true does \e not indicate
     *     success.
     */
    virtual bool run_async(const DBus::AsyncResultAvailableFunction &result_available) = 0;

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
    virtual bool cancel() = 0;

  protected:
    /*!
     * Cancel asynchronous operation, if any, and wait for it to happen.
     *
     * \returns
     *     True if an ongoing operation has been canceled and the call object
     *     is safe to be deleted, false if there is no operation in progress or
     *     the call object cannot be deleted (zombie or already gone).
     *
     * \note
     *     To make any sense, implementations of the #List::QueryContext_
     *     interface should override this function.
     *
     * \note
     *     Regular code should favor #List::QueryContext_::cancel() over this
     *     function.
     */
    virtual bool cancel_sync() { return false; }
};

/*!
 * Context for entering a D-Bus list asynchronously.
 */
class QueryContextEnterList: public QueryContext_
{
  public:
    enum class CallerID
    {
        SYNC_WRAPPER,
        ENTER_ROOT,
        ENTER_CHILD,
    };

    tdbuslistsNavigation *proxy_;

    const struct
    {
        ID::List list_id_;
        unsigned int line_;
    }
    parameters_;

    using AsyncListNavCheckRange = DBus::AsyncCall<tdbuslistsNavigation, guint>;

    AsyncListNavCheckRange *async_call_;

    QueryContextEnterList(const QueryContextEnterList &) = delete;
    QueryContextEnterList &operator=(const QueryContextEnterList &) = delete;

    explicit QueryContextEnterList(AsyncListIface &result_receiver,
                                   unsigned short caller_id,
                                   tdbuslistsNavigation *proxy,
                                   ID::List list_id, unsigned int line):
        QueryContext_(result_receiver, caller_id),
        proxy_(proxy),
        parameters_({list_id, line}),
        async_call_(nullptr)
    {}

    virtual ~QueryContextEnterList()
    {
        if(cancel_sync())
            delete async_call_;

        async_call_ = nullptr;
    }

    CallerID get_caller_id() const { return static_cast<CallerID>(caller_id_); }

    bool run_async(const DBus::AsyncResultAvailableFunction &result_available) final override;
    bool synchronize(DBus::AsyncResult &result) final override;

    bool cancel() final override
    {
        if(async_call_ != nullptr)
        {
            async_call_->cancel();
            return true;
        }
        else
            return false;
    }

    bool cancel_sync() final override
    {
        if(!cancel())
            return false;

        try
        {
            DBus::AsyncResult dummy;

            if(synchronize(dummy))
                return true;
        }
        catch(...)
        {
            /* ignore, the #DBus::AsyncCall is already deleted or a zombie */
        }

        return false;
    }

  private:
    static void put_result(DBus::AsyncResult &async_success,
                           AsyncListNavCheckRange::PromiseType &promise,
                           tdbuslistsNavigation *p, GAsyncResult *async_result,
                           GError *&error, ID::List list_id)
        throw(List::DBusListException);
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

    CacheModifications(const CacheModifications &) = delete;
    CacheModifications &operator=(const CacheModifications &) = delete;

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
class QueryContextGetItem: public QueryContext_
{
  public:
    enum class CallerID
    {
        SYNC_WRAPPER,
        SELECT_IN_VIEW,
        SERIALIZE,
        SERIALIZE_DEBUG,
    };

    tdbuslistsNavigation *proxy_;

    const struct
    {
        ID::List list_id_;
        unsigned int line_;
        unsigned int count_;
        bool have_meta_data_;
        unsigned int cache_list_replace_index_;
    }
    parameters_;

    using AsyncListNavGetRange =
        DBus::AsyncCall<tdbuslistsNavigation, std::tuple<guchar, guint, GVariant *>>;

    AsyncListNavGetRange *async_call_;

    QueryContextGetItem(const QueryContextGetItem &) = delete;
    QueryContextGetItem &operator=(const QueryContextGetItem &) = delete;

    explicit QueryContextGetItem(AsyncListIface &result_receiver,
                                 unsigned short caller_id,
                                 tdbuslistsNavigation *proxy,
                                 ID::List list_id,
                                 unsigned int line, unsigned int count,
                                 bool have_meta_data,
                                 unsigned int replace_index):
        QueryContext_(result_receiver, caller_id),
        proxy_(proxy),
        parameters_({list_id, line, count, have_meta_data, replace_index}),
        async_call_(nullptr)
    {}

    virtual ~QueryContextGetItem()
    {
        if(cancel_sync())
            delete async_call_;

        async_call_ = nullptr;
    }

    CallerID get_caller_id() const { return static_cast<CallerID>(caller_id_); }

    bool run_async(const DBus::AsyncResultAvailableFunction &result_available) final override;
    bool synchronize(DBus::AsyncResult &result) final override;

    bool cancel() final override
    {
        if(async_call_ != nullptr)
        {
            async_call_->cancel();
            return true;
        }
        else
            return false;
    }

    bool cancel_sync() final override
    {
        if(!cancel())
            return false;

        try
        {
            DBus::AsyncResult dummy;

            if(synchronize(dummy))
                return true;
        }
        catch(...)
        {
            /* ignore, the #DBus::AsyncCall is already deleted or a zombie */
        }

        return false;
    }

    bool is_loading(unsigned int line) const
    {
        return line >= parameters_.line_ &&
               line < parameters_.line_ + parameters_.count_;
    }

  private:
    static void put_result(DBus::AsyncResult &async_ready,
                           AsyncListNavGetRange::PromiseType &promise,
                           tdbuslistsNavigation *p, GAsyncResult *async_result,
                           GError *&error,
                           ID::List list_id, bool have_meta_data)
        throw(List::DBusListException);
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
        std::function<void(OpEvent, OpResult, const std::shared_ptr<QueryContext_> &)>;

  private:
    tdbuslistsNavigation *const dbus_proxy_;

    struct AsyncDBusData
    {
        LoggedLock::Mutex lock_;
        LoggedLock::ConditionVariable query_done_;

        AsyncWatcher event_watcher_;

        std::shared_ptr<QueryContextEnterList> enter_list_query_;
        std::shared_ptr<QueryContextGetItem> get_item_query_;

        AsyncDBusData()
        {
            LoggedLock::set_name(lock_, "DBusListAsyncData");
            LoggedLock::set_name(query_done_, "DBusListAsyncDone");
        }
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

        CacheData(const CacheData &) = delete;
        CacheData &operator=(const CacheData &) = delete;

        explicit CacheData():
            first_item_line_(0)
        {}

        const List::Item *operator[](unsigned int line) const
        {
            log_assert(line >= first_item_line_);
            log_assert(line < first_item_line_ + items_.get_number_of_items());

            return items_.get_item(line - first_item_line_);
        }
    };

    CacheData window_;

  public:
    DBusList(const DBusList &) = delete;
    DBusList &operator=(const DBusList &) = delete;

    explicit DBusList(tdbuslistsNavigation *nav_proxy,
                      const List::ContextMap &list_contexts,
                      unsigned int prefetch, NewItemFn new_item_fn):
        dbus_proxy_(nav_proxy),
        list_contexts_(list_contexts),
        number_of_prefetched_items_(prefetch),
        new_item_fn_(new_item_fn),
        number_of_items_(0)
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
     * on the event. The callback function must be sure to provide correct
     * synchronization.
     */
    void register_watcher(const AsyncWatcher &event_handler)
    {
        async_dbus_data_.event_watcher_ = event_handler;
    }

    void clone_state(const DBusList &src) throw(List::DBusListException);

    unsigned int get_number_of_items() const override;
    bool empty() const override;

    void enter_list(ID::List list_id, unsigned int line) throw(List::DBusListException) override;
    bool enter_list_async_wait() override;
    OpResult enter_list_async(ID::List list_id, unsigned int line,
                              QueryContextEnterList::CallerID caller)
    {
        return enter_list_async(list_id, line, static_cast<unsigned short>(caller));
    }

    const Item *get_item(unsigned int line) const throw(List::DBusListException) override;
    bool get_item_async_wait(unsigned int line, const Item *&item) override;
    OpResult get_item_async(unsigned int line, const Item *&item,
                            QueryContextGetItem::CallerID caller)
    {
        return get_item_async(line, item, static_cast<unsigned short>(caller));
    }

    ID::List get_list_id() const override { return window_.list_id_; }

    tdbuslistsNavigation *get_dbus_proxy() const { return dbus_proxy_; }

  private:
    bool is_position_unchanged(ID::List list_id, unsigned int line) const;
    bool is_line_cached(unsigned int line) const;
    bool can_scroll_to_line(unsigned int line, CacheModifications &cm,
                            unsigned int &fetch_head, unsigned int &count,
                            unsigned int &cache_list_replace_index) const;
    bool is_line_pending(unsigned int line) const;
    bool is_line_loading(unsigned int line) const;

    /*!
     * Little helper that calls the event watcher.
     *
     * Function must be called while holding \p lock. The lock will be unlocked
     * unconditionally before calling the watcher callback.
     */
    void notify_watcher(OpEvent event, OpResult result,
                        const std::shared_ptr<QueryContext_> &ctx,
                        LoggedLock::UniqueLock<LoggedLock::Mutex> &lock)
    {
        lock.unlock();

        if(async_dbus_data_.event_watcher_ != nullptr)
            async_dbus_data_.event_watcher_(event, result, ctx);
    }

    OpResult enter_list_async(ID::List list_id, unsigned int line, unsigned short caller_id) override;
    OpResult get_item_async(unsigned int line, const Item *&item, unsigned short caller_id) override;

    /*!
     * Stop loading items.
     *
     * Must be called while holding  #List::DBusList::AsyncDBusData::lock_ of
     * the embedded #List::DBusList::async_dbus_data_ structure.
     */
    void cancel_get_item_query();

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
    void enter_list_async_handle_done(LoggedLock::UniqueLock<LoggedLock::Mutex> &lock);

    /*!
     * Internal function called by #async_done_notification().
     *
     * Must be called while holding #List::DBusList::AsyncDBusData::lock_ of
     * the embedded #List::DBusList::async_dbus_data_ structure.
     */
    void get_item_async_handle_done(LoggedLock::UniqueLock<LoggedLock::Mutex> &lock);
};

};

/*!@}*/

#endif /* !DBUSLIST_HH */
