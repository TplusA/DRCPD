/*
 * Copyright (C) 2015--2020  T+A elektroakustik GmbH & Co. KG
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

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "dbuslist.hh"
#include "de_tahifi_lists_context.h"
#include "view_filebrowser_fileitem.hh"

#include <algorithm>
#include <sstream>

constexpr const char *ListError::names_[];

unsigned int List::DBusList::get_number_of_items() const
{
    return number_of_items_;
}

bool List::DBusList::empty() const
{
    return number_of_items_ == 0;
}

static unsigned int query_list_size_sync(tdbuslistsNavigation *proxy,
                                         ID::List list_id,
                                         const std::string &list_iface_name)
{
    guchar error_code;
    guint first_item;
    guint size;
    GErrorWrapper gerror;

    tdbus_lists_navigation_call_check_range_sync(proxy,
                                                 list_id.get_raw_id(), 0, 0,
                                                 &error_code, &first_item,
                                                 &size, nullptr, gerror.await());

    if(gerror.log_failure("Check range"))
    {
        msg_error(0, LOG_NOTICE,
                  "Failed obtaining size of list %u (sync) [%s]",
                  list_id.get_raw_id(), list_iface_name.c_str());

        throw List::DBusListException(gerror);
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
        msg_error(EINVAL, LOG_NOTICE, "Invalid list ID %u, cannot query size [%s]",
                  list_id.get_raw_id(), list_iface_name.c_str());
        break;

      case ListError::Code::INVALID_URI:
        msg_error(EINVAL, LOG_NOTICE, "Invalid URI for list ID %u, cannot query size [%s]",
                  list_id.get_raw_id(), list_iface_name.c_str());
        break;

      case ListError::Code::INVALID_STRBO_URL:
        msg_error(EINVAL, LOG_NOTICE, "Invalid StrBo URL for list ID %u, cannot query size [%s]",
                  list_id.get_raw_id(), list_iface_name.c_str());
        break;

      case ListError::Code::NOT_FOUND:
        msg_error(EINVAL, LOG_NOTICE,
                  "Failed to locate content for list ID %u, cannot query size [%s]",
                  list_id.get_raw_id(), list_iface_name.c_str());
        break;

      case ListError::Code::BUSY_500:
      case ListError::Code::BUSY_1000:
      case ListError::Code::BUSY_1500:
      case ListError::Code::BUSY_3000:
      case ListError::Code::BUSY_5000:
      case ListError::Code::BUSY:
        BUG("List broker is busy, should retry getting list size later");

        /* fall-through */

      case ListError::Code::INTERRUPTED:
      case ListError::Code::PHYSICAL_MEDIA_IO:
      case ListError::Code::NET_IO:
      case ListError::Code::PROTOCOL:
      case ListError::Code::AUTHENTICATION:
      case ListError::Code::INCONSISTENT:
      case ListError::Code::PERMISSION_DENIED:
      case ListError::Code::NOT_SUPPORTED:
      case ListError::Code::OUT_OF_RANGE:
      case ListError::Code::EMPTY:
      case ListError::Code::OVERFLOWN:
      case ListError::Code::UNDERFLOWN:
      case ListError::Code::INVALID_STREAM_URL:
        msg_error(0, LOG_NOTICE,
                  "Error while obtaining size of list ID %u: %s [%s]",
                  list_id.get_raw_id(), error.to_string(), list_iface_name.c_str());
        break;
    }

    if(error.get() == ListError::Code::INTERNAL)
        BUG("Unknown error code %u while obtaining size of list ID %u [%s]",
            error_code, list_id.get_raw_id(), list_iface_name.c_str());

    throw List::DBusListException(error);
}

void List::DBusList::enter_list(ID::List list_id)
{
    LOGGED_LOCK_CONTEXT_HINT;
    std::lock_guard<LoggedLock::RecMutex> lk(lock_);

    log_assert(list_id.is_valid());

    if(list_id == list_id_)
        return;

    enter_list_data_.cancel_all(viewports_and_fetchers_);
    number_of_items_ = query_list_size_sync(dbus_proxy_, list_id,
                                            list_iface_name_);
}

static DBusRNF::GetRangeResult
fetch_window_sync(DBusRNF::CookieManagerIface &cm, tdbuslistsNavigation *proxy,
                  const std::string &list_iface_name,
                  const List::ContextMap &list_contexts,
                  ID::List list_id, List::Segment &&window)
{
    msg_info("Fetch %u lines of list %u: starting at %u (sync) [%s]",
             window.size(), list_id.get_raw_id(),
             window.line(), list_iface_name.c_str());

    const uint32_t list_flags(list_contexts[DBUS_LISTS_CONTEXT_GET(list_id.get_raw_id())].get_flags());

    if((list_flags & List::ContextInfo::HAS_EXTERNAL_META_DATA) != 0)
    {
        auto call = std::make_shared<DBusRNF::GetRangeWithMetaDataCall>(
                            cm, proxy, list_iface_name, list_id,
                            std::move(window), nullptr, nullptr);

        call->request();
        call->fetch_blocking();
        return call->get_result_locked();
    }
    else
    {
        auto call = std::make_shared<DBusRNF::GetRangeCall>(
                            cm, proxy, list_iface_name, list_id,
                            std::move(window), nullptr, nullptr);

        call->request();
        call->fetch_blocking();
        return call->get_result_locked();
    }
}

List::AsyncListIface::OpResult
List::DBusList::enter_list_async(const DBusListViewport *associated_viewport,
                                 ID::List list_id, unsigned int line,
                                 QueryContextEnterList::CallerID caller_id,
                                 I18n::String &&dynamic_title)
{
    log_assert(list_id.is_valid());

    LOGGED_LOCK_CONTEXT_HINT;
    std::lock_guard<LoggedLock::RecMutex> lk(lock_);

    enter_list_data_.cancel_enter_list_query();

    enter_list_data_.query_ =
        std::make_shared<QueryContextEnterList>(*this,
                                                static_cast<unsigned short>(caller_id),
                                                dbus_proxy_, associated_viewport,
                                                list_id, line,
                                                std::move(dynamic_title));

    if(enter_list_data_.query_ == nullptr)
    {
        msg_out_of_memory("asynchronous context (enter list)");
        return OpResult::FAILED;
    }

    if(!enter_list_data_.query_->run_async(
            [this, q = enter_list_data_.query_] (DBus::AsyncCall_ &c)
            {
                LOGGED_LOCK_CONTEXT_HINT;
                std::lock_guard<LoggedLock::RecMutex> llk(lock_);

                if(q != enter_list_data_.query_)
                    msg_error(0, LOG_NOTICE,
                              "Async enter-list done notification for "
                              "outdated query %p (expected %p)",
                              static_cast<const void *>(q.get()),
                              static_cast<const void *>(enter_list_data_.query_.get()));
                else if(&c != enter_list_data_.query_->async_call_.get())
                {
                    if(dynamic_cast<QueryContextEnterList::AsyncListNavCheckRange *>(&c) != nullptr)
                        BUG("Unexpected async enter-list done notification [%s]",
                            list_iface_name_.c_str());
                    else
                        BUG("Unexpected UNKNOWN async done notification [%s]",
                            list_iface_name_.c_str());
                }
                else
                {
                    enter_list_data_.query_ = nullptr;
                    enter_list_async_handle_done(std::move(q));
                }
            }))
    {
        enter_list_data_.query_ = nullptr;
        return OpResult::FAILED;
    }

    notify_watcher(OpResult::STARTED, enter_list_data_.query_);

    return OpResult::STARTED;
}

void List::DBusList::enter_list_async_handle_done(std::shared_ptr<QueryContextEnterList> q)
{
    const ID::List list_id(q->parameters_.list_id_.get_raw_id());
    DBus::AsyncResult async_result;

    try
    {
        q->synchronize(async_result);
    }
    catch(const List::DBusListException &e)
    {
        async_result = DBus::AsyncResult::FAILED;
        msg_error(0, LOG_ERR, "List error %u: %s [%s]",
                  e.get(), e.what(), list_iface_name_.c_str());
    }

    OpResult op_result;

    if(async_result == DBus::AsyncResult::DONE)
    {
        op_result = OpResult::SUCCEEDED;

        const auto &size(q->async_call_->get_result(async_result));

        number_of_items_ = size;
        list_id_ = q->parameters_.list_id_;

        for(auto &vp : viewports_and_fetchers_)
            if(vp.first.get() != q->parameters_.associated_viewport_)
                vp.first->clear_for_line(0);
            else
                vp.first->clear_for_line(q->parameters_.line_);
    }
    else
    {
        op_result = (async_result == DBus::AsyncResult::CANCELED
                     ? OpResult::CANCELED
                     : OpResult::FAILED);

        msg_error(0, LOG_NOTICE,
                  "Failed obtaining size of list %u (async) [%s]",
                  list_id.get_raw_id(), list_iface_name_.c_str());
    }

    notify_watcher(op_result, q);
}

void List::DBusList::list_invalidate(ID::List list_id, ID::List replacement_id)
{
    LOGGED_LOCK_CONTEXT_HINT;
    std::lock_guard<LoggedLock::RecMutex> lk(lock_);

    if(list_id_ == list_id)
    {
        list_id_ = replacement_id;

        if(replacement_id.is_valid())
        {
            for(auto &vp : viewports_and_fetchers_)
                if(vp.second != nullptr)
                    vp.second->cancel_op();
        }
        else
            for(auto &vp : viewports_and_fetchers_)
                vp.first->clear_for_line(0);
    }

    QueryContextEnterList::restart_if_necessary(enter_list_data_.query_,
                                                list_id, replacement_id);
}

const List::ContextInfo &List::DBusList::get_context_info_by_list_id(ID::List id) const
{
    return list_contexts_[DBUS_LISTS_CONTEXT_GET(id.get_raw_id())];
}

bool List::QueryContextEnterList::run_async(DBus::AsyncResultAvailableFunction &&result_available)
{
    log_assert(async_call_ == nullptr);

    async_call_ = std::make_shared<AsyncListNavCheckRange>(
        proxy_,
        [] (GObject *source_object) { return TDBUS_LISTS_NAVIGATION(source_object); },
        [this]
        (auto &ready, auto &promise, tdbuslistsNavigation *p,
         GAsyncResult *result, GErrorWrapper &error)
        { put_result(ready, promise, p, result, error, parameters_.list_id_); },
        std::move(result_available),
        [] (AsyncListNavCheckRange::PromiseReturnType &values) {},
        [] () { return true; },
        "AsyncListNavCheckRange", MESSAGE_LEVEL_DEBUG);

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

            async_call_ = nullptr;
        }
        catch(const List::DBusListException &e)
        {
            result = DBus::AsyncResult::FAILED;
            async_call_ = nullptr;
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
        ctx = nullptr;
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
                std::make_shared<QueryContextEnterList>(
                    *ctx_local_ref, ctx_local_ref->parameters_.associated_viewport_,
                    replacement_id, ctx_local_ref->parameters_.line_,
                    std::move(I18n::String(ctx_local_ref->parameters_.title_)));

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
                                             GErrorWrapper &error, ID::List list_id)
{
    guchar error_code = 0;
    guint first_item = 0;
    guint size = 0;

    async_ready =
        tdbus_lists_navigation_call_check_range_finish(p, &error_code,
                                                       &first_item, &size,
                                                       async_result, error.await())
        ? DBus::AsyncResult::READY
        : DBus::AsyncResult::FAILED;

    if(async_ready == DBus::AsyncResult::FAILED)
        throw List::DBusListException(error);

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

      case ListError::Code::INVALID_URI:
        msg_error(EINVAL, LOG_NOTICE,
                  "Invalid URI for list ID %u, cannot put async result",
                  list_id.get_raw_id());
        break;

      case ListError::Code::INVALID_STRBO_URL:
        msg_error(EINVAL, LOG_NOTICE,
                  "Invalid StrBo URL for list ID %u, cannot put async result",
                  list_id.get_raw_id());
        break;

      case ListError::Code::NOT_FOUND:
        msg_error(EINVAL, LOG_NOTICE,
                  "Failed to locate content for list ID %u, cannot put async result",
                  list_id.get_raw_id());
        break;

      case ListError::Code::BUSY_500:
      case ListError::Code::BUSY_1000:
      case ListError::Code::BUSY_1500:
      case ListError::Code::BUSY_3000:
      case ListError::Code::BUSY_5000:
      case ListError::Code::BUSY:
        BUG("List broker is busy, should retry checking range later");

        /* fall-through */

      case ListError::Code::INTERRUPTED:
      case ListError::Code::PHYSICAL_MEDIA_IO:
      case ListError::Code::NET_IO:
      case ListError::Code::PROTOCOL:
      case ListError::Code::AUTHENTICATION:
      case ListError::Code::INCONSISTENT:
      case ListError::Code::PERMISSION_DENIED:
      case ListError::Code::NOT_SUPPORTED:
      case ListError::Code::OUT_OF_RANGE:
      case ListError::Code::EMPTY:
      case ListError::Code::OVERFLOWN:
      case ListError::Code::UNDERFLOWN:
      case ListError::Code::INVALID_STREAM_URL:
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

void List::DBusList::add_referrer(std::shared_ptr<DBusListViewport> vp)
{
    log_assert(vp != nullptr);
    if(viewports_and_fetchers_.find(vp) == viewports_and_fetchers_.end())
        viewports_and_fetchers_[vp] = nullptr;
}

static void update_viewport_cache(List::DBusListViewport &viewport,
                                  const DBusRNF::GetRangeResult &result,
                                  const List::DBusListViewport::NewItemFn &new_item_fn)
{
    viewport.locked(
        [&result, &new_item_fn] (auto &vp)
        {
            const unsigned int beginning_of_gap = vp.prepare_update();

            if(result.have_meta_data_)
                vp.update_cache_region_with_meta_data(
                        new_item_fn, beginning_of_gap, result.list_);
            else
                vp.update_cache_region_simple(
                        new_item_fn, beginning_of_gap, result.list_);
        });
}

const List::Item *
List::DBusList::get_item(std::shared_ptr<DBusListViewport> vp, unsigned int line)
{
    log_assert(vp != nullptr);

    LOGGED_LOCK_CONTEXT_HINT;
    std::lock_guard<LoggedLock::RecMutex> lk(lock_);

    add_referrer(vp);

    log_assert(list_id_.is_valid());

    if(line >= number_of_items_)
        return nullptr;

    {
        const auto item = vp->item_at(line);

        if(item.first != nullptr)
        {
            BUG_IF(!item.second, "Returning invisible item");
            return item.first;
        }

        BUG_IF(item.second, "View diverges from cache while using synchronous API");
    }

    unsigned int cached_lines_count;
    vp->set_view(line, vp->get_default_view_size(), get_number_of_items(),
                 cached_lines_count);

    Segment missing(vp->get_missing_segment());

    try
    {
        const auto expected_size = missing.size();
        const DBusRNF::GetRangeResult result(
            fetch_window_sync(cm_, dbus_proxy_, list_iface_name_, list_contexts_,
                              list_id_, std::move(missing)));

        log_assert(g_variant_n_children(GVariantWrapper::get(result.list_)) == expected_size);
        update_viewport_cache(*vp, result, new_item_fn_);
    }
    catch(const std::exception &e)
    {
        BUG("Exception while getting item in line %u: %s", line, e.what());
        return nullptr;
    }
    catch(...)
    {
        BUG("Exception while getting item in line %u", line);
        return nullptr;
    }

    return vp->item_at(line).first;
}

static bool announce_viewport(List::DBusList::ViewportsAndFetchersMap &vfm,
                              std::shared_ptr<List::DBusListViewport> viewport,
                              unsigned int line)
{
    auto vf(vfm.find(viewport));

    if(vf == vfm.end())
    {
        /* viewport is yet unknown, so clear it and add it to our map */
        viewport->clear_for_line(line);
        vfm[viewport] = nullptr;
        return true;
    }

    if(vf->second == nullptr)
    {
        /* viewport is known, but there is no active filler */
        return true;
    }

    if(vf->second->is_filling_viewport(*viewport))
    {
        /* we are currently filling in the range we want */
        return false;
    }

    /* viewport is known, filler is out of date */
    vf->second->cancel_op();
    vf->second = nullptr;
    return true;
}

void List::DBusList::detach_viewport(std::shared_ptr<DBusListViewport> vp)
{
    log_assert(vp != nullptr);

    LOGGED_LOCK_CONTEXT_HINT;
    std::lock_guard<LoggedLock::RecMutex> lk(lock_);

    viewports_and_fetchers_.erase(vp);
}

std::shared_ptr<DBusRNF::GetRangeCallBase>
List::DBusList::mk_get_range_rnf_call(ID::List list_id, bool with_meta_data,
                                      Segment &&segment,
                                      std::unique_ptr<QueryContextGetItem> ctx,
                                      DBusRNF::StatusWatcher &&watcher) const
{
    if(with_meta_data)
        return
            std::make_shared<DBusRNF::GetRangeWithMetaDataCall>(
                    cm_, dbus_proxy_, list_iface_name_, list_id,
                    std::move(segment), std::move(ctx), std::move(watcher));
    else
        return
            std::make_shared<DBusRNF::GetRangeCall>(
                    cm_, dbus_proxy_, list_iface_name_, list_id,
                    std::move(segment), std::move(ctx), std::move(watcher));
}

List::AsyncListIface::OpResult
List::DBusList::get_item_async_set_hint(std::shared_ptr<DBusListViewport> vp,
                                        unsigned int line, unsigned int count,
                                        DBusRNF::StatusWatcher &&status_watcher,
                                        HintItemDoneNotification &&hinted_fn)
{
    log_assert(vp != nullptr);

    LOGGED_LOCK_CONTEXT_HINT;
    std::lock_guard<LoggedLock::RecMutex> lk(lock_);

    if(!list_id_.is_valid())
    {
        BUG("Cannot hint item access operation %u +%u for invalid list [%s]",
            line, count, list_iface_name_.c_str());
        return OpResult::FAILED;
    }

    /* already entering a list, cannot get items in this situation */
    if(enter_list_data_.query_ != nullptr)
        return OpResult::CANCELED;

    if(line >= number_of_items_)
        return OpResult::FAILED;

    if(count == 0 || count > vp->get_default_view_size())
    {
        if(count == 0)
            BUG("Hint async operation with no items [%s]",
                list_iface_name_.c_str());
        else
            BUG("Hint async operation with more items (%u) "
                "than default view size (%u) [%s]",
                count, vp->get_default_view_size(), list_iface_name_.c_str());

        return OpResult::FAILED;
    }

    if(!announce_viewport(viewports_and_fetchers_, vp, line))
    {
        const auto &fetcher = viewports_and_fetchers_[vp];
        const auto is_loading_result(fetcher->is_line_loading(line));
        const auto &loading_state(is_loading_result.first);

        switch(loading_state)
        {
          case DBusRNF::GetRangeCallBase::LoadingState::INACTIVE:
          case DBusRNF::GetRangeCallBase::LoadingState::OUT_OF_RANGE:
            break;

          case DBusRNF::GetRangeCallBase::LoadingState::LOADING:
            return OpResult::STARTED;

          case DBusRNF::GetRangeCallBase::LoadingState::DONE:
            return OpResult::SUCCEEDED;

          case DBusRNF::GetRangeCallBase::LoadingState::FAILED_OR_ABORTED:
            return OpResult::FAILED;
        }
    }

    unsigned int cached_lines_count;
    const auto segment_state = vp->set_view(line, count, number_of_items_,
                                            cached_lines_count);

    switch(segment_state)
    {
      case CacheSegmentState::EMPTY:
      case CacheSegmentState::CACHED_CENTER:
      case CacheSegmentState::LOADING_CENTER:
      case CacheSegmentState::LOADING_TOP_EMPTY_BOTTOM:
      case CacheSegmentState::LOADING_BOTTOM_EMPTY_TOP:
        /* need to wipe out everything and restart loading everything */
        break;

      case CacheSegmentState::CACHED_TOP_EMPTY_BOTTOM:
      case CacheSegmentState::CACHED_TOP_LOADING_CENTER_EMPTY_BOTTOM:
        /* need to start loading bottom half, any other load operation can be
         * canceled and ignored */
        break;

      case CacheSegmentState::CACHED_BOTTOM_EMPTY_TOP:
      case CacheSegmentState::CACHED_BOTTOM_LOADING_CENTER_EMPTY_TOP:
        /* need to start loading top half, any other load operation can be
         * canceled and ignored */
        break;

      case CacheSegmentState::CACHED:
        /* everything is there already */
        return OpResult::SUCCEEDED;

      case CacheSegmentState::LOADING:
      case CacheSegmentState::CACHED_TOP_LOADING_BOTTOM:
      case CacheSegmentState::CACHED_BOTTOM_LOADING_TOP:
        /* everything is done that could be done */
        return OpResult::STARTED;
    }

    auto fetcher = std::make_shared<DBusListSegmentFetcher>(vp);

    fetcher->prepare(
        // #List::DBusListSegmentFetcher::MkGetRangeRNFCall
        [this, status_watcher = std::move(status_watcher)]
        (Segment &&missing, std::unique_ptr<QueryContextGetItem> ctx) mutable
        {
            const auto flags =
                list_contexts_[DBUS_LISTS_CONTEXT_GET(list_id_.get_raw_id())]
                .get_flags();
            const bool with_meta_data =
                (flags & List::ContextInfo::HAS_EXTERNAL_META_DATA) != 0;

            return mk_get_range_rnf_call(list_id_, with_meta_data,
                                         std::move(missing), std::move(ctx),
                                         std::move(status_watcher));
        },

        // #List::DBusListSegmentFetcher::DoneFn
        [this, hinted_fn = std::move(hinted_fn)]
        (DBusListSegmentFetcher &sf) mutable
        {
            get_item_result_available_notification(sf, std::move(hinted_fn));
        });

    viewports_and_fetchers_[vp] = fetcher;

    return fetcher->load_segment_in_background();
}

List::AsyncListIface::OpResult
List::DBusList::get_item_async(std::shared_ptr<DBusListViewport> vp,
                               unsigned int line, const Item *&item)
{
    log_assert(vp != nullptr);

    LOGGED_LOCK_CONTEXT_HINT;
    std::lock_guard<LoggedLock::RecMutex> lk(lock_);

    item = nullptr;

    if(!list_id_.is_valid())
    {
        BUG("Cannot fetch line %u for invalid list ID [%s]",
            line, list_iface_name_.c_str());
        return OpResult::FAILED;
    }

    /* already entering a list, cannot get items in this situation */
    if(enter_list_data_.query_ != nullptr)
        return OpResult::CANCELED;

    if(line >= number_of_items_)
        return OpResult::FAILED;

    if(!announce_viewport(viewports_and_fetchers_, vp, line))
    {
        auto &fetcher = viewports_and_fetchers_[vp];
        const auto is_loading_result(fetcher->is_line_loading(line));
        const auto &loading_state(is_loading_result.first);

        switch(loading_state)
        {
          case DBusRNF::GetRangeCallBase::LoadingState::INACTIVE:
          case DBusRNF::GetRangeCallBase::LoadingState::OUT_OF_RANGE:
          case DBusRNF::GetRangeCallBase::LoadingState::DONE:
          case DBusRNF::GetRangeCallBase::LoadingState::FAILED_OR_ABORTED:
            break;

          case DBusRNF::GetRangeCallBase::LoadingState::LOADING:
            item = &ViewFileBrowser::FileItem::get_loading_placeholder();
            return OpResult::STARTED;
        }
    }

    const auto it = vp->item_at(line);

    if(it.first != nullptr)
    {
        item = it.first;
        return OpResult::SUCCEEDED;
    }

    msg_error(0, LOG_NOTICE,
              "Requested line %u out of range (%s) "
              "(cached valid segment %u +%u, view %u +%u)",
              line, it.second ? "visible/invalid" : "invisible/invalid",
              vp->items_segment().line(), vp->items_segment().size(),
              vp->view_segment().line(), vp->view_segment().size());
    return OpResult::FAILED;
}

bool List::DBusList::cancel_all_async_calls()
{
    LOGGED_LOCK_CONTEXT_HINT;
    std::lock_guard<LoggedLock::RecMutex> lk(lock_);

    switch(enter_list_data_.cancel_all(viewports_and_fetchers_))
    {
      case DBus::CancelResult::CANCELED:
      case DBus::CancelResult::NOT_RUNNING:
        return true;

      case DBus::CancelResult::BLOCKED_RECURSIVE_CALL:
        break;
    }

    return false;
}

void List::DBusList::get_item_result_available_notification(
        DBusListSegmentFetcher &fetcher, HintItemDoneNotification &&hinted_fn)
{
    auto call_and_viewport(fetcher.take_rnf_call_and_viewport());
    auto call(std::move(call_and_viewport.first));
    auto viewport(std::move(call_and_viewport.second));

    LOGGED_LOCK_CONTEXT_HINT;
    std::lock_guard<LoggedLock::RecMutex> lk(lock_);

    {
        auto vfpair(viewports_and_fetchers_.find(viewport));
        BUG_IF(vfpair == viewports_and_fetchers_.end(),
               "Viewport %p not found", static_cast<const void *>(viewport.get()));

        if(vfpair->second.get() != &fetcher)
        {
            log_assert(hinted_fn != nullptr);
            hinted_fn(OpResult::CANCELED);
            return;
        }

        vfpair->second = nullptr;
    }

    OpResult op_result;

    try
    {
        auto result(call->get_result_locked());

        update_viewport_cache(*viewport, result, new_item_fn_);
        op_result = OpResult::SUCCEEDED;
    }
    catch(const DBusRNF::AbortedError &)
    {
        op_result = OpResult::CANCELED;
    }
    catch(const List::DBusListException &e)
    {
        op_result = OpResult::FAILED;
        msg_error(0, LOG_ERR, "List error %u: %s [%s]",
                  e.get(), e.what(), list_iface_name_.c_str());
    }
    catch(const std::exception &e)
    {
        op_result = OpResult::FAILED;
        msg_error(0, LOG_ERR, "List error: %s [%s]",
                  e.what(), list_iface_name_.c_str());
    }
    catch(...)
    {
        op_result = OpResult::FAILED;
        msg_error(0, LOG_ERR, "List error [%s]", list_iface_name_.c_str());
    }

    if(op_result != OpResult::SUCCEEDED)
        msg_error(0, LOG_NOTICE,
                  "%s obtaining lines %u through %u of list %u [%s]",
                  op_result == OpResult::FAILED ? "Failed" : "Canceled",
                  call->loading_segment_.line(),
                  call->loading_segment_.line() + call->loading_segment_.size() - 1,
                  call->list_id_.get_raw_id(), list_iface_name_.c_str());

    log_assert(hinted_fn != nullptr);
    hinted_fn(op_result);
}

void List::DBusList::async_done_notification(DBus::AsyncCall_ &async_call)
{
    LOGGED_LOCK_CONTEXT_HINT;
    std::lock_guard<LoggedLock::RecMutex> lk(lock_);

    if(enter_list_data_.query_ != nullptr)
    {
        if(&async_call == enter_list_data_.query_->async_call_.get())
        {
            auto c(std::move(enter_list_data_.query_));
            enter_list_data_.query_ = nullptr;
            enter_list_async_handle_done(c);
        }
    }
    else
    {
        if(dynamic_cast<QueryContextEnterList::AsyncListNavCheckRange *>(&async_call) != nullptr)
            BUG("Unexpected async enter-line done notification [%s]",
                list_iface_name_.c_str());
        else
            BUG("Unexpected UNKNOWN async done notification [%s]",
                list_iface_name_.c_str());
    }
}

std::string
List::DBusList::get_get_range_op_description(const DBusListViewport &viewport) const
{
    LOGGED_LOCK_CONTEXT_HINT;
    std::lock_guard<LoggedLock::RecMutex> lk(lock_);

    const auto it(std::find_if(
        viewports_and_fetchers_.begin(), viewports_and_fetchers_.end(),
        [&viewport] (const auto &vf) { return vf.first.get() == &viewport; }));

    if(it == viewports_and_fetchers_.end() || it->second == nullptr ||
       it->second->query() == nullptr)
        return "";

    std::ostringstream os;
    os << it->second->query().get() << " " << it->second->query()->get_description();
    return os.str();
}
