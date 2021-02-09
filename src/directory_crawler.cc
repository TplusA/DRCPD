/*
 * Copyright (C) 2016--2021  T+A elektroakustik GmbH & Co. KG
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

#include "directory_crawler.hh"

#include <algorithm>
#include <sstream>

void Playlist::Crawler::DirectoryCrawler::init_dbus_list_watcher()
{
    traversal_list_.register_enter_list_watcher(
        [this] (List::AsyncListIface::OpResult result,
                std::shared_ptr<List::QueryContextEnterList> ctx)
        {
            async_list__enter_list_event(result, std::move(ctx));
        });
}

Playlist::Crawler::PublicIface &
Playlist::Crawler::DirectoryCrawler::set_cursor(const CursorBase &cursor)
{
    const auto *const c = dynamic_cast<const Cursor *>(&cursor);
    log_assert(c != nullptr);
    start_cache_enforcer(c->get_list_id());
    return *this;
}

void Playlist::Crawler::DirectoryCrawler::deactivated(std::shared_ptr<CursorBase> cursor)
{
    stop_cache_enforcer();
}

bool Playlist::Crawler::DirectoryCrawler::list_invalidate(ID::List list_id, ID::List replacement_id)
{
    LOGGED_LOCK_CONTEXT_HINT;
    std::lock_guard<LoggedLock::Mutex> lock(lock_);

    log_assert(list_id.is_valid());

    if(DerivedCrawlerFuns::reference_point(*this) == nullptr)
        return false;

    if(cache_enforcer_ != nullptr && cache_enforcer_->get_list_id() == list_id)
    {
        const bool restart_enforcer =
            !cache_enforcer_->is_stopped() && replacement_id.is_valid();

        stop_cache_enforcer(false);

        if(restart_enforcer)
            start_cache_enforcer(replacement_id);
    }

    /* TODO: Need to update cursors in each op */
    if(!DerivedCrawlerFuns::ops(*this).empty())
        MSG_NOT_IMPLEMENTED();

    return std::dynamic_pointer_cast<Cursor>(DerivedCrawlerFuns::reference_point(*this))->list_invalidate(list_id, replacement_id)
        ? !replacement_id.is_valid()
        : traversal_list_.get_list_id() == list_id;
}

List::AsyncListIface::OpResult
Playlist::Crawler::DirectoryCrawler::Cursor::hint_planned_access(
        List::DBusList &list, bool forward,
        List::AsyncListIface::HintItemDoneNotification &&hinted_fn)
{
    const auto total_list_size = nav_.get_total_number_of_visible_items();
    const auto hint_count = nav_.maximum_number_of_displayed_lines_;
    log_assert(hint_count > 0);

    if(total_list_size <= hint_count)
        return list.get_item_async_set_hint(nav_.get_viewport(),
                                            0, total_list_size, nullptr,
                                            std::move(hinted_fn));

    const auto cursor_pos = nav_.get_cursor();
    unsigned int start_pos;

    if(forward)
        start_pos = ((cursor_pos + hint_count <= total_list_size)
                     ? cursor_pos
                     : total_list_size - hint_count);
    else
        start_pos = ((cursor_pos >= hint_count)
                     ? cursor_pos - (hint_count - 1)
                     : 0);

    return list.get_item_async_set_hint(nav_.get_viewport(),
                                        start_pos, hint_count, nullptr,
                                        std::move(hinted_fn));
}

std::string Playlist::Crawler::DirectoryCrawler::Cursor::get_description(bool full) const
{
    std::ostringstream os;

    os << "Position: [list " << list_id_.get_raw_id()
       << " line " << nav_.get_cursor_unchecked()
       << " depth " << directory_depth_ << "]";

    if(full)
       os << "; Requested: [list " << requested_list_id_.get_raw_id()
          << " line " << requested_line_
          << "]; " << nav_.get_total_number_of_visible_items()
          << " visible items";

    return os.str();
}

void Playlist::Crawler::DirectoryCrawler::async_list__enter_list_event(
        List::AsyncListIface::OpResult result,
        std::shared_ptr<List::QueryContextEnterList> ctx)
{
    switch(result)
    {
      case List::AsyncListIface::OpResult::STARTED:
        return;

      case List::AsyncListIface::OpResult::SUCCEEDED:
      case List::AsyncListIface::OpResult::FAILED:
      case List::AsyncListIface::OpResult::CANCELED:
        break;

      case List::AsyncListIface::OpResult::BUSY:
        MSG_UNREACHABLE();
        return;
    }

    LOGGED_LOCK_CONTEXT_HINT;
    std::lock_guard<LoggedLock::Mutex> lock(lock_);

    const auto cid(ctx->get_caller_id());
    const auto &found_op =
        std::find_if(
            DerivedCrawlerFuns::ops(*this).begin(),
            DerivedCrawlerFuns::ops(*this).end(),
            [&ctx, cid] (const auto &op)
            {
                const auto fop = std::dynamic_pointer_cast<FindNextOp>(op);
                return fop != nullptr && fop->matches_async_result(*ctx, cid);
            });

    if(found_op != DerivedCrawlerFuns::ops(*this).end())
        std::static_pointer_cast<FindNextOp>(*found_op)->enter_list_event(result, *ctx);
    else
        BUG("Got asynchronous enter-list result %d (cid %d), "
            "but found no matching op", int(result), int(cid));
}

void Playlist::Crawler::DirectoryCrawler::start_cache_enforcer(ID::List list_id)
{
    msg_info("Keeping list %u in cache", list_id.get_raw_id());

    log_assert(list_id.is_valid());

    stop_cache_enforcer();

    cache_enforcer_ = std::make_unique<CacheEnforcer>(traversal_list_, list_id);
    cache_enforcer_->start();
}

bool Playlist::Crawler::DirectoryCrawler::stop_cache_enforcer(bool remove_override)
{
    if(cache_enforcer_ == nullptr)
        return false;

    msg_info("Stop keeping list %u in cache",
             cache_enforcer_->get_list_id().get_raw_id());

    CacheEnforcer::stop(std::move(cache_enforcer_), remove_override);

    return true;
}
