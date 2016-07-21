/*
 * Copyright (C) 2016  T+A elektroakustik GmbH & Co. KG
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

#include <functional>
#include <cstring>

#include "directory_crawler.hh"
#include "view_filebrowser_utils.hh"

/*!
 * Invoke callback if avalable.
 *
 * The trick with this function template is not so much the null pointer check,
 * but the fact that it can be called with \p callback passed via \c std::move.
 * This results in the original callback pointer to be set to \p nullptr.
 */
template <typename CBType, typename ResultType>
static void call_callback(CBType callback, Playlist::CrawlerIface &crawler,
                          ResultType result)
{
    if(callback != nullptr)
        callback(crawler, result);
}

bool Playlist::DirectoryCrawler::init()
{
    traversal_list_.register_watcher(
        [this] (List::AsyncListIface::OpEvent event,
                List::AsyncListIface::OpResult result,
                const std::shared_ptr<List::QueryContext_> &ctx)
        {
            switch(event)
            {
              case List::AsyncListIface::OpEvent::ENTER_LIST:
                handle_enter_list_event(result, std::static_pointer_cast<List::QueryContextEnterList>(ctx));
                return;

              case List::AsyncListIface::OpEvent::GET_ITEM:
                handle_get_item_event(result, std::static_pointer_cast<List::QueryContextGetItem>(ctx));
                return;
            }

            BUG("Asynchronous event %u not handled", static_cast<unsigned int>(event));
        });

    return true;
}

bool Playlist::DirectoryCrawler::set_start_position(const List::DBusList &start_list,
                                                    int start_line_number)
{
    user_start_position_.list_id_ = start_list.get_list_id();
    user_start_position_.line_ = start_line_number;

    marked_position_ = user_start_position_;
    marked_position_.directory_depth_ = 1;

    return true;
}

bool Playlist::DirectoryCrawler::restart()
{
    directory_depth_ = 0;
    is_first_item_in_list_processed_ = false;
    is_waiting_for_async_enter_list_completion_ = false;
    is_waiting_for_async_get_list_item_completion_ = false;
    current_item_info_.clear();
    marked_position_ = user_start_position_;
    marked_position_.directory_depth_ = 1;

    switch(traversal_list_.enter_list_async(user_start_position_.list_id_,
                                            user_start_position_.line_,
                                            List::QueryContextEnterList::CallerID::CRAWLER_RESTART))
    {
      case List::AsyncListIface::OpResult::STARTED:
      case List::AsyncListIface::OpResult::SUCCEEDED:
        return true;

      case List::AsyncListIface::OpResult::FAILED:
      case List::AsyncListIface::OpResult::CANCELED:
        msg_error(0, LOG_NOTICE, "Failed entering list for playback");
        break;
    }

    return false;
}

bool Playlist::DirectoryCrawler::list_invalidate(ID::List list_id, ID::List replacement_id)
{
    log_assert(list_id.is_valid());

    if(!user_start_position_.list_id_.is_valid())
        return false;

    if(user_start_position_.list_id_ == list_id)
    {
        if(replacement_id.is_valid())
            user_start_position_.list_id_ = replacement_id;
        else
            return true;
    }

    if(user_start_position_.list_id_ == list_id)
        return true;

    if(traversal_list_.get_list_id() == list_id)
        return true;

    return false;
}

bool Playlist::DirectoryCrawler::retrieve_item_information_impl(RetrieveItemInfoCallback callback)
{
    retrieve_item_info_callback_ = nullptr;

    if(!is_waiting_for_async_enter_list_completion_)
        return do_retrieve_item_information(callback);
    else
    {
        BUG("Ignoring get-item-info command while entering list");
        return false;
    }
}

static List::AsyncListIface::OpResult
set_dbuslist_hint(List::DBusList &list, List::Nav &nav, bool forward,
                  List::QueryContextGetItem::CallerID caller_id)
{
    const unsigned int total_list_size = nav.get_total_number_of_visible_items();
    const unsigned int &hint_count = nav.maximum_number_of_displayed_lines_;
    log_assert(hint_count > 0);

    if(total_list_size <= hint_count)
        return list.get_item_async_set_hint(0, total_list_size, caller_id);

    const auto cursor_pos = nav.get_cursor();
    unsigned int start_pos;

    if(forward)
        start_pos = ((cursor_pos + hint_count <= total_list_size)
                     ? cursor_pos
                     : total_list_size - hint_count);
    else
        start_pos = ((cursor_pos >= hint_count)
                     ? cursor_pos - (hint_count - 1)
                     : 0);

    return list.get_item_async_set_hint(start_pos, hint_count, caller_id);
}

void Playlist::DirectoryCrawler::mark_current_position()
{
    mark_position(traversal_list_.get_list_id(), navigation_.get_cursor(),
                  directory_depth_);
}

void Playlist::DirectoryCrawler::switch_direction()
{
    traversal_list_.cancel_async();
    log_assert(!is_waiting_for_async_enter_list_completion_);
    log_assert(!is_waiting_for_async_get_list_item_completion_);

    if(!marked_position_.list_id_.is_valid())
        return;

    if(traversal_list_.get_list_id() != marked_position_.list_id_)
    {
        switch(traversal_list_.enter_list_async(marked_position_.list_id_,
                                                marked_position_.line_,
                                                List::QueryContextEnterList::CallerID::CRAWLER_RESET_POSITION))
        {
          case List::AsyncListIface::OpResult::STARTED:
          case List::AsyncListIface::OpResult::SUCCEEDED:
            break;

          case List::AsyncListIface::OpResult::FAILED:
          case List::AsyncListIface::OpResult::CANCELED:
            msg_error(0, LOG_NOTICE, "Failed reseting crawler position due to direction switch");
            break;
        }

        return;
    }

    if(navigation_.get_cursor() != marked_position_.line_)
    {
        navigation_.set_cursor_by_line_number(marked_position_.line_);

        set_dbuslist_hint(traversal_list_, navigation_, is_crawling_forward(),
                          List::QueryContextGetItem::CallerID::CRAWLER_FIND_NEXT);
    }
}

static Playlist::CrawlerIface::FindNext
map_opresult_to_find_next_result(const List::AsyncListIface::OpResult op_result)
{
    switch(op_result)
    {
      case List::AsyncListIface::OpResult::STARTED:
        break;

      case List::AsyncListIface::OpResult::SUCCEEDED:
        return Playlist::CrawlerIface::FindNext::FOUND;

      case List::AsyncListIface::OpResult::FAILED:
        return Playlist::CrawlerIface::FindNext::FAILED;

      case List::AsyncListIface::OpResult::CANCELED:
        return Playlist::CrawlerIface::FindNext::CANCELED;
    }

    return Playlist::CrawlerIface::FindNext::FAILED;
}

static Playlist::CrawlerIface::RetrieveItemInfo
map_asyncresult_to_retrieve_item_result(const DBus::AsyncResult async_result)
{
    switch(async_result)
    {
      case DBus::AsyncResult::INITIALIZED:
      case DBus::AsyncResult::IN_PROGRESS:
      case DBus::AsyncResult::READY:
      case DBus::AsyncResult::FAILED:
        break;

      case DBus::AsyncResult::DONE:
        return Playlist::CrawlerIface::RetrieveItemInfo::FOUND;

      case DBus::AsyncResult::CANCELED:
        return Playlist::CrawlerIface::RetrieveItemInfo::CANCELED;
    }

    return Playlist::CrawlerIface::RetrieveItemInfo::FAILED;
}

bool Playlist::DirectoryCrawler::try_get_dbuslist_item_after_started_or_successful_hint(const FindNextCallback &callback)
{
    find_next_callback_ = nullptr;

    const List::Item *item;
    auto op_result(traversal_list_.get_item_async(navigation_.get_cursor(), item));

    switch(op_result)
    {
      case List::AsyncListIface::OpResult::STARTED:
        /* getting in background, have the get-item-done handler deal with this
         * item when it is actually available */
        find_next_callback_ = callback;
        return false;

      case List::AsyncListIface::OpResult::SUCCEEDED:
      case List::AsyncListIface::OpResult::FAILED:
      case List::AsyncListIface::OpResult::CANCELED:
        /* already have it or failed early, do as the handler would do */
        switch(process_current_ready_item(dynamic_cast<const ViewFileBrowser::FileItem *>(item),
                                          op_result, callback, false))
        {
          case RecurseResult::FOUND_ITEM:
            /* we have a non-directory item right here */
            break;

          case RecurseResult::ASYNC_IN_PROGRESS:
          case RecurseResult::SKIP:
            return false;

          case RecurseResult::ASYNC_DONE:
            /* so, we had an item which was not a directory, attempted to enter
             * it, \e and succeeded immediately */
            return true;

          case RecurseResult::ERROR:
            op_result = List::AsyncListIface::OpResult::FAILED;
            break;
        }

        break;
    }

    if(callback != nullptr)
        callback(*this, map_opresult_to_find_next_result(op_result));

    return false;
}

/*!
 * Start retrieving URIs associated with selected item.
 *
 * \bug This function only returns a single URI. See tickets #13 and #285.
 */
static std::shared_ptr<Playlist::DirectoryCrawler::AsyncGetURIs>
mk_async_get_uris(tdbuslistsNavigation *proxy,
                  const DBus::AsyncResultAvailableFunction &result_available_fn)
{
    return std::make_shared<Playlist::DirectoryCrawler::AsyncGetURIs>(
        proxy,
        [] (GObject *source_object) { return TDBUS_LISTS_NAVIGATION(source_object); },
        [] (DBus::AsyncResult &async_ready,
            Playlist::DirectoryCrawler::AsyncGetURIs::PromiseType &promise,
            tdbuslistsNavigation *p, GAsyncResult *async_result, GError *&error)
        {
            guchar error_code = 0;
            gchar **uri_list = NULL;

            async_ready =
                tdbus_lists_navigation_call_get_uris_finish(p, &error_code, &uri_list,
                                                            async_result, &error)
                ? DBus::AsyncResult::READY
                : DBus::AsyncResult::FAILED;

            if(async_ready == DBus::AsyncResult::FAILED)
                msg_error(0, LOG_NOTICE,
                          "Async D-Bus method call failed: %s",
                          error != nullptr ? error->message : "*NULL*");

            promise.set_value(std::move(std::make_tuple(error_code, uri_list)));
        },
        result_available_fn,
        [] (Playlist::DirectoryCrawler::AsyncGetURIs::PromiseReturnType &values)
        {
            gchar **const strings = std::get<1>(values);

            if(strings == NULL)
                return;

            for(gchar **ptr = strings; *ptr != NULL; ++ptr)
                g_free(*ptr);

            g_free(strings);
        },
        [] () { return true; });
}

static void update_navigation(List::Nav &nav, List::NavItemFilterIface &item_filter,
                              bool forward, unsigned int line)
{
    item_filter.list_content_changed();

    const unsigned int lines = nav.get_total_number_of_visible_items();

    if(lines == 0)
        line = 0;
    else if(forward)
    {
        if(line >= lines)
            line = lines - 1;
    }
    else
    {
        if(line >= lines)
            line = 0;
        else
            line = lines - 1 - line;
    }

    nav.set_cursor_by_line_number(line);
}

void Playlist::DirectoryCrawler::handle_entered_list(unsigned int line,
                                                     bool continue_if_empty)
{
    ++directory_depth_;
    is_first_item_in_list_processed_ = false;

    update_navigation(navigation_, item_flags_, is_crawling_forward(), line);
    msg_info("Entered list %u at depth %u with %u entries, line %u",
             traversal_list_.get_list_id().get_raw_id(), directory_depth_,
             navigation_.get_total_number_of_visible_items(), line);

    if(navigation_.get_total_number_of_visible_items() == 0)
    {
        if(continue_if_empty)
        {
            /* found empty directory, go up again */
            if(!find_next(std::move(find_next_callback_)))
                fail_crawler();
        }
        else
        {
            msg_error(0, LOG_NOTICE, "Cannot play empty directory");
            fail_crawler();
        }
    }
    else if(find_next_callback_ != nullptr)
    {
        /* have pending find-next request */
        if(!find_next(std::move(find_next_callback_)))
            fail_crawler();
    }
}

Playlist::DirectoryCrawler::RecurseResult
Playlist::DirectoryCrawler::try_descend(const FindNextCallback &callback)
{
    switch(recursive_mode_)
    {
      case RecursiveMode::FLAT:
        return RecurseResult::SKIP;

      case RecursiveMode::DEPTH_FIRST:
        break;
    }

    if(directory_depth_ >= MAX_DIRECTORY_DEPTH)
    {
        msg_error(0, LOG_NOTICE,
                  "Maximum directory depth of %u reached, not going down any further",
                  MAX_DIRECTORY_DEPTH);
        return RecurseResult::SKIP;
    }

    const ID::List list_id =
        ViewFileBrowser::Utils::get_child_item_id(traversal_list_,
                                                  traversal_list_.get_list_id(),
                                                  navigation_, nullptr, true);

    if(!list_id.is_valid())
        return RecurseResult::ERROR;

    static constexpr const unsigned int line = 0;

    find_next_callback_ = callback;

    switch(traversal_list_.enter_list_async(list_id, line,
                                            List::QueryContextEnterList::CallerID::CRAWLER_DESCEND))
    {
      case List::AsyncListIface::OpResult::STARTED:
        return RecurseResult::ASYNC_IN_PROGRESS;

      case List::AsyncListIface::OpResult::SUCCEEDED:
        handle_entered_list(line, true);
        return RecurseResult::ASYNC_DONE;

      case List::AsyncListIface::OpResult::FAILED:
      case List::AsyncListIface::OpResult::CANCELED:
        msg_error(0, LOG_NOTICE, "Failed entering child list");
        break;
    }

    find_next_callback_ = nullptr;

    return RecurseResult::ERROR;
}

Playlist::DirectoryCrawler::RecurseResult
Playlist::DirectoryCrawler::process_current_ready_item(const ViewFileBrowser::FileItem *file_item,
                                                       List::AsyncListIface::OpResult op_result,
                                                       const FindNextCallback &callback,
                                                       bool expecting_file_item)
{
    switch(op_result)
    {
      case List::AsyncListIface::OpResult::STARTED:
        BUG("Expected item %u in list %u to be available",
            navigation_.get_cursor(), traversal_list_.get_list_id().get_raw_id());
        break;

      case List::AsyncListIface::OpResult::SUCCEEDED:
        log_assert(file_item != nullptr);

        if(!file_item->get_kind().is_directory())
            return RecurseResult::FOUND_ITEM;

        if(expecting_file_item)
        {
            msg_info("Found directory \"%s\", expected file (skipping)", file_item->get_text());
            return RecurseResult::SKIP;
        }
        else
        {
            msg_info("Found directory \"%s\", entering", file_item->get_text());
            const auto result = try_descend(callback);

            switch(result)
            {
              case RecurseResult::FOUND_ITEM:
                BUG("Unexpected result from try_descend()");
                fail_crawler();
                break;

              case RecurseResult::ASYNC_IN_PROGRESS:
                break;

              case RecurseResult::ASYNC_DONE:
                break;

              case RecurseResult::SKIP:
                if(!find_next(callback))
                    fail_crawler();

                break;

              case RecurseResult::ERROR:
                fail_crawler();
                break;
            }

            if(get_crawler_state() == CrawlerState::CRAWLING)
                return result;
        }

        break;

      case List::AsyncListIface::OpResult::FAILED:
      case List::AsyncListIface::OpResult::CANCELED:
        break;
    }

    return RecurseResult::ERROR;
}

bool Playlist::DirectoryCrawler::do_retrieve_item_information(const RetrieveItemInfoCallback &callback)
{
    const List::Item *item;
    const auto op_result(traversal_list_.get_item_async(navigation_.get_cursor(), item));
    const auto find_next_callback(std::move(find_next_callback_));

    switch(process_current_ready_item(dynamic_cast<const ViewFileBrowser::FileItem *>(item),
                                      op_result, find_next_callback, true))
    {
      case RecurseResult::FOUND_ITEM:
        msg_info("Retrieving URIs for item \"%s\"",
                 dynamic_cast<const ViewFileBrowser::FileItem *>(item)->get_text());
        retrieve_item_info_callback_ = callback;
        async_get_uris_call_ =
            mk_async_get_uris(dbus_proxy_,
                              std::bind(&Playlist::DirectoryCrawler::process_item_information,
                                        this, std::placeholders::_1,
                                        traversal_list_.get_list_id(),
                                        navigation_.get_cursor(),
                                        directory_depth_));

        if(async_get_uris_call_ != nullptr)
        {
            async_get_uris_call_->invoke(tdbus_lists_navigation_call_get_uris,
                                         traversal_list_.get_list_id().get_raw_id(),
                                         navigation_.get_cursor());
            return true;
        }

        msg_out_of_memory("asynchronous D-Bus call");
        retrieve_item_info_callback_ = nullptr;
        break;

      case RecurseResult::ERROR:
        break;

      case RecurseResult::ASYNC_IN_PROGRESS:
      case RecurseResult::ASYNC_DONE:
      case RecurseResult::SKIP:
        BUG("Unexpected result for ready item");
        break;
    }

    call_callback(callback, *this, RetrieveItemInfo::FAILED);

    return false;
}

void Playlist::DirectoryCrawler::process_item_information(const DBus::AsyncCall_ &async_call,
                                                          ID::List list_id, unsigned int line,
                                                          unsigned int directory_depth)
{
    auto lock_this(lock());

    if(&async_call != async_get_uris_call_.get())
    {
        call_callback(std::move(retrieve_item_info_callback_), *this,
                      RetrieveItemInfo::CANCELED);
        return;
    }

    DBus::AsyncResult async_result;

    try
    {
        async_result = async_get_uris_call_->wait_for_result();
    }
    catch(const List::DBusListException &e)
    {
        async_result = DBus::AsyncResult::FAILED;
        msg_error(0, LOG_ERR, "Failed stream URIs for item %u in list %u: %s",
                  line, list_id.get_raw_id(), e.what());
    }

    if(!async_get_uris_call_->success() ||
       async_result != DBus::AsyncResult::DONE)
    {
        call_callback(std::move(retrieve_item_info_callback_), *this,
                      map_asyncresult_to_retrieve_item_result(async_result));
        async_get_uris_call_.reset();
        return;
    }

    const auto &result(async_get_uris_call_->get_result(async_result));
    const ListError error(std::get<0>(result));

    if(error != ListError::Code::OK)
    {
        msg_error(0, LOG_NOTICE,
                  "Got error %s instead of URIs for item %u in list %u",
                  error.to_string(), line, list_id.get_raw_id());
        call_callback(std::move(retrieve_item_info_callback_), *this,
                      RetrieveItemInfo::FAILED);
        return;
    }

    current_item_info_.set(list_id, line, directory_depth,
                           dynamic_cast<const ViewFileBrowser::FileItem *>(get_current_list_item_impl()));

    if(!current_item_info_.position_.list_id_.is_valid() ||
       current_item_info_.file_item_ == nullptr)
    {
        BUG("Invalid item, retrieving item info failed");
        call_callback(std::move(retrieve_item_info_callback_), *this,
                      RetrieveItemInfo::FAILED);
        return;
    }

    gchar **const uri_list(std::get<1>(result));

    if(uri_list != NULL)
    {
        for(gchar **ptr = uri_list; *ptr != NULL; ++ptr)
            msg_info("URI: \"%s\"", *ptr);

        for(gchar **ptr = uri_list; *ptr != NULL; ++ptr)
        {
            const size_t len = strlen(*ptr);

            if(len < 4)
                continue;

            const gchar *const suffix = &(*ptr)[len - 4];

            if(strncasecmp(".m3u", suffix, 4) == 0 ||
               strncasecmp(".pls", suffix, 4) == 0)
                continue;

            current_item_info_.stream_uris_.push_back(*ptr);
        }

    }

    if(current_item_info_.stream_uris_.empty())
        msg_info("No URI for item %u in list %u", line, list_id.get_raw_id());

    call_callback(std::move(retrieve_item_info_callback_), *this,
                  RetrieveItemInfo::FOUND);
}

void Playlist::DirectoryCrawler::handle_end_of_list(const FindNextCallback &callback)
{
    current_item_info_.clear();

    call_callback(callback, *this,
                  is_crawling_forward() ? FindNext::END_OF_LIST : FindNext::START_OF_LIST);
}

static List::AsyncListIface::OpResult back_to_parent(List::DBusList &list,
                                                     unsigned int directory_depth)
{
    if(directory_depth <= 1)
        return List::AsyncListIface::OpResult::CANCELED;

    unsigned int item_id;
    const ID::List list_id =
        ViewFileBrowser::Utils::get_parent_link_id(list, list.get_list_id(), item_id);

    if(!list_id.is_valid())
        return List::AsyncListIface::OpResult::FAILED;

    const auto result =
        list.enter_list_async(list_id, item_id,
                              List::QueryContextEnterList::CallerID::CRAWLER_ASCEND);

    switch(result)
    {
      case List::AsyncListIface::OpResult::STARTED:
      case List::AsyncListIface::OpResult::SUCCEEDED:
        break;

      case List::AsyncListIface::OpResult::FAILED:
      case List::AsyncListIface::OpResult::CANCELED:
        msg_error(0, LOG_NOTICE, "Failed entering parent list");
        break;
    }

    return result;
}

bool Playlist::DirectoryCrawler::find_next_impl(FindNextCallback callback)
{
    find_next_callback_ = nullptr;

    bool looping = true;

    while(looping)
    {
        msg_info("Find next %s, depth %u, wait enter %d, wait item %d, first is %sprocessed",
                 is_crawling_forward() ? "forward" : "backward",
                 directory_depth_,
                 is_waiting_for_async_enter_list_completion_,
                 is_waiting_for_async_get_list_item_completion_,
                 is_first_item_in_list_processed_ ? "" : "not ");
        looping = false;

        if(is_waiting_for_async_enter_list_completion_)
        {
            /* let the enter-list-done handler deal with this */
            find_next_callback_ = callback;
            break;
        }

        if(!is_first_item_in_list_processed_)
        {
            if(navigation_.get_total_number_of_visible_items() == 0)
            {
                handle_end_of_list(callback);
                return false;
            }
        }
        else if(!go_to_next_list_item())
        {
            if(directory_depth_ <= 1)
            {
                handle_end_of_list(callback);
                return false;
            }

            switch(back_to_parent(traversal_list_, directory_depth_))
            {
              case List::AsyncListIface::OpResult::STARTED:
                return true;

              case List::AsyncListIface::OpResult::SUCCEEDED:
                break;

              case List::AsyncListIface::OpResult::FAILED:
              case List::AsyncListIface::OpResult::CANCELED:
                fail_crawler();
                return false;
            }
        }

        const auto result =
            is_waiting_for_async_get_list_item_completion_
            ? List::AsyncListIface::OpResult::STARTED
            : set_dbuslist_hint(traversal_list_, navigation_, is_crawling_forward(),
                                List::QueryContextGetItem::CallerID::CRAWLER_FIND_NEXT);

        switch(result)
        {
          case List::AsyncListIface::OpResult::STARTED:
          case List::AsyncListIface::OpResult::SUCCEEDED:
            looping = try_get_dbuslist_item_after_started_or_successful_hint(callback);
            break;

          case List::AsyncListIface::OpResult::FAILED:
            call_callback(callback, *this, FindNext::FAILED);
            return false;

          case List::AsyncListIface::OpResult::CANCELED:
            BUG("Unexpected canceled result");
            return false;
        }
    }

    return true;
}

const List::Item *Playlist::DirectoryCrawler::get_current_list_item_impl()
{
    const List::Item *item;

    if(traversal_list_.get_item_async(navigation_.get_cursor(), item) == List::AsyncListIface::OpResult::SUCCEEDED)
        return item;
    else
        return nullptr;
}

void Playlist::DirectoryCrawler::handle_enter_list_event(List::AsyncListIface::OpResult result,
                                                         const std::shared_ptr<List::QueryContextEnterList> &ctx)
{
    auto lock_this(lock());

    is_waiting_for_async_enter_list_completion_ =
        (result == List::AsyncListIface::OpResult::STARTED);

    switch(ctx->get_caller_id())
    {
      case List::QueryContextEnterList::CallerID::SYNC_WRAPPER:
      case List::QueryContextEnterList::CallerID::ENTER_ROOT:
      case List::QueryContextEnterList::CallerID::ENTER_CHILD:
      case List::QueryContextEnterList::CallerID::ENTER_PARENT:
      case List::QueryContextEnterList::CallerID::RELOAD_LIST:
        BUG("Wrong caller ID in %s()", __PRETTY_FUNCTION__);
        break;

      case List::QueryContextEnterList::CallerID::CRAWLER_RESTART:
        switch(result)
        {
          case List::AsyncListIface::OpResult::STARTED:
            break;

          case List::AsyncListIface::OpResult::SUCCEEDED:
            log_assert(directory_depth_ == 0);
            handle_entered_list(ctx->parameters_.line_, false);
            break;

          case List::AsyncListIface::OpResult::FAILED:
          case List::AsyncListIface::OpResult::CANCELED:
            fail_crawler();
            break;
        }

        break;

      case List::QueryContextEnterList::CallerID::CRAWLER_RESET_POSITION:
        switch(result)
        {
          case List::AsyncListIface::OpResult::STARTED:
            break;

          case List::AsyncListIface::OpResult::SUCCEEDED:
            log_assert(marked_position_.list_id_ == traversal_list_.get_list_id());
            log_assert(marked_position_.line_ == ctx->parameters_.line_);

            directory_depth_ = marked_position_.directory_depth_ - 1;

            handle_entered_list(ctx->parameters_.line_, false);
            break;

          case List::AsyncListIface::OpResult::FAILED:
          case List::AsyncListIface::OpResult::CANCELED:
            fail_crawler();
            break;
        }

        break;

      case List::QueryContextEnterList::CallerID::CRAWLER_DESCEND:
        switch(result)
        {
          case List::AsyncListIface::OpResult::STARTED:
            break;

          case List::AsyncListIface::OpResult::SUCCEEDED:
            handle_entered_list(ctx->parameters_.line_, true);
            break;

          case List::AsyncListIface::OpResult::FAILED:
            if(!find_next(std::move(find_next_callback_)))
                fail_crawler();

            break;

          case List::AsyncListIface::OpResult::CANCELED:
            break;
        }

        break;

      case List::QueryContextEnterList::CallerID::CRAWLER_ASCEND:
        switch(result)
        {
          case List::AsyncListIface::OpResult::STARTED:
            break;

          case List::AsyncListIface::OpResult::SUCCEEDED:
            --directory_depth_;
            is_first_item_in_list_processed_ = true;

            update_navigation(navigation_, item_flags_, is_crawling_forward(),
                              ctx->parameters_.line_);

            if(navigation_.get_total_number_of_visible_items() == 0)
            {
                /* parent directory cannot be empty, must be an error */
                fail_crawler();
            }

            if(!find_next(std::move(find_next_callback_)))
                fail_crawler();

            break;

          case List::AsyncListIface::OpResult::FAILED:
            fail_crawler();
            break;

          case List::AsyncListIface::OpResult::CANCELED:
            break;
        }

        break;
    }
}

void Playlist::DirectoryCrawler::handle_get_item_event(List::AsyncListIface::OpResult result,
                                                       const std::shared_ptr<List::QueryContextGetItem> &ctx)
{
    auto lock_this(lock());

    is_waiting_for_async_get_list_item_completion_ =
        (result == List::AsyncListIface::OpResult::STARTED);

    switch(ctx->get_caller_id())
    {
      case List::QueryContextGetItem::CallerID::SERIALIZE:
      case List::QueryContextGetItem::CallerID::SERIALIZE_DEBUG:
      case List::QueryContextGetItem::CallerID::DBUSLIST_GET_ITEM:
        BUG("Wrong caller ID in %s()", __PRETTY_FUNCTION__);
        break;

      case List::QueryContextGetItem::CallerID::CRAWLER_FIND_NEXT:
        switch(result)
        {
          case List::AsyncListIface::OpResult::STARTED:
            current_item_info_.clear();
            break;

          case List::AsyncListIface::OpResult::SUCCEEDED:
          case List::AsyncListIface::OpResult::FAILED:
          case List::AsyncListIface::OpResult::CANCELED:
            if(find_next_callback_ != nullptr)
            {
                is_first_item_in_list_processed_ = true;

                const List::Item *item;
                auto op_result(traversal_list_.get_item_async(navigation_.get_cursor(), item));
                const auto find_next_callback(std::move(find_next_callback_));

                switch(process_current_ready_item(dynamic_cast<const ViewFileBrowser::FileItem *>(item),
                                                  op_result, find_next_callback, false))
                {
                  case RecurseResult::FOUND_ITEM:
                    break;

                  case RecurseResult::ASYNC_IN_PROGRESS:
                  case RecurseResult::SKIP:
                    return;

                  case RecurseResult::ASYNC_DONE:
                    find_next(find_next_callback);
                    return;

                  case RecurseResult::ERROR:
                    result = List::AsyncListIface::OpResult::FAILED;
                    break;
                }

                call_callback(find_next_callback, *this,
                              map_opresult_to_find_next_result(result));
            }

            break;
        }

        break;
    }
}
