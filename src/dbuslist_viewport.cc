/*
 * Copyright (C) 2020  T+A elektroakustik GmbH & Co. KG
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
#include "main_context.hh"

List::CacheSegmentState
List::DBusListViewport::compute_overlap(const Segment &segment,
                                        unsigned int &cached_lines_count) const
{
    CacheSegmentState cached_state = CacheSegmentState::EMPTY;

    switch(segment.intersection(items_segment_, cached_lines_count))
    {
      case SegmentIntersection::DISJOINT:
        break;

      case SegmentIntersection::EQUAL:
      case SegmentIntersection::INCLUDED_IN_OTHER:
        cached_state = CacheSegmentState::CACHED;
        break;

      case SegmentIntersection::TOP_REMAINS:
        cached_state = CacheSegmentState::CACHED_TOP_EMPTY_BOTTOM;
        break;

      case SegmentIntersection::BOTTOM_REMAINS:
        cached_state = CacheSegmentState::CACHED_BOTTOM_EMPTY_TOP;
        break;

      case SegmentIntersection::CENTER_REMAINS:
        cached_state = CacheSegmentState::CACHED_CENTER;
        break;
    }

    if(cached_lines_count == 0)
        cached_state = CacheSegmentState::EMPTY;

    return cached_state;
}

List::CacheSegmentState
List::DBusListViewport::set_view(unsigned int line, unsigned int count,
                                 unsigned int total_number_of_lines,
                                 unsigned int &cached_lines_count)
{
    LOGGED_LOCK_CONTEXT_HINT;
    std::lock_guard<LoggedLock::Mutex> lk(lock_);

    /* avoid integer overflows */
    if(line > UINT_MAX - count)
        line = UINT_MAX - count;

    if(line + count <= total_number_of_lines)
    {
        /* regular case */
        view_segment_ = Segment(line, count);
    }
    else if(count <= total_number_of_lines)
    {
        /* requested segment covers end of list and goes beyond */
        view_segment_ = Segment(total_number_of_lines - count, count);
    }
    else
    {
        /* requested segment is larger than whole list */
        view_segment_ = Segment(0, total_number_of_lines);
    }

    return compute_overlap(view_segment_, cached_lines_count);
}

List::Segment List::DBusListViewport::get_missing_segment() const
{
    unsigned int intersection_size;

    switch(view_segment_.intersection(items_segment_, intersection_size))
    {
      case SegmentIntersection::DISJOINT:
      case SegmentIntersection::CENTER_REMAINS:
        return Segment(view_segment_);

      case SegmentIntersection::EQUAL:
      case SegmentIntersection::INCLUDED_IN_OTHER:
        return Segment(view_segment_.line(), 0);

      case SegmentIntersection::TOP_REMAINS:
        return Segment(view_segment_.line() + intersection_size,
                       view_segment_.size() - intersection_size);

      case SegmentIntersection::BOTTOM_REMAINS:
        return Segment(view_segment_.line(),
                       view_segment_.size() - intersection_size);
    }

    MSG_UNREACHABLE();
    return Segment(view_segment_);
}

unsigned int List::DBusListViewport::prepare_update()
{
    unsigned int intersection_size;
    unsigned int beginning_of_gap = 0;

    switch(view_segment_.intersection(items_segment_, intersection_size))
    {
      case SegmentIntersection::DISJOINT:
      case SegmentIntersection::CENTER_REMAINS:
        items_.clear();
        break;

      case SegmentIntersection::EQUAL:
      case SegmentIntersection::INCLUDED_IN_OTHER:
        break;

      case SegmentIntersection::TOP_REMAINS:
        items_.shift_up(items_.get_number_of_items() - intersection_size);
        beginning_of_gap = intersection_size;
        break;

      case SegmentIntersection::BOTTOM_REMAINS:
        items_.shift_down(items_.get_number_of_items() - intersection_size);
        break;
    }

    return beginning_of_gap;
}

void List::DBusListViewport::update_cache_region_simple(
        NewItemFn new_item_fn, unsigned int cache_list_index,
        const GVariantWrapper &dbus_data)
{
    GVariantIter iter;

    if(g_variant_iter_init(&iter, GVariantWrapper::get(dbus_data)) <= 0)
        return;

    const bool replace_mode = !items_.empty();
    const gchar *name;
    uint8_t item_kind;

    while(g_variant_iter_next(&iter, "(&sy)", &name, &item_kind))
    {
        if(replace_mode)
            items_.replace(cache_list_index++,
                           new_item_fn(name, ListItemKind(item_kind), nullptr));
        else
            items_.append(new_item_fn(name, ListItemKind(item_kind), nullptr));
    }

    items_segment_ = view_segment_;
}

void List::DBusListViewport::update_cache_region_with_meta_data(
        NewItemFn new_item_fn, unsigned int cache_list_index,
        const GVariantWrapper &dbus_data)
{
    GVariantIter iter;

    if(g_variant_iter_init(&iter, GVariantWrapper::get(dbus_data)) <= 0)
        return;

    const bool replace_mode = !items_.empty();
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
            BUG("Got unexpected index of primary name (%u) [%s]",
                primary_name_index, items_.get_list_iface_name().c_str());
            primary_name_index = 0;
        }

        static const char empty_item_string[] = "----";

        const gchar *name = ((primary_name_index != UINT8_MAX)
                             ? names[primary_name_index]
                             : empty_item_string);

        if(primary_name_index == UINT8_MAX)
            item_kind = ListItemKind::LOCKED;

        if(replace_mode)
            items_.replace(cache_list_index++,
                           new_item_fn(name, ListItemKind(item_kind), names));
        else
            items_.append(new_item_fn(name, ListItemKind(item_kind), names));
    }

    items_segment_ = view_segment_;
}

List::DBusListSegmentFetcher::DBusListSegmentFetcher(
        std::shared_ptr<DBusListViewport> list_viewport):
    is_cancel_blocked_(false),
    is_done_notification_deferred_(false),
    list_viewport_(std::move(list_viewport))
{
    LoggedLock::configure(lock_, "DBusListSegmentFetcher", MESSAGE_LEVEL_DEBUG);
    log_assert(list_viewport_ != nullptr);
}

void List::DBusListSegmentFetcher::prepare(const MkGetRangeRNFCall &mk_call,
                                           DoneFn &&done_fn)
{
    auto ctx =
        std::make_unique<QueryContextGetItem>(
            // #DBusRNF::ContextData::NotificationFunction
            [fetcher = this->shared_from_this(), done_fn = std::move(done_fn)]
            (DBusRNF::CallBase &call, DBusRNF::CallState)
            {
                LOGGED_LOCK_CONTEXT_HINT;
                std::lock_guard<LoggedLock::RecMutex> l(fetcher->lock_);

                if(&call != fetcher->get_range_query_.get())
                {
                    BUG("Got done notification for unknown GetItem call");
                    return;
                }

                auto fn = new std::function<void()>(
                    [fetcher, done_fn = std::move(done_fn)] ()
                    {
                        fetcher->is_done_notification_deferred_ = false;
                        done_fn(*fetcher);
                    });

                fetcher->is_done_notification_deferred_ = true;
                MainContext::deferred_call(fn, false);
            });

    Segment missing(list_viewport_->get_missing_segment());

    get_range_query_ = mk_call(std::move(missing), std::move(ctx));
    log_assert(get_range_query_ != nullptr);
}

List::AsyncListIface::OpResult
List::DBusListSegmentFetcher::load_segment_in_background()
{
    LOGGED_LOCK_CONTEXT_HINT;
    std::lock_guard<LoggedLock::RecMutex> lock(lock_);

    log_assert(get_range_query_ != nullptr);

    switch(get_range_query_->request())
    {
      case DBusRNF::CallState::WAIT_FOR_NOTIFICATION:
        return AsyncListIface::OpResult::STARTED;

      case DBusRNF::CallState::RESULT_FETCHED:
        return AsyncListIface::OpResult::SUCCEEDED;

      case DBusRNF::CallState::INITIALIZED:
      case DBusRNF::CallState::READY_TO_FETCH:
        BUG("GetRangeCallBase ended up in unexpected state");
        break;

      case DBusRNF::CallState::ABORTING:
      case DBusRNF::CallState::ABORTED_BY_LIST_BROKER:
        return AsyncListIface::OpResult::CANCELED;

      case DBusRNF::CallState::FAILED:
      case DBusRNF::CallState::ABOUT_TO_DESTROY:
        break;
    }

    get_range_query_ = nullptr;
    return AsyncListIface::OpResult::FAILED;
}
