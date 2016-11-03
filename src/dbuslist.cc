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
#include "view_filebrowser_fileitem.hh"
#include "i18n.h"
#include "messages.h"
#include "logged_lock.hh"

constexpr const char *ListError::names_[];

void List::DBusList::clone_state(const List::DBusList &src)
    throw(List::DBusListException)
{
    async_dbus_data_.cancel_all();

    number_of_items_ = src.number_of_items_;
    window_stash_is_in_use_ = false;
    window_.clone(src.window_);
}

unsigned int List::DBusList::get_number_of_items() const
{
    return number_of_items_;
}

bool List::DBusList::empty() const
{
    return number_of_items_ == 0;
}

static unsigned int query_list_size_sync(tdbuslistsNavigation *proxy,
                                         ID::List list_id)
    throw(List::DBusListException)
{
    guchar error_code;
    guint first_item;
    guint size;

    if(!tdbus_lists_navigation_call_check_range_sync(proxy,
                                                     list_id.get_raw_id(), 0, 0,
                                                     &error_code, &first_item,
                                                     &size, NULL, NULL))
    {
        msg_error(0, LOG_NOTICE,
                  "Failed obtaining size of list %u (sync)", list_id.get_raw_id());

        throw List::DBusListException(ListError::Code::INTERNAL, true);
    }

    const ListError error(error_code);

    switch(error.get())
    {
      case ListError::Code::OK:
        log_assert(first_item == 0);
        return size;

      case ListError::Code::INTERNAL:
        break;

      case ListError::Code::INVALID_ID:
        msg_error(EINVAL, LOG_NOTICE, "Invalid list ID %u, cannot query size",
                  list_id.get_raw_id());
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
                  list_id.get_raw_id(), error.to_string());
        break;
    }

    if(error.get() == ListError::Code::INTERNAL)
        BUG("Unknown error code %u while obtaining size of list ID %u",
            error_code, list_id.get_raw_id());

    throw List::DBusListException(error);
}

void List::DBusList::enter_list(ID::List list_id, unsigned int line)
    throw(List::DBusListException)
{
    log_assert(list_id.is_valid());

    if(list_id != window_.list_id_)
    {
        std::lock_guard<LoggedLock::RecMutex> lock((const_cast<List::DBusList *>(this)->async_dbus_data_).lock_);
        const_cast<List::DBusList *>(this)->cancel_enter_list_query();
    }

    if(list_id != window_.list_id_)
        number_of_items_ = query_list_size_sync(dbus_proxy_, list_id);
    else if(line == window_.first_item_line_)
        return;

    window_.clear_for_line(list_id, line);
}

static bool fetch_window_sync(tdbuslistsNavigation *proxy,
                              const List::ContextMap &list_contexts,
                              ID::List list_id, unsigned int line,
                              unsigned int count, GVariant **out_list)
    throw(List::DBusListException)
{
    msg_info("Fetch %u lines of list %u: starting at %u (sync)",
             count, list_id.get_raw_id(), line);

    guchar error_code;
    guint first_item;
    gboolean success;
    bool have_meta_data;

    const uint32_t list_flags(list_contexts[DBUS_LISTS_CONTEXT_GET(list_id.get_raw_id())].get_flags());

    if((list_flags & List::ContextInfo::HAS_EXTERNAL_META_DATA) != 0)
    {
        success =
            tdbus_lists_navigation_call_get_range_with_meta_data_sync(
                proxy, list_id.get_raw_id(), line, count,
                &error_code, &first_item, out_list, NULL, NULL);
        have_meta_data = true;
    }
    else
    {
        success =
            tdbus_lists_navigation_call_get_range_sync(
                proxy, list_id.get_raw_id(), line, count,
                &error_code, &first_item, out_list, NULL, NULL);
        have_meta_data = false;
    }

    if(!success)
    {
        msg_error(0, LOG_NOTICE,
                  "Failed obtaining contents of list %u", list_id.get_raw_id());

        throw List::DBusListException(ListError::Code::INTERNAL, true);
    }

    const ListError error(error_code);

    if(error_code != ListError::Code::OK)
    {
        /* method error, stop trying */
        msg_error(0, LOG_INFO, "Error reading list %u: %s",
                  list_id.get_raw_id(), error.to_string());

        if(*out_list != nullptr)
            g_variant_unref(*out_list);

        throw List::DBusListException(error);
    }

    log_assert(g_variant_type_is_array(g_variant_get_type(*out_list)));

    return have_meta_data;
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
 * \param prefetch_hint
 *    Number of visible lines that follow the current line. This parameter is
 *    required for hinting the internal list range query how many lines it
 *    should try to fetch from the list broker.
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
                                        unsigned int prefetch_hint,
                                        CacheModifications &cm,
                                        unsigned int &fetch_head,
                                        unsigned int &fetch_count,
                                        unsigned int &cache_list_replace_index) const
{
    if(window_.items_.get_number_of_items() == 0)
        return false;

    if(prefetch_hint > 1)
        line += prefetch_hint - 1;

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

    LoggedLock::UniqueLock<LoggedLock::RecMutex> lock(async_dbus_data_.lock_);

    cancel_enter_list_query();
    cancel_get_item_query();

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
                   async_dbus_data_.enter_list_query_);

    return OpResult::STARTED;
}

void List::DBusList::enter_list_async_handle_done()
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
        window_.clear_for_line(q->parameters_.list_id_, q->parameters_.line_);
    }
    else
    {
        op_result = (async_result == DBus::AsyncResult::CANCELED
                     ? OpResult::CANCELED
                     : OpResult::FAILED);

        msg_error(0, LOG_NOTICE,
                  "Failed obtaining size of list %u (async)", list_id.get_raw_id());
    }

    async_dbus_data_.enter_list_query_.reset();
    async_dbus_data_.query_done_.notify_all();

    notify_watcher(OpEvent::ENTER_LIST, op_result, q);
}

void List::DBusList::list_invalidate(ID::List list_id, ID::List replacement_id)
{
    LoggedLock::UniqueLock<LoggedLock::RecMutex> lock(async_dbus_data_.lock_);

    if(window_.list_id_ == list_id)
    {
        if(replacement_id.is_valid())
            window_.list_id_ = replacement_id;
        else
            window_.clear_for_line(ID::List(), 0);
    }

    QueryContextEnterList::restart_if_necessary(async_dbus_data_.enter_list_query_,
                                                list_id, replacement_id);
    QueryContextGetItem::restart_if_necessary(async_dbus_data_.get_item_query_,
                                              list_id, replacement_id);
}

bool List::QueryContextEnterList::run_async(DBus::AsyncResultAvailableFunction &&result_available)
{
    log_assert(async_call_ == nullptr);

    async_call_ = std::make_shared<AsyncListNavCheckRange>(
        proxy_,
        [] (GObject *source_object) { return TDBUS_LISTS_NAVIGATION(source_object); },
        std::bind(put_result,
                  std::placeholders::_1, std::placeholders::_2,
                  std::placeholders::_3, std::placeholders::_4,
                  std::placeholders::_5, parameters_.list_id_),
        std::move(result_available),
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

            if(async_call_ == nullptr)
                return DBus::AsyncCall_::is_success(result);

            if(async_call_->success())
                return true;

            async_call_.reset();
        }
        catch(const List::DBusListException &e)
        {
            result = DBus::AsyncResult::FAILED;
            async_call_.reset();
            throw;
        }
    }

    return false;
}

template <typename QueryContextType>
static List::AsyncListIface::OpResult
cancel_and_restart(std::shared_ptr<QueryContextType> &ctx,
                   const std::shared_ptr<QueryContextType> &ctx_local_ref,
                   const ID::List invalidated_list_id,
                   const ID::List replacement_id,
                   const ID::List list_id_in_invocation,
                   std::function<std::shared_ptr<QueryContextType>(void)> make_query)
{
    const auto async_local_ref(ctx->async_call_);

    if(async_local_ref == nullptr)
        return List::AsyncListIface::OpResult::SUCCEEDED;

    if(list_id_in_invocation != invalidated_list_id)
        return List::AsyncListIface::OpResult::SUCCEEDED;

    if(!replacement_id.is_valid())
    {
        ctx->cancel(false);
        return List::AsyncListIface::OpResult::CANCELED;
    }

    ctx->cancel(true);

    ctx = make_query();

    if(ctx == nullptr)
        return List::AsyncListIface::OpResult::FAILED;

    if(!ctx->run_async(std::move(async_local_ref->get_result_available_fn())))
    {
        ctx.reset();
        return List::AsyncListIface::OpResult::FAILED;
    }

    return List::AsyncListIface::OpResult::STARTED;
}

List::AsyncListIface::OpResult
List::QueryContextEnterList::restart_if_necessary(std::shared_ptr<QueryContextEnterList> &ctx,
                                                  ID::List invalidated_list_id,
                                                  ID::List replacement_id)
{
    const std::shared_ptr<QueryContextEnterList> ctx_local_ref(ctx);

    if(ctx == nullptr)
        return List::AsyncListIface::OpResult::SUCCEEDED;

    const std::function<std::shared_ptr<QueryContextEnterList>(void)>
    make_query_fn([&ctx_local_ref, replacement_id] ()
        {
            auto new_ctx =
                std::make_shared<QueryContextEnterList>(*ctx_local_ref, replacement_id,
                                                        ctx_local_ref->parameters_.line_);

            if(new_ctx == nullptr)
                msg_out_of_memory("asynchronous context (restart enter list)");

            return new_ctx;
        });

    return cancel_and_restart(ctx, ctx_local_ref,
                              invalidated_list_id, replacement_id,
                              ctx->parameters_.list_id_, make_query_fn);
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
                  "Invalid list ID %u, cannot put async result",
                  list_id.get_raw_id());
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
                  "Error while putting async result for list ID %u: %s",
                  list_id.get_raw_id(), list_error.to_string());
        break;
    }

    if(list_error.get() == ListError::Code::INTERNAL)
        BUG("Unknown error code %u while putting async result for list ID %u",
            error_code, list_id.get_raw_id());

    throw List::DBusListException(list_error);
}

List::CacheSegmentState
List::DBusList::get_cache_segment_state(const CacheSegment &segment,
                                        unsigned int &size_of_cached_segment,
                                        unsigned int &size_of_loading_segment) const
{
    CacheSegmentState cached_state = CacheSegmentState::EMPTY;

    switch(segment.intersection(window_.valid_segment_, size_of_cached_segment))
    {
      case CacheSegment::DISJOINT:
        break;

      case CacheSegment::EQUAL:
      case CacheSegment::INCLUDED_IN_OTHER:
        cached_state = CacheSegmentState::CACHED;
        break;

      case CacheSegment::TOP_REMAINS:
        cached_state = CacheSegmentState::CACHED_TOP_EMPTY_BOTTOM;
        break;

      case CacheSegment::BOTTOM_REMAINS:
        cached_state = CacheSegmentState::CACHED_BOTTOM_EMPTY_TOP;
        break;

      case CacheSegment::CENTER_REMAINS:
        cached_state = CacheSegmentState::CACHED_CENTER;
        break;
    }

    if(size_of_cached_segment == 0)
        cached_state = CacheSegmentState::EMPTY;

    if(async_dbus_data_.get_item_query_ == nullptr)
    {
        size_of_loading_segment = 0;
        return cached_state;
    }

    auto loading_state =
        async_dbus_data_.get_item_query_->get_cache_segment_state(segment, size_of_loading_segment);

    switch(loading_state)
    {
      case CacheSegmentState::EMPTY:
        /* loading something entirely different */
        break;

      case CacheSegmentState::LOADING:
        log_assert(cached_state == CacheSegmentState::EMPTY);
        cached_state = loading_state;
        break;

      case CacheSegmentState::LOADING_TOP_EMPTY_BOTTOM:
        log_assert(cached_state == CacheSegmentState::EMPTY ||
                   cached_state == CacheSegmentState::CACHED_BOTTOM_EMPTY_TOP ||
                   cached_state == CacheSegmentState::CACHED_CENTER);

        if(cached_state == CacheSegmentState::EMPTY)
            cached_state = loading_state;
        else if(cached_state == CacheSegmentState::CACHED_BOTTOM_EMPTY_TOP)
            cached_state = CacheSegmentState::CACHED_BOTTOM_LOADING_TOP;

        break;

      case CacheSegmentState::LOADING_BOTTOM_EMPTY_TOP:
        log_assert(cached_state == CacheSegmentState::EMPTY ||
                   cached_state == CacheSegmentState::CACHED_TOP_EMPTY_BOTTOM ||
                   cached_state == CacheSegmentState::CACHED_CENTER);

        if(cached_state == CacheSegmentState::EMPTY)
            cached_state = loading_state;
        else if(cached_state == CacheSegmentState::CACHED_TOP_EMPTY_BOTTOM)
            cached_state = CacheSegmentState::CACHED_TOP_LOADING_BOTTOM;

        break;

      case CacheSegmentState::LOADING_CENTER:
        if(cached_state == CacheSegmentState::EMPTY)
            cached_state = loading_state;
        else if(cached_state == CacheSegmentState::CACHED_TOP_EMPTY_BOTTOM)
            cached_state = CacheSegmentState::CACHED_TOP_LOADING_CENTER_EMPTY_BOTTOM;
        else if(cached_state == CacheSegmentState::CACHED_BOTTOM_EMPTY_TOP)
            cached_state = CacheSegmentState::CACHED_BOTTOM_LOADING_CENTER_EMPTY_TOP;
        else if(cached_state != CacheSegmentState::CACHED_CENTER)
            BUG("Unexpected cached state %u", static_cast<unsigned int>(cached_state));

        break;

      case CacheSegmentState::CACHED:
      case CacheSegmentState::CACHED_TOP_LOADING_BOTTOM:
      case CacheSegmentState::CACHED_BOTTOM_LOADING_TOP:
      case CacheSegmentState::CACHED_TOP_EMPTY_BOTTOM:
      case CacheSegmentState::CACHED_BOTTOM_EMPTY_TOP:
      case CacheSegmentState::CACHED_TOP_LOADING_CENTER_EMPTY_BOTTOM:
      case CacheSegmentState::CACHED_BOTTOM_LOADING_CENTER_EMPTY_TOP:
      case CacheSegmentState::CACHED_CENTER:
        BUG("Unexpected loading state %u", static_cast<unsigned int>(loading_state));
        break;
    }

    return cached_state;
}

const List::Item *List::DBusList::get_item(unsigned int line) const
    throw(List::DBusListException)
{
    log_assert(window_.list_id_.is_valid());

    if(line >= number_of_items_)
        return nullptr;

    if(is_line_cached(line))
        return window_[line];

    CacheModifications cache_modifications;
    unsigned int fetch_head;
    unsigned int fetch_count;
    unsigned int cache_list_replace_index;

    if(!can_scroll_to_line(line, number_of_prefetched_items_, cache_modifications,
                           fetch_head, fetch_count, cache_list_replace_index))
    {
        fetch_head = line;
        fetch_count = (line + number_of_prefetched_items_ <= get_number_of_items()
                       ? number_of_prefetched_items_
                       : get_number_of_items() - line);
        cache_list_replace_index = 0;
        cache_modifications.set(line);
    }

    GVariant *out_list;

    const bool have_meta_data =
        fetch_window_sync(dbus_proxy_, list_contexts_, window_.list_id_,
                          fetch_head, fetch_count, &out_list);

    log_assert(g_variant_n_children(out_list) == fetch_count);

    /* Shall I burn in hell for this? */
    auto *nonconst_this = const_cast<List::DBusList *>(this);

    nonconst_this->apply_cache_modifications(cache_modifications);

    if(have_meta_data)
        fill_cache_list_with_meta_data(nonconst_this->window_.items_,
                                       new_item_fn_, cache_list_replace_index,
                                       !window_.items_.empty(), out_list);
    else
        fill_cache_list_generic(nonconst_this->window_.items_,
                                new_item_fn_, cache_list_replace_index,
                                !window_.items_.empty(), out_list);

    g_variant_unref(out_list);

    nonconst_this->window_.valid_segment_.line_ = window_.first_item_line_;
    nonconst_this->window_.valid_segment_.count_ = window_.items_.get_number_of_items();

    return window_[line];
}

List::AsyncListIface::OpResult
List::DBusList::load_segment_in_background(const CacheSegment &prefetch_segment,
                                           int keep_cache_entries,
                                           unsigned int current_number_of_loading_items,
                                           unsigned short caller_id)

{
    cancel_get_item_query();

    CacheModifications cm;
    unsigned int fetch_head;
    unsigned int fetch_count;
    unsigned int cache_list_replace_index;

    if(keep_cache_entries > 0)
    {
        /* keep cached bottom elements and move them to top, load bottom */
        fetch_head = prefetch_segment.line_ + keep_cache_entries;
        fetch_count = prefetch_segment.count_ - keep_cache_entries;
        cache_list_replace_index = window_.items_.get_number_of_items() - fetch_count;
        cm.set(prefetch_segment.line_, false,
               fetch_count - current_number_of_loading_items);
    }
    else if(keep_cache_entries < 0)
    {
        /* keep cached top elements and move them to bottom, load top */
        keep_cache_entries = -keep_cache_entries;
        fetch_head = prefetch_segment.line_;
        fetch_count = prefetch_segment.count_ - keep_cache_entries;
        cache_list_replace_index = 0;
        cm.set(prefetch_segment.line_, true,
               fetch_count - current_number_of_loading_items);
    }
    else
    {
        fetch_head = prefetch_segment.line_;
        fetch_count = prefetch_segment.count_;
        cache_list_replace_index = 0;
        cm.set(prefetch_segment.line_);
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

    apply_cache_modifications(cm);

    notify_watcher(OpEvent::GET_ITEM, OpResult::STARTED,
                   async_dbus_data_.get_item_query_);

    return OpResult::STARTED;
}

List::AsyncListIface::OpResult
List::DBusList::get_item_async_set_hint(unsigned int line, unsigned int count,
                                        unsigned short caller_id)
{
    if(!window_.list_id_.is_valid())
    {
        BUG("Cannot hint async operation for invalid list");
        return OpResult::FAILED;
    }

    LoggedLock::UniqueLock<LoggedLock::RecMutex> lock(async_dbus_data_.lock_);

    /* already entering a list, cannot get items in this situation */
    if(async_dbus_data_.enter_list_query_ != nullptr)
        return OpResult::CANCELED;

    if(line >= number_of_items_)
        return OpResult::FAILED;

    if(count == 0 || count > number_of_prefetched_items_)
    {
        if(count == 0)
            BUG("Hint async operation with no items");
        else
            BUG("Hint async operation with more items than prefetched");

        return OpResult::FAILED;
    }

    unsigned int size_of_cached_overlap;
    unsigned int size_of_loading_segment;
    const CacheSegment prefetch_segment(line, count);
    const CacheSegmentState segment_state =
        get_cache_segment_state(prefetch_segment, size_of_cached_overlap,
                                size_of_loading_segment);

    OpResult retval = OpResult::FAILED;

    switch(segment_state)
    {
      case CacheSegmentState::EMPTY:
      case CacheSegmentState::CACHED_CENTER:
      case CacheSegmentState::LOADING_CENTER:
      case CacheSegmentState::LOADING_TOP_EMPTY_BOTTOM:
      case CacheSegmentState::LOADING_BOTTOM_EMPTY_TOP:
        /* need to wipe out everything and restart loading everything */
        retval = load_segment_in_background(prefetch_segment, 0, 0,
                                            caller_id);
        break;

      case CacheSegmentState::CACHED:
        retval = OpResult::SUCCEEDED;
        break;

      case CacheSegmentState::LOADING:
      case CacheSegmentState::CACHED_TOP_LOADING_BOTTOM:
      case CacheSegmentState::CACHED_BOTTOM_LOADING_TOP:
        /* everything is done that could be done */
        retval = OpResult::STARTED;
        break;

      case CacheSegmentState::CACHED_TOP_EMPTY_BOTTOM:
      case CacheSegmentState::CACHED_TOP_LOADING_CENTER_EMPTY_BOTTOM:
        /* need to start loading bottom half, any other load operation can be
         * canceled and ignored */
        retval = load_segment_in_background(prefetch_segment, size_of_cached_overlap,
                                            size_of_loading_segment,
                                            caller_id);
        break;

      case CacheSegmentState::CACHED_BOTTOM_EMPTY_TOP:
      case CacheSegmentState::CACHED_BOTTOM_LOADING_CENTER_EMPTY_TOP:
        /* need to start loading top half, any other load operation can be
         * canceled and ignored */
        retval = load_segment_in_background(prefetch_segment, -size_of_cached_overlap,
                                            size_of_loading_segment,
                                            caller_id);
        break;
    }

    return retval;
}

List::AsyncListIface::OpResult
List::DBusList::get_item_async(unsigned int line, const Item *&item)
{
    /*
     * TODO: This object must be moved to ViewFileBrowser and must be
     *       retrievable from there as a regular #List::Item.
     * TODO: i18n
     */
    static const ViewFileBrowser::FileItem loading(_("Loading..."), 0U,
                                                   ListItemKind(ListItemKind::LOCKED),
                                                   MetaData::PreloadedSet());

    if(!window_.list_id_.is_valid())
        return OpResult::FAILED;

    LoggedLock::UniqueLock<LoggedLock::RecMutex> lock(async_dbus_data_.lock_);

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

    return OpResult::FAILED;
}

void List::DBusList::cancel_async()
{
    LoggedLock::UniqueLock<LoggedLock::RecMutex> lock(async_dbus_data_.lock_);
    async_dbus_data_.cancel_all();
}

void List::DBusList::get_item_async_handle_done()
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

        window_.valid_segment_.line_ = window_.first_item_line_;
        window_.valid_segment_.count_ = window_.items_.get_number_of_items();
    }
    else
    {
        op_result = (async_result == DBus::AsyncResult::CANCELED
                     ? OpResult::CANCELED
                     : OpResult::FAILED);

        msg_error(0, LOG_NOTICE, "Failed obtaining line %u of list %u",
                  q->parameters_.loading_segment_.line_,
                  q->parameters_.list_id_.get_raw_id());
    }

    async_dbus_data_.get_item_query_.reset();
    async_dbus_data_.query_done_.notify_all();

    notify_watcher(OpEvent::GET_ITEM, op_result, q);
}

void List::DBusList::apply_cache_modifications(const CacheModifications &cm)
{
    if(cm.is_filling_from_scratch_)
    {
        window_.items_.clear();
        window_.valid_segment_.line_ = 0;
        window_.valid_segment_.count_ = 0;
    }
    else if(cm.shift_distance_ > 0)
    {
        if(cm.is_shift_down_)
        {
            if(window_.valid_segment_.count_ > 0)
            {
                const unsigned int distance_vstop_to_winbottom =
                    window_.first_item_line_ + number_of_prefetched_items_ -
                    window_.valid_segment_.line_;

                if(cm.shift_distance_ >= distance_vstop_to_winbottom)
                    window_.valid_segment_.count_ = 0;
                else
                {
                    const unsigned int distance_vsbottom_to_winbottom =
                        distance_vstop_to_winbottom - window_.valid_segment_.count_;

                    if(cm.shift_distance_ > distance_vsbottom_to_winbottom)
                        window_.valid_segment_.count_ -=
                            cm.shift_distance_ - distance_vsbottom_to_winbottom;
                }
            }

            window_.items_.shift_down(cm.shift_distance_);
        }
        else
        {
            if(window_.valid_segment_.count_ > 0)
            {
                const unsigned int distance_vsbottom_to_wintop =
                    window_.valid_segment_.line_ + window_.valid_segment_.count_ -
                    window_.first_item_line_;

                if(cm.shift_distance_ >= distance_vsbottom_to_wintop)
                    window_.valid_segment_.count_ = 0;
                else
                {
                    const unsigned int distance_vstop_to_wintop =
                        distance_vsbottom_to_wintop - window_.valid_segment_.count_;

                    if(cm.shift_distance_ > distance_vstop_to_wintop)
                    {
                        window_.valid_segment_.line_ +=
                            cm.shift_distance_ - distance_vstop_to_wintop;
                        window_.valid_segment_.count_ -=
                            cm.shift_distance_ - distance_vstop_to_wintop;
                    }
                }
            }

            window_.items_.shift_up(cm.shift_distance_);
        }
    }

    window_.first_item_line_ = cm.new_first_line_;
}

bool List::QueryContextGetItem::run_async(DBus::AsyncResultAvailableFunction &&result_available)
{
    log_assert(async_call_ == nullptr);

    async_call_ = std::make_shared<AsyncListNavGetRange>(
        proxy_,
        [] (GObject *source_object) { return TDBUS_LISTS_NAVIGATION(source_object); },
        std::bind(put_result,
                  std::placeholders::_1, std::placeholders::_2,
                  std::placeholders::_3, std::placeholders::_4,
                  std::placeholders::_5,
                  parameters_.list_id_, parameters_.have_meta_data_),
        std::move(result_available),
        [] (AsyncListNavGetRange::PromiseReturnType &values)
        {
            if(std::get<2>(values) != nullptr)
                g_variant_unref(std::get<2>(values));
        },
        [] () { return true; });

    if(async_call_ == nullptr)
    {
        msg_out_of_memory("asynchronous D-Bus invocation");
        return false;
    }

    async_call_->invoke(parameters_.have_meta_data_
                        ? tdbus_lists_navigation_call_get_range_with_meta_data
                        : tdbus_lists_navigation_call_get_range,
                        parameters_.list_id_.get_raw_id(),
                        parameters_.loading_segment_.line_,
                        parameters_.loading_segment_.count_);

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

            if(async_call_ == nullptr)
                return DBus::AsyncCall_::is_success(result);

            if(async_call_->success())
                return true;

            async_call_.reset();
        }
        catch(const List::DBusListException &e)
        {
            result = DBus::AsyncResult::FAILED;
            async_call_.reset();
            throw;
        }
    }

    return false;
}

List::AsyncListIface::OpResult
List::QueryContextGetItem::restart_if_necessary(std::shared_ptr<QueryContextGetItem> &ctx,
                                                ID::List invalidated_list_id,
                                                ID::List replacement_id)
{
    const std::shared_ptr<QueryContextGetItem> ctx_local_ref(ctx);

    if(ctx == nullptr)
        return List::AsyncListIface::OpResult::SUCCEEDED;

    const std::function<std::shared_ptr<QueryContextGetItem>(void)>
    make_query_fn([&ctx_local_ref, replacement_id] ()
        {
            auto new_ctx =
                std::make_shared<QueryContextGetItem>(*ctx_local_ref,
                                                      replacement_id,
                                                      ctx_local_ref->parameters_.loading_segment_.line_,
                                                      ctx_local_ref->parameters_.loading_segment_.count_,
                                                      ctx_local_ref->parameters_.have_meta_data_,
                                                      ctx_local_ref->parameters_.cache_list_replace_index_);

            if(new_ctx == nullptr)
                msg_out_of_memory("asynchronous context (restart get item)");

            return new_ctx;
        });

    return cancel_and_restart(ctx, ctx_local_ref,
                              invalidated_list_id, replacement_id,
                              ctx->parameters_.list_id_, make_query_fn);
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

        if(out_list != nullptr)
            g_variant_unref(out_list);

        throw List::DBusListException(list_error);
    }

    log_assert(g_variant_type_is_array(g_variant_get_type(out_list)));

    promise.set_value(std::make_tuple(error_code, first_item, out_list));
}

void List::DBusList::async_done_notification(DBus::AsyncCall_ &async_call)
{
    LoggedLock::UniqueLock<LoggedLock::RecMutex> lock(async_dbus_data_.lock_);

    if(async_dbus_data_.enter_list_query_ != nullptr)
    {
        if(&async_call == async_dbus_data_.enter_list_query_->async_call_.get())
            enter_list_async_handle_done();
    }
    else if(async_dbus_data_.get_item_query_ != nullptr)
    {
        if(&async_call == async_dbus_data_.get_item_query_->async_call_.get())
            get_item_async_handle_done();
    }
    else
    {
        if(dynamic_cast<QueryContextEnterList::AsyncListNavCheckRange *>(&async_call) != nullptr)
            BUG("Unexpected async enter-line done notification");
        else if(dynamic_cast<QueryContextGetItem::AsyncListNavGetRange *>(&async_call) != nullptr)
            BUG("Unexpected async get-item done notification");
        else
            BUG("Unexpected UNKNOWN async done notification");
    }
}
