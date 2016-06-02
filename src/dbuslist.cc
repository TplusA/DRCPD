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

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "dbuslist.hh"
#include "de_tahifi_lists_errors.hh"
#include "de_tahifi_lists_context.h"
#include "context_map.hh"
#include "streaminfo.hh"
#include "view_filebrowser.hh"
#include "messages.h"
#include "logged_lock.hh"

constexpr const char *ListError::names_[];

void List::DBusList::clone_state(const List::DBusList &src)
    throw(List::DBusListException)
{
    number_of_items_ = src.number_of_items_;
    window_.list_id_ = src.window_.list_id_;

    BUG("%s(): not implemented yet", __PRETTY_FUNCTION__);
    log_assert(false);
}

unsigned int List::DBusList::get_number_of_items() const
{
    return number_of_items_;
}

bool List::DBusList::empty() const
{
    return number_of_items_ == 0;
}

void List::DBusList::enter_list(ID::List list_id, unsigned int line)
    throw(List::DBusListException)
{
    if(enter_list_async(list_id, line,
                        QueryContextEnterList::CallerID::SYNC_WRAPPER) == OpResult::STARTED)
        enter_list_async_wait();
}

const List::Item *List::DBusList::get_item(unsigned int line) const
    throw(List::DBusListException)
{
    const List::Item *item = nullptr;

    if(const_cast<DBusList *>(this)->get_item_async(line, item,
                      QueryContextGetItem::CallerID::SYNC_WRAPPER) == OpResult::STARTED)
        const_cast<DBusList *>(this)->get_item_async_wait(line, item);

    return item;
}

bool List::DBusList::is_position_unchanged(ID::List list_id, unsigned int line) const
{
    return list_id == window_.list_id_ && line == window_.first_item_line_;
}

bool List::DBusList::is_line_cached(unsigned int line) const
{
    return (line >= window_.first_item_line_ &&
            line - window_.first_item_line_ < window_.items_.get_number_of_items());
}

static void fill_cache_list_generic(List::RamList &items,
                                    List::DBusList::NewItemFn new_item_fn,
                                    unsigned int cache_list_index,
                                    bool replace_mode, GVariant *dbus_data)
{
    GVariantIter iter;

    if(g_variant_iter_init(&iter, dbus_data) <= 0)
        return;

    const gchar *name;
    uint8_t item_kind;

    while(g_variant_iter_next(&iter, "(&sy)", &name, &item_kind))
    {
        if(replace_mode)
            items.replace(cache_list_index++,
                          new_item_fn(name, ListItemKind(item_kind), nullptr));
        else
            items.append(new_item_fn(name, ListItemKind(item_kind), nullptr));
    }
}

static void fill_cache_list_with_meta_data(List::RamList &items,
                                           List::DBusList::NewItemFn new_item_fn,
                                           unsigned int cache_list_index,
                                           bool replace_mode,
                                           GVariant *dbus_data)
{
    GVariantIter iter;

    if(g_variant_iter_init(&iter, dbus_data) <= 0)
        return;

    const gchar *names[3];
    uint8_t primary_name_index;
    uint8_t item_kind;

    while(g_variant_iter_next(&iter, "(&s&s&syy)",
                              &names[0], &names[1], &names[2],
                              &primary_name_index, &item_kind))
    {
        if(primary_name_index > sizeof(names) / sizeof(names[0]) &&
           primary_name_index != UINT8_MAX)
        {
            BUG("Got unexpected index of primary name (%u)",
                primary_name_index);
            primary_name_index = 0;
        }

        static const char empty_item_string[] = "----";

        const gchar *name = ((primary_name_index != UINT8_MAX)
                             ? names[primary_name_index]
                             : empty_item_string);

        if(primary_name_index == UINT8_MAX)
            item_kind = ListItemKind::LOCKED;

        if(replace_mode)
            items.replace(cache_list_index++,
                          new_item_fn(name, ListItemKind(item_kind), names));
        else
            items.append(new_item_fn(name, ListItemKind(item_kind), names));
    }
}

/*!
 * Check if fetching only part of the window is possible.
 *
 * In case it is possible to move the list window so that it covers the
 * requested line and part of the window, then the caller can move the window
 * according to the values returned in the function parameters and fill in only
 * the missing part.
 *
 * Note that fetching a full list window over D-Bus is not the bottleneck that
 * we are trying to avoid here: D-Bus is reasonably fast and there are only few
 * items to be fetched even in worst case anyway. The point of this
 * optimization is that fetching a full window of data may cross tile
 * boundaries of the UPnP list broker. In some unlucky cases this may trigger
 * unnecessary further UPnP communication (which is generally slow) for
 * fetching data that is already known.
 *
 * \param line
 *     Which line to scroll to.
 *
 * \param[out] cm
 *     How to modify the window to achieve the impression of scrolling.
 *
 * \param[out] fetch_head, fetch_count
 *     First missing line and how many missing lines to retrieve from the list
 *     broker.
 *
 * \param[out] cache_list_replace_index
 *     Where to insert the retrieved lines into the window.
 *
 * \returns
 *     True if the window can be scrolled, false if not. If this function
 *     returns false, then the values returned in the function parameters shall
 *     be ignored by the caller.
 */
bool List::DBusList::can_scroll_to_line(unsigned int line,
                                        CacheModifications &cm,
                                        unsigned int &fetch_head,
                                        unsigned int &fetch_count,
                                        unsigned int &cache_list_replace_index) const
{
    if(window_.items_.get_number_of_items() == 0)
        return false;

    unsigned int new_first_line;

    if(line >= window_.first_item_line_ + number_of_prefetched_items_ &&
       line < window_.first_item_line_ + 2U * number_of_prefetched_items_ - 1)
    {
        /* requested line is below current window, but we can just
         * scroll down a bit */
        new_first_line = 1U + line - number_of_prefetched_items_;
        fetch_count = new_first_line - window_.first_item_line_;
        fetch_head = line + 1U - fetch_count;
        cache_list_replace_index = window_.items_.get_number_of_items() - fetch_count;
    }
    else if(line < window_.first_item_line_ &&
            line + number_of_prefetched_items_ > window_.first_item_line_)
    {
        /* requested line is above current window, but we can just
         * scroll up a bit */
        new_first_line = line;
        fetch_count = window_.first_item_line_ - new_first_line;
        fetch_head = line;
        cache_list_replace_index = 0;
    }
    else
    {
        /* requested line is too far above or below our window so that we
         * cannot move the window to the line without losing all of our window
         * content */
        return false;
    }

    if(fetch_count == 0)
        return false;

    log_assert(fetch_count < number_of_prefetched_items_);
    log_assert(cache_list_replace_index < window_.items_.get_number_of_items());

    cm.set(new_first_line,
           new_first_line < window_.first_item_line_, fetch_count);

    return true;
}

List::AsyncListIface::OpResult
List::DBusList::enter_list_async(ID::List list_id, unsigned int line,
                                 unsigned short caller_id)
{
    log_assert(list_id.is_valid());

    LoggedLock::UniqueLock<LoggedLock::Mutex> lock(async_dbus_data_.lock_);

    if(async_dbus_data_.enter_list_query_ != nullptr)
    {
        async_dbus_data_.enter_list_query_->cancel_sync();
        async_dbus_data_.enter_list_query_.reset();
    }

    cancel_get_item_query();

    if(is_position_unchanged(list_id, line))
        return OpResult::SUCCEEDED;

    async_dbus_data_.enter_list_query_ =
        std::make_shared<QueryContextEnterList>(*this, caller_id,
                                                dbus_proxy_, list_id, line);

    if(async_dbus_data_.enter_list_query_ == nullptr)
    {
        msg_out_of_memory("asynchronous context (enter list)");
        return OpResult::FAILED;
    }

    if(!async_dbus_data_.enter_list_query_->run_async(
            std::bind(&List::DBusList::async_done_notification,
                      this, std::placeholders::_1)))
    {
        async_dbus_data_.enter_list_query_.reset();
        return OpResult::FAILED;
    }

    notify_watcher(OpEvent::ENTER_LIST, OpResult::STARTED,
                   async_dbus_data_.enter_list_query_, lock);

    return OpResult::STARTED;
}

bool List::DBusList::enter_list_async_wait()
{
    LoggedLock::UniqueLock<LoggedLock::Mutex> lock(async_dbus_data_.lock_);

    if(async_dbus_data_.enter_list_query_ == nullptr)
        return false;

    async_dbus_data_.query_done_.wait(lock,
        [this] () -> bool
        {
            return async_dbus_data_.enter_list_query_ == nullptr;
        });

    return true;
}

void List::DBusList::enter_list_async_handle_done(LoggedLock::UniqueLock<LoggedLock::Mutex> &lock)
{
    auto q = async_dbus_data_.enter_list_query_;
    const ID::List list_id(q->parameters_.list_id_.get_raw_id());
    DBus::AsyncResult async_result;

    try
    {
        q->synchronize(async_result);
    }
    catch(const List::DBusListException &e)
    {
        async_result = DBus::AsyncResult::FAILED;
        msg_error(0, LOG_ERR, "List error %u: %s", e.get(), e.what());
    }

    OpResult op_result;

    if(async_result == DBus::AsyncResult::DONE)
    {
        op_result = OpResult::SUCCEEDED;

        const auto &size(q->async_call_->get_result(async_result));

        number_of_items_ = size;
        window_.list_id_ = q->parameters_.list_id_;
        window_.first_item_line_ = q->parameters_.line_;
        window_.items_.clear();
    }
    else
    {
        op_result = (async_result == DBus::AsyncResult::CANCELED
                     ? OpResult::CANCELED
                     : OpResult::FAILED);

        msg_error(0, LOG_NOTICE,
                  "Failed obtaining size of list %u", list_id.get_raw_id());
    }

    async_dbus_data_.enter_list_query_.reset();
    async_dbus_data_.query_done_.notify_all();

    notify_watcher(OpEvent::ENTER_LIST, op_result, q, lock);
}

bool List::QueryContextEnterList::run_async(const DBus::AsyncResultAvailableFunction &result_available)
{
    log_assert(async_call_ == nullptr);

    async_call_ = new AsyncListNavCheckRange(
        proxy_,
        [] (GObject *source_object) { return TDBUS_LISTS_NAVIGATION(source_object); },
        std::bind(put_result,
                  std::placeholders::_1, std::placeholders::_2,
                  std::placeholders::_3, std::placeholders::_4,
                  std::placeholders::_5, parameters_.list_id_),
        result_available,
        [] (AsyncListNavCheckRange::PromiseReturnType &values) {},
        [] () { return true; });

    if(async_call_ == nullptr)
    {
        msg_out_of_memory("asynchronous D-Bus invocation");
        return false;
    }

    async_call_->invoke(tdbus_lists_navigation_call_check_range,
                        parameters_.list_id_.get_raw_id(), 0, 0);

    return true;
}

bool List::QueryContextEnterList::synchronize(DBus::AsyncResult &result)
{
    if(async_call_ == nullptr)
        result = DBus::AsyncResult::INITIALIZED;
    else
    {
        try
        {
            result = async_call_->wait_for_result();

            if(!DBus::AsyncCall_::cleanup_if_failed(async_call_))
                return true;

            async_call_ = nullptr;
        }
        catch(const List::DBusListException &e)
        {
            result = DBus::AsyncResult::FAILED;
            delete async_call_;
            async_call_ = nullptr;
            throw;
        }
    }

    return false;
}

void List::QueryContextEnterList::put_result(DBus::AsyncResult &async_ready,
                                             AsyncListNavCheckRange::PromiseType &promise,
                                             tdbuslistsNavigation *p,
                                             GAsyncResult *async_result,
                                             GError *&error, ID::List list_id)
    throw(List::DBusListException)
{
    guchar error_code = 0;
    guint first_item = 0;
    guint size = 0;

    async_ready =
        tdbus_lists_navigation_call_check_range_finish(p, &error_code,
                                                       &first_item, &size,
                                                       async_result, &error)
        ? DBus::AsyncResult::READY
        : DBus::AsyncResult::FAILED;

    if(async_ready == DBus::AsyncResult::FAILED)
        throw List::DBusListException(ListError::Code::INTERNAL, true);

    const ListError list_error(error_code);

    switch(list_error.get())
    {
      case ListError::Code::OK:
        log_assert(first_item == 0);
        promise.set_value(size);
        return;

      case ListError::Code::INTERNAL:
        break;

      case ListError::Code::INVALID_ID:
        msg_error(EINVAL, LOG_NOTICE,
                  "Invalid list ID %u", list_id.get_raw_id());
        break;

      case ListError::Code::INTERRUPTED:
      case ListError::Code::PHYSICAL_MEDIA_IO:
      case ListError::Code::NET_IO:
      case ListError::Code::PROTOCOL:
      case ListError::Code::AUTHENTICATION:
      case ListError::Code::INCONSISTENT:
      case ListError::Code::PERMISSION_DENIED:
      case ListError::Code::NOT_SUPPORTED:
        msg_error(0, LOG_NOTICE,
                  "Error while obtaining size of list ID %u: %s",
                  list_id.get_raw_id(), list_error.to_string());
        break;
    }

    if(list_error.get() == ListError::Code::INTERNAL)
        BUG("Unknown error code %u while obtaining size of list ID %u",
            error_code, list_id.get_raw_id());

    throw List::DBusListException(list_error);
}

void List::DBusList::cancel_get_item_query()
{
    if(async_dbus_data_.get_item_query_ != nullptr)
        async_dbus_data_.get_item_query_->cancel_sync();
}

bool List::DBusList::is_line_loading(unsigned int line) const
{
    if(async_dbus_data_.get_item_query_ != nullptr)
        return async_dbus_data_.get_item_query_->is_loading(line);
    else
        return false;
}

List::AsyncListIface::OpResult
List::DBusList::get_item_async(unsigned int line, const Item *&item,
                               unsigned short caller_id)
{
    /*
     * TODO: This object must be moved to ViewFileBrowser and must be
     *       retrievable from there as a regular #List::Item.
     * TODO: i18n
     */
    static const ViewFileBrowser::FileItem loading(_("Loading..."), 0U,
                                                   ListItemKind(ListItemKind::LOCKED),
                                                   PreloadedMetaData());

    log_assert(window_.list_id_.is_valid());

    LoggedLock::UniqueLock<LoggedLock::Mutex> lock(async_dbus_data_.lock_);

    item = nullptr;

    /* already entering a list, cannot get items in this situation */
    if(async_dbus_data_.enter_list_query_ != nullptr)
        return OpResult::CANCELED;

    if(line >= number_of_items_)
        return OpResult::FAILED;

    if(is_line_cached(line))
    {
        item = window_[line];
        return OpResult::SUCCEEDED;
    }

    if(is_line_loading(line))
    {
        item = &loading;
        return OpResult::STARTED;
    }

    cancel_get_item_query();

    CacheModifications cache_modifications;
    unsigned int fetch_head;
    unsigned int fetch_count;
    unsigned int cache_list_replace_index;

    if(!can_scroll_to_line(line, cache_modifications,
                           fetch_head, fetch_count, cache_list_replace_index))
    {
        fetch_head = line;
        fetch_count = number_of_prefetched_items_;
        cache_list_replace_index = 0;
        cache_modifications.set(line);
    }

    const uint32_t list_flags(list_contexts_[DBUS_LISTS_CONTEXT_GET(window_.list_id_.get_raw_id())].get_flags());

    async_dbus_data_.get_item_query_ =
        std::make_shared<QueryContextGetItem>(*this, caller_id,
                                            dbus_proxy_, window_.list_id_,
                                            fetch_head, fetch_count,
                                            (list_flags & List::ContextInfo::HAS_EXTERNAL_META_DATA) != 0,
                                            cache_list_replace_index);

    if(async_dbus_data_.get_item_query_ == nullptr)
    {
        msg_out_of_memory("asynchronous context (get item)");
        return OpResult::FAILED;
    }

    if(!async_dbus_data_.get_item_query_->run_async(
            std::bind(&List::DBusList::async_done_notification,
                    this, std::placeholders::_1)))
    {
        async_dbus_data_.get_item_query_.reset();
        return OpResult::FAILED;
    }

    apply_cache_modifications(cache_modifications);

    item = &loading;
    notify_watcher(OpEvent::GET_ITEM, OpResult::STARTED,
                   async_dbus_data_.get_item_query_, lock);

    return OpResult::STARTED;;
}

bool List::DBusList::get_item_async_wait(unsigned int line, const Item *&item)
{
    LoggedLock::UniqueLock<LoggedLock::Mutex> lock(async_dbus_data_.lock_);

    if(!is_line_loading(line))
        return false;

    async_dbus_data_.query_done_.wait(lock,
        [this, line] () -> bool
        {
            return !is_line_loading(line);
        });

    return true;
}

void List::DBusList::get_item_async_handle_done(LoggedLock::UniqueLock<LoggedLock::Mutex> &lock)
{
    auto q = async_dbus_data_.get_item_query_;
    DBus::AsyncResult async_result;

    try
    {
        q->synchronize(async_result);
    }
    catch(const List::DBusListException &e)
    {
        async_result = DBus::AsyncResult::FAILED;
        msg_error(0, LOG_ERR, "List error %u: %s", e.get(), e.what());
    }

    OpResult op_result;

    if(async_result == DBus::AsyncResult::DONE)
    {
        op_result = OpResult::SUCCEEDED;

        const auto &result(q->async_call_->get_result(async_result));

        if(q->parameters_.have_meta_data_)
            fill_cache_list_with_meta_data(window_.items_, new_item_fn_,
                                           q->parameters_.cache_list_replace_index_,
                                           !window_.items_.empty(),
                                           std::get<2>(result));
        else
            fill_cache_list_generic(window_.items_, new_item_fn_,
                                    q->parameters_.cache_list_replace_index_,
                                    !window_.items_.empty(),
                                    std::get<2>(result));
    }
    else
    {
        op_result = (async_result == DBus::AsyncResult::CANCELED
                     ? OpResult::CANCELED
                     : OpResult::FAILED);

        msg_error(0, LOG_NOTICE, "Failed obtaining line %u of list %u",
                  q->parameters_.line_, q->parameters_.list_id_.get_raw_id());
    }

    async_dbus_data_.get_item_query_.reset();
    async_dbus_data_.query_done_.notify_all();

    notify_watcher(OpEvent::GET_ITEM, op_result, q, lock);
}

void List::DBusList::apply_cache_modifications(const CacheModifications &cm)
{
    if(cm.is_filling_from_scratch_)
        window_.items_.clear();
    else if(cm.shift_distance_ > 0)
    {
        if(cm.is_shift_down_)
            window_.items_.shift_down(cm.shift_distance_);
        else
            window_.items_.shift_up(cm.shift_distance_);
    }

    window_.first_item_line_ = cm.new_first_line_;
}

bool List::QueryContextGetItem::run_async(const DBus::AsyncResultAvailableFunction &result_available)
{
    log_assert(async_call_ == nullptr);

    async_call_ = new AsyncListNavGetRange(
        proxy_,
        [] (GObject *source_object) { return TDBUS_LISTS_NAVIGATION(source_object); },
        std::bind(put_result,
                    std::placeholders::_1, std::placeholders::_2,
                    std::placeholders::_3, std::placeholders::_4,
                    std::placeholders::_5,
                    parameters_.list_id_, parameters_.have_meta_data_),
        result_available,
        [] (AsyncListNavGetRange::PromiseReturnType &values)
        {
            g_variant_unref(std::get<2>(values));
        },
        [] () { return true; });

    if(async_call_ == nullptr)
    {
        msg_out_of_memory("asynchronous D-Bus invocation");
        return false;
    }

    msg_info("Fetch %u lines of list %u, starting at %u",
             parameters_.count_, parameters_.list_id_.get_raw_id(),
             parameters_.line_);

    async_call_->invoke(parameters_.have_meta_data_
                        ? tdbus_lists_navigation_call_get_range_with_meta_data
                        : tdbus_lists_navigation_call_get_range,
                        parameters_.list_id_.get_raw_id(),
                        parameters_.line_, parameters_.count_);

    return true;
}

bool List::QueryContextGetItem::synchronize(DBus::AsyncResult &result)
{
    if(async_call_ == nullptr)
        result = DBus::AsyncResult::INITIALIZED;
    else
    {
        try
        {
            result = async_call_->wait_for_result();

            if(!DBus::AsyncCall_::cleanup_if_failed(async_call_))
                return true;

            async_call_ = nullptr;
        }
        catch(const List::DBusListException &e)
        {
            result = DBus::AsyncResult::FAILED;
            delete async_call_;
            async_call_ = nullptr;
            throw;
        }
    }

    return false;
}

void List::QueryContextGetItem::put_result(DBus::AsyncResult &async_ready,
                                           AsyncListNavGetRange::PromiseType &promise,
                                           tdbuslistsNavigation *p,
                                           GAsyncResult *async_result,
                                           GError *&error, ID::List list_id,
                                           bool have_meta_data)
    throw(List::DBusListException)
{
    guchar error_code;
    guint first_item;
    GVariant *out_list = nullptr;

    const bool dbus_ok = have_meta_data
        ? tdbus_lists_navigation_call_get_range_with_meta_data_finish(
              p, &error_code, &first_item, &out_list, async_result, &error)
        : tdbus_lists_navigation_call_get_range_finish(
              p, &error_code, &first_item, &out_list, async_result, &error);

    async_ready = dbus_ok ? DBus::AsyncResult::READY : DBus::AsyncResult::FAILED;

    if(async_ready == DBus::AsyncResult::FAILED)
        throw List::DBusListException(ListError::Code::INTERNAL, true);

    const ListError list_error(error_code);

    if(error_code != ListError::Code::OK)
    {
        /* method error, stop trying */
        msg_error(0, LOG_INFO, "Error reading list %u: %s",
                  list_id.get_raw_id(), list_error.to_string());
        g_variant_unref(out_list);

        throw List::DBusListException(list_error);
    }

    log_assert(g_variant_type_is_array(g_variant_get_type(out_list)));

    promise.set_value(std::make_tuple(error_code, first_item, out_list));
}

void List::DBusList::async_done_notification(DBus::AsyncCall_ &async_call)
{
    LoggedLock::UniqueLock<LoggedLock::Mutex> lock(async_dbus_data_.lock_);

    if(async_dbus_data_.enter_list_query_ != nullptr)
    {
        log_assert(&async_call == async_dbus_data_.enter_list_query_->async_call_);
        enter_list_async_handle_done(lock);
    }
    else if(async_dbus_data_.get_item_query_ != nullptr)
    {
        log_assert(&async_call == async_dbus_data_.get_item_query_->async_call_);
        get_item_async_handle_done(lock);
    }
    else
        BUG("Unexpected async done notification");
}
