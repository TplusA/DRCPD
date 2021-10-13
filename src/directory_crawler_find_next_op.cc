/*
 * Copyright (C) 2019, 2020, 2021  T+A elektroakustik GmbH & Co. KG
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
#include "view_filebrowser_fileitem.hh"
#include "view_filebrowser_utils.hh"
#include "view_play.hh"
#include "dump_enum_value.hh"

static const char skip_message_fmt[] = "Skipping directory \"%s\" (%s)";

/* just in case we need a hook for the debugger */
static Playlist::Crawler::DirectoryCrawler::FindNextOp::Continue fail_here()
{
    return Playlist::Crawler::DirectoryCrawler::FindNextOp::Continue::NOT_WITH_ERROR;
}

/* just in case we need a hook for the debugger */
static Playlist::Crawler::DirectoryCrawler::FindNextOp::Continue succeed_here()
{
    return Playlist::Crawler::DirectoryCrawler::FindNextOp::Continue::NOT_WITH_SUCCESS;
}

static bool is_forward_direction(Playlist::Crawler::Direction d)
{
    return d != Playlist::Crawler::Direction::BACKWARD;
}

bool Playlist::Crawler::DirectoryCrawler::FindNextOp::check_skip_directory(const ViewFileBrowser::FileItem &item) const
{
    switch(recursive_mode_)
    {
      case RecursiveMode::FLAT:
        msg_info(skip_message_fmt, item.get_text(), "non-recursive mode");
        return true;

      case RecursiveMode::DEPTH_FIRST:
        break;
    }

    if(directory_depth_ >= MAX_DIRECTORY_DEPTH)
    {
        msg_error(0, LOG_NOTICE, skip_message_fmt, item.get_text(),
                  "maximum directory depth reached");
        return true;
    }

    return false;
}

bool Playlist::Crawler::DirectoryCrawler::FindNextOp::finish_op_if_possible(Continue cont)
{
    switch(cont)
    {
      case Continue::NOT_WITH_ERROR:
        operation_finished(false);
        return true;

      case Continue::NOT_WITH_SUCCESS:
        operation_finished(true);
        return true;

      case Continue::LATER:
        return true;

      case Continue::WITH_THIS_ITEM:
        break;
    }

    return false;
}

void Playlist::Crawler::DirectoryCrawler::FindNextOp::run_as_far_as_possible()
{
    while(true)
    {
        if(finish_op_if_possible(finish_with_current_item_or_continue()))
            return;
    }
}

/*!
 * Get child ID.
 *
 * This function calls #get_child_item_internal(), which invokes
 * \c de.tahifi.Lists.Navigation.GetListId synchronously in turn.
 *
 * \bug Synchronous D-Bus call of potentially long-running method.
 */
static ID::List get_child_id_for_enter(List::DBusList &dbus_list, List::Nav &navigation,
                                       const ViewFileBrowser::FileItem &file_item,
                                       bool &is_hard_error)
{
    is_hard_error = false;

    try
    {
        std::string list_title;
        return ViewFileBrowser::Utils::get_child_item_id(
                    dbus_list, dbus_list.get_list_id(),
                    navigation, nullptr, nullptr, list_title, true);
    }
    catch(const List::DBusListException &e)
    {
        msg_error(0, LOG_NOTICE, skip_message_fmt, file_item.get_text(), e.what());

        switch(e.get())
        {
          case ListError::Code::OK:
          case ListError::Code::INTERNAL:
          case ListError::Code::INVALID_ID:
          case ListError::Code::INVALID_URI:
          case ListError::Code::INCONSISTENT:
          case ListError::Code::OUT_OF_RANGE:
          case ListError::Code::EMPTY:
          case ListError::Code::OVERFLOWN:
          case ListError::Code::UNDERFLOWN:
          case ListError::Code::INVALID_STREAM_URL:
          case ListError::Code::INVALID_STRBO_URL:
          case ListError::Code::NOT_FOUND:
            break;

          case ListError::Code::BUSY_500:
          case ListError::Code::BUSY_1000:
          case ListError::Code::BUSY_1500:
          case ListError::Code::BUSY_3000:
          case ListError::Code::BUSY_5000:
          case ListError::Code::BUSY:
            BUG("List broker is busy, should retry later");
            return ID::List();

          case ListError::Code::INTERRUPTED:
          case ListError::Code::PHYSICAL_MEDIA_IO:
          case ListError::Code::NET_IO:
          case ListError::Code::PROTOCOL:
          case ListError::Code::AUTHENTICATION:
          case ListError::Code::NOT_SUPPORTED:
          case ListError::Code::PERMISSION_DENIED:
            return ID::List();
        }

        is_hard_error = true;
        return ID::List();
    }
}

static void fill_in_meta_data(MetaData::Set &md,
                              const ViewFileBrowser::FileItem *file_item)
{
    if(file_item == nullptr)
        return;

    const auto &pl(file_item->get_preloaded_meta_data());

    md.add(MetaData::Set::ARTIST, pl.artist_.c_str());
    md.add(MetaData::Set::ALBUM,  pl.album_.c_str());
    md.add(MetaData::Set::TITLE,  pl.title_.c_str());
    md.add(MetaData::Set::INTERNAL_DRCPD_TITLE, file_item->get_text());
}

Playlist::Crawler::DirectoryCrawler::FindNextOp::Continue
Playlist::Crawler::DirectoryCrawler::FindNextOp::finish_with_current_item_or_continue()
{
    if(is_waiting_for_item_hint_)
        return Continue::LATER;

    switch(direction_)
    {
      case Direction::NONE:
        break;

      case Direction::FORWARD:
        if(!has_skipped_first_ &&
           (position_->nav_.get_cursor() >= position_->nav_.get_total_number_of_visible_items() ||
            entering_list_caller_id_ == List::QueryContextEnterList::CallerID::CRAWLER_ASCEND))
        {
            if(entering_list_caller_id_ == List::QueryContextEnterList::CallerID::CRAWLER_ASCEND)
                has_skipped_first_ = true;

            return continue_search();
        }

        break;

      case Direction::BACKWARD:
        if(!has_skipped_first_ &&
           (position_->nav_.get_cursor() == 0 ||
            entering_list_caller_id_ == List::QueryContextEnterList::CallerID::CRAWLER_ASCEND))
        {
            if(entering_list_caller_id_ == List::QueryContextEnterList::CallerID::CRAWLER_ASCEND)
                has_skipped_first_ = true;

            return continue_search();
        }

        break;
    }

    const auto hint_result =
        position_->hint_planned_access(
            dbus_list_, is_forward_direction(direction_),
            [op = std::move(std::static_pointer_cast<FindNextOp>(shared_from_this()))]
            (auto op_result)
            {
                if(!op->is_op_active())
                    return;

                op->is_waiting_for_item_hint_ = false;
                op->run_as_far_as_possible();
            });

    switch(hint_result)
    {
      case List::AsyncListIface::OpResult::SUCCEEDED:
        break;

      case List::AsyncListIface::OpResult::STARTED:
        is_waiting_for_item_hint_ = true;
        return Continue::LATER;

      case List::AsyncListIface::OpResult::BUSY:
        return Continue::LATER;

      case List::AsyncListIface::OpResult::FAILED:
        return fail_here();

      case List::AsyncListIface::OpResult::CANCELED:
        BUG("Unexpected canceled result");
        log_assert(is_op_canceled());
        return Continue::LATER;
    }

    /* may have the item in cache now */
    const List::Item *item;
    auto op_result(dbus_list_.get_item_async(position_->nav_.get_viewport(),
                                             position_->nav_.get_cursor(), item));

    switch(op_result)
    {
      case List::AsyncListIface::OpResult::SUCCEEDED:
        break;

      case List::AsyncListIface::OpResult::STARTED:
        return Continue::LATER;

      case List::AsyncListIface::OpResult::FAILED:
        BUG("Unexpected failed result");
        return fail_here();

      case List::AsyncListIface::OpResult::CANCELED:
        BUG("Unexpected canceled result");
        log_assert(is_op_canceled());
        return Continue::LATER;

      case List::AsyncListIface::OpResult::BUSY:
        return Continue::LATER;
    }

    if(item == nullptr)
    {
        BUG("Unexpected null item");
        return fail_here();
    }

    /* we have something */
    file_item_ = dynamic_cast<const ViewFileBrowser::FileItem *>(item);
    log_assert(file_item_ != nullptr);

    if(!file_item_->get_kind().is_directory())
    {
        /* we have a non-directory item right here */
        result_.pos_state_ = PositionalState::SOMEWHERE_IN_LIST;
        fill_in_meta_data(result_.meta_data_, file_item_);
        return succeed_here();
    }

    /* we have a directory */
    switch(direction_)
    {
      case Direction::NONE:
        /* we wanted a file, but we are not allowed to move to find one */
        return fail_here();

      case Direction::FORWARD:
      case Direction::BACKWARD:
        break;
    }

    if(check_skip_directory(*file_item_))
    {
        ++directories_skipped_;
        return continue_search();
    }

    bool is_hard_error;
    const auto list_id =
        get_child_id_for_enter(dbus_list_, position_->nav_, *file_item_, is_hard_error);

    if(!list_id.is_valid())
    {
        if(is_hard_error)
            return fail_here();

        ++directories_skipped_;
        return continue_search();
    }

    msg_info("Found directory \"%s\", entering", file_item_->get_text());

    entering_list_caller_id_ = List::QueryContextEnterList::CallerID::CRAWLER_DESCEND;
    position_->requested_list_id_ = list_id;
    position_->requested_line_ = 0;

    const auto enter_result =
        dbus_list_.enter_list_async(position_->get_viewport().get(),
                                    position_->requested_list_id_,
                                    position_->requested_line_,
                                    entering_list_caller_id_,
                                    I18n::String(false));

    switch(enter_result)
    {
      case List::AsyncListIface::OpResult::STARTED:
        /*
         * Flow continues in
         * #Playlist::Crawler::DirectoryCrawler::FindNextOp::enter_list_event()
         */
        break;

      case List::AsyncListIface::OpResult::FAILED:
        msg_error(0, LOG_NOTICE, "Failed entering child list");
        return fail_here();

      case List::AsyncListIface::OpResult::SUCCEEDED:
        BUG("Unexpected success from enter_list_async()");
        return fail_here();

      case List::AsyncListIface::OpResult::CANCELED:
        BUG("Unexpected canceled result");
        log_assert(is_op_canceled());
        break;

      case List::AsyncListIface::OpResult::BUSY:
        MSG_UNREACHABLE();
        break;
    }

    return Continue::LATER;
}

Playlist::Crawler::DirectoryCrawler::FindNextOp::Continue
Playlist::Crawler::DirectoryCrawler::FindNextOp::continue_search()
{
    if(position_->advance(direction_))
        return Continue::WITH_THIS_ITEM;

    /* position didn't move */
    if(direction_ == Direction::NONE)
    {
        /* because we are restricted to process a single item */
        result_.pos_state_ = PositionalState::SOMEWHERE_IN_LIST;
        fill_in_meta_data(result_.meta_data_, file_item_);
        return succeed_here();
    }

    if(directory_depth_ <= 1)
    {
        /* end of top-level directory */
        result_.pos_state_ = is_forward_direction(direction_)
            ? PositionalState::REACHED_END_OF_LIST
            : PositionalState::REACHED_START_OF_LIST;
        fill_in_meta_data(result_.meta_data_, file_item_);
        return succeed_here();
    }

    /* end of nested directory, back to parent */
    unsigned int item_id;
    ID::List list_id;

    try
    {
        std::string list_title;
        list_id = ViewFileBrowser::Utils::get_parent_link_id(dbus_list_,
                                                             dbus_list_.get_list_id(),
                                                             item_id, list_title);
    }
    catch(const List::DBusListException &e)
    {
        /* leave #list_id invalid, fail below */
        msg_error(0, LOG_NOTICE, "Failed going back to parent directory: %s", e.what());
    }

    if(!list_id.is_valid())
        return fail_here();

    entering_list_caller_id_ = List::QueryContextEnterList::CallerID::CRAWLER_ASCEND;
    position_->requested_list_id_ = list_id;
    position_->requested_line_ = item_id;

    switch(dbus_list_.enter_list_async(
                position_->get_viewport().get(), list_id, item_id,
                List::QueryContextEnterList::CallerID::CRAWLER_ASCEND,
                I18n::String(false)))
    {
      case List::AsyncListIface::OpResult::STARTED:
        /*
         * Flow continues in
         * #Playlist::Crawler::DirectoryCrawler::FindNextOp::enter_list_event()
         */
        break;

      case List::AsyncListIface::OpResult::SUCCEEDED:
        BUG("Unexpected result from enter_list_async()");
        return fail_here();

      case List::AsyncListIface::OpResult::FAILED:
        msg_error(0, LOG_NOTICE, "Failed entering parent list");
        return fail_here();

      case List::AsyncListIface::OpResult::CANCELED:
        BUG("Canceled entering parent list");
        log_assert(is_op_canceled());
        break;

      case List::AsyncListIface::OpResult::BUSY:
        MSG_UNREACHABLE();
        break;
    }

    return Continue::LATER;
}

/*!
 * Check if the asynchronous enter-list result matches this op.
 */
bool Playlist::Crawler::DirectoryCrawler::FindNextOp::matches_async_result(
        const List::QueryContextEnterList &ctx,
        List::QueryContextEnterList::CallerID cid) const
{
    if(entering_list_caller_id_ != cid)
        return false;

    if(ctx.parameters_.list_id_ != position_->requested_list_id_)
        return false;

    if(ctx.parameters_.line_ != position_->requested_line_)
        return false;

    return true;
}

static void update_navigation(List::Nav &nav,
                              Playlist::Crawler::Direction direction,
                              unsigned int line)
{
    nav.get_item_filter().list_content_changed();

    const unsigned int lines = nav.get_total_number_of_visible_items();

    if(lines == 0)
        line = 0;
    else if(line >= lines)
        line = is_forward_direction(direction) ? lines - 1 : 0;
    else if(!is_forward_direction(direction))
        line = lines - 1 - line;

    nav.set_cursor_by_line_number(line);
}

/*!
 * Just entered list, running in D-Bus context.
 */
void Playlist::Crawler::DirectoryCrawler::FindNextOp::enter_list_event(
        List::AsyncListIface::OpResult op_result,
        const List::QueryContextEnterList &ctx)
{
    LOGGED_LOCK_CONTEXT_HINT;
    std::lock_guard<LoggedLock::Mutex> lock(lock_);

    const auto cid(ctx.get_caller_id());
    bool has_succeeded = false;

    switch(op_result)
    {
      case List::AsyncListIface::OpResult::SUCCEEDED:
        {
            unsigned int dir_depth = UINT_MAX;

            switch(cid)
            {
              case List::QueryContextEnterList::CallerID::ENTER_ROOT:
              case List::QueryContextEnterList::CallerID::ENTER_CHILD:
              case List::QueryContextEnterList::CallerID::ENTER_PARENT:
              case List::QueryContextEnterList::CallerID::ENTER_CONTEXT_ROOT:
              case List::QueryContextEnterList::CallerID::ENTER_ANYWHERE:
              case List::QueryContextEnterList::CallerID::RELOAD_LIST:
                dir_depth = directory_depth_;
                file_item_ = nullptr;
                break;

              case List::QueryContextEnterList::CallerID::CRAWLER_DESCEND:
                dir_depth = directory_depth_ + 1;
                file_item_ = nullptr;
                break;

              case List::QueryContextEnterList::CallerID::CRAWLER_ASCEND:
                dir_depth = directory_depth_ - 1;
                file_item_ = nullptr;
                break;

              case List::QueryContextEnterList::CallerID::CRAWLER_RESET_POSITION:
              case List::QueryContextEnterList::CallerID::CRAWLER_FIRST_ENTRY:
                dir_depth = directory_depth_;
                break;
            }

            has_succeeded = true;
            const unsigned int line = position_->requested_line_;
            update_navigation(
                position_->nav_,
                cid != List::QueryContextEnterList::CallerID::CRAWLER_ASCEND
                ? direction_
                : Direction::FORWARD,
                line);
            msg_info("Entered list %u at depth %u with %u entries, line %u",
                     dbus_list_.get_list_id().get_raw_id(), dir_depth,
                     position_->nav_.get_total_number_of_visible_items(), line);
        }

        break;

      case List::AsyncListIface::OpResult::FAILED:
        has_succeeded = false;
        break;

      case List::AsyncListIface::OpResult::STARTED:
        /* not interested in this */
        return;

      case List::AsyncListIface::OpResult::CANCELED:
        /* not really interested in this */
        log_assert(is_op_canceled());
        return;

      case List::AsyncListIface::OpResult::BUSY:
        MSG_UNREACHABLE();
        return;
    }

    switch(cid)
    {
      case List::QueryContextEnterList::CallerID::ENTER_ROOT:
      case List::QueryContextEnterList::CallerID::ENTER_CHILD:
      case List::QueryContextEnterList::CallerID::ENTER_PARENT:
      case List::QueryContextEnterList::CallerID::ENTER_CONTEXT_ROOT:
      case List::QueryContextEnterList::CallerID::ENTER_ANYWHERE:
      case List::QueryContextEnterList::CallerID::RELOAD_LIST:
        BUG("Invalid caller ID %d", int(cid));
        operation_finished(false);
        break;

      case List::QueryContextEnterList::CallerID::CRAWLER_RESET_POSITION:
      case List::QueryContextEnterList::CallerID::CRAWLER_FIRST_ENTRY:
        /* first entry into first list */
        log_assert(directory_depth_ == 0 ||
                   cid == List::QueryContextEnterList::CallerID::CRAWLER_RESET_POSITION);
        log_assert(directories_entered_ == 0);
        log_assert(!is_waiting_for_item_hint_);

        if(!has_succeeded)
        {
            operation_finished(false);
            break;
        }

        if(cid == List::QueryContextEnterList::CallerID::CRAWLER_FIRST_ENTRY)
            directory_depth_ = 1;

        directories_entered_ = 1;
        position_->sync_list_id_with_request(directory_depth_);

        if(position_->is_list_empty())
        {
            result_.pos_state_ = is_forward_direction(direction_)
                ? PositionalState::REACHED_END_OF_LIST
                : PositionalState::REACHED_START_OF_LIST;
            fill_in_meta_data(result_.meta_data_, file_item_);
            operation_finished(true);
            break;
        }

        run_as_far_as_possible();
        break;

      case List::QueryContextEnterList::CallerID::CRAWLER_DESCEND:
        if(has_succeeded)
        {
            ++directory_depth_;
            ++directories_entered_;
            has_skipped_first_ = false;
            position_->sync_list_id_with_request(directory_depth_);
        }

        if((!has_succeeded || position_->is_list_empty()) &&
           finish_op_if_possible(continue_search()))
            return;

        run_as_far_as_possible();
        break;

      case List::QueryContextEnterList::CallerID::CRAWLER_ASCEND:
        if(!has_succeeded)
        {
            finish_op_if_possible(fail_here());
            return;
        }

        --directory_depth_;
        has_skipped_first_ = false;
        position_->sync_list_id_with_request(directory_depth_);

        if(position_->is_list_empty())
        {
            /* parent directory cannot be empty, must be an error */
            finish_op_if_possible(fail_here());
            return;
        }

        run_as_far_as_possible();
        break;
    }
}

bool Playlist::Crawler::DirectoryCrawler::FindNextOp::do_start()
{
    if(position_->requested_list_id_ == dbus_list_.get_list_id())
    {
        operation_yield();
        return true;
    }

    switch(find_mode_)
    {
      case FindMode::FIND_FIRST:
        break;

      case FindMode::FIND_NEXT:
        /* we may assume that the list and our cursor are doing fine, so we can
         * continue just like that */
        operation_yield();
        return true;
    }

    /* we have not entered the list yet nor do we have a meaningful cursor,
     * so let's have that sorted out first */
    switch(dbus_list_.enter_list_async(
                position_->get_viewport().get(),
                position_->requested_list_id_, position_->requested_line_,
                entering_list_caller_id_, std::move(root_list_title_)))
    {
      case List::AsyncListIface::OpResult::STARTED:
        /*
         * Flow continues in
         * #Playlist::Crawler::DirectoryCrawler::FindNextOp::enter_list_event()
         */
        return true;

      case List::AsyncListIface::OpResult::SUCCEEDED:
        BUG("Unexpected result from enter_list_async()");
        break;

      case List::AsyncListIface::OpResult::FAILED:
      case List::AsyncListIface::OpResult::CANCELED:
        break;

      case List::AsyncListIface::OpResult::BUSY:
        MSG_UNREACHABLE();
        break;
    }

    return false;
}

void Playlist::Crawler::DirectoryCrawler::FindNextOp::do_continue()
{
    switch(find_mode_)
    {
      case FindMode::FIND_FIRST:
        run_as_far_as_possible();
        break;

      case FindMode::FIND_NEXT:
        if(!finish_op_if_possible(continue_search()))
        {
            has_skipped_first_ = true;
            run_as_far_as_possible();
        }

        break;
    }
}

void Playlist::Crawler::DirectoryCrawler::FindNextOp::do_cancel()
{
    dbus_list_.cancel_all_async_calls();
}

bool Playlist::Crawler::DirectoryCrawler::FindNextOp::do_restart()
{
    result_.clear();
    return false;
}

std::ostream &operator<<(std::ostream &os, Playlist::Crawler::Direction dir)
{
    static const std::array<const char *const, 3> names
    {
        "NONE", "FORWARD", "BACKWARD",
    };
    return dump_enum_value(os, names, "Direction", dir);
}

std::ostream &operator<<(std::ostream &os,
                         Playlist::Crawler::FindNextOpBase::RecursiveMode rm)
{
    static const std::array<const char *const, 2> names
    {
        "FLAT", "DEPTH_FIRST",
    };
    return dump_enum_value(os, names, "RecursiveMode", rm);
}

std::ostream &operator<<(std::ostream &os,
                         Playlist::Crawler::FindNextOpBase::PositionalState ps)
{
    static const std::array<const char *const, 4> names
    {
        "UNKNOWN", "SOMEWHERE_IN_LIST",
        "REACHED_START_OF_LIST", "REACHED_END_OF_LIST",
    };
    return dump_enum_value(os, names, "PositionalState", ps);
}

std::ostream &operator<<(std::ostream &os,
                         Playlist::Crawler::FindNextOpBase::FindMode fm)
{
    static const std::array<const char *const, 2> names
    {
        "FIND_FIRST", "FIND_NEXT",
    };
    return dump_enum_value(os, names, "FindMode", fm);
}

std::string Playlist::Crawler::DirectoryCrawler::FindNextOp::get_short_name() const
{
    std::ostringstream os;
    os << "FindNextOp [" << debug_description_ << "] " << get_state_name();
    return os.str();
}

std::string Playlist::Crawler::DirectoryCrawler::FindNextOp::get_description() const
{
    static const char prefix[] = "\n    FindNextOp: ";
    std::ostringstream os;

    os << "DirectoryCrawler::FindNextOp " << static_cast<const void *>(this)
       << " (tag " << int(tag_)
       << ", caller ID " << int(entering_list_caller_id_) << ")"
       << prefix << debug_description_ << get_base_description(prefix);

    if(position_ != nullptr)
        os << prefix << position_->get_description();
    else
        os << prefix << "No position stored";

    os << prefix << find_mode_
       << ", " << (has_skipped_first_ ? "" : "has not ") << "skipped first item"
       << prefix << (is_waiting_for_item_hint_ ? "Waiting" : "Not waiting")
       << " for item hint"
       << prefix << recursive_mode_ << ", " << direction_
       << ", " << result_.pos_state_
       << ", depth " << directory_depth_
       << prefix << "Skipped " << files_skipped_
       << " files, " << directories_skipped_
       << " directories, entered " << directories_entered_
       << " directories";

    if(position_ != nullptr)
    {
        const auto vp(std::static_pointer_cast<const List::DBusListViewport>(
                                        position_->nav_.get_viewport()));
        const auto &temp(dbus_list_.get_get_range_op_description(*vp));
        if(!temp.empty())
            os << prefix << "GetRangeCallBase " << temp;
    }

    if(file_item_ == nullptr)
        os << prefix << "Have no file item";
    else
        os << prefix << "Have file item: \"" << file_item_->get_text() << "\"";

    return os.str();
}
