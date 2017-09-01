/*
 * Copyright (C) 2016, 2017  T+A elektroakustik GmbH & Co. KG
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
#include "view_filebrowser_fileitem.hh"
#include "view_filebrowser_utils.hh"

template <typename T>
static inline const T pass_on(T &original_object)
{
    const auto obj_copy(original_object);

    original_object = nullptr;

    return obj_copy;
}

/*!
 * Invoke callback if avalable.
 *
 * The trick with this function template is not so much the null pointer check,
 * but the fact that it can be called with \p callback passed via #pass_on().
 * This results in the original callback pointer to be set to \p nullptr.
 */
template <typename CBType, typename... Args>
static typename CBType::result_type
call_callback(CBType callback, Playlist::CrawlerIface &crawler, Args... args)
{
    return (callback != nullptr)
        ? callback(crawler, args...)
        : static_cast<typename CBType::result_type>(0);
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
    user_start_position_.set(start_list.get_list_id(), start_line_number);
    marked_position_ = user_start_position_;
    return true;
}

bool Playlist::DirectoryCrawler::is_busy_impl() const
{
    return is_waiting_for_async_enter_list_completion_ ||
           is_waiting_for_async_get_list_item_completion_ ||
           is_resetting_to_marked_position_ ||
           find_next_callback_ != nullptr;
}

bool Playlist::DirectoryCrawler::restart()
{
    directory_depth_ = 0;
    is_first_item_in_list_processed_ = false;
    is_waiting_for_async_enter_list_completion_ = false;
    is_waiting_for_async_get_list_item_completion_ = false;
    current_item_info_.clear();
    marked_position_ = user_start_position_;

    static constexpr auto cid(List::QueryContextEnterList::CallerID::CRAWLER_RESTART);
    const auto result =
        traversal_list_.enter_list_async(user_start_position_.get_list_id(),
                                         user_start_position_.get_line(), cid);

    switch(result)
    {
      case List::AsyncListIface::OpResult::STARTED:
      case List::AsyncListIface::OpResult::SUCCEEDED:
        return true;

      case List::AsyncListIface::OpResult::FAILED:
      case List::AsyncListIface::OpResult::CANCELED:
        msg_error(0, LOG_NOTICE, "Failed entering list for playback");
        call_callback(failure_callback_enter_list_, *this, cid, result);
        break;
    }

    return false;
}

bool Playlist::DirectoryCrawler::list_invalidate(ID::List list_id, ID::List replacement_id)
{
    log_assert(list_id.is_valid());

    /* nothing to do in passive mode */
    if(!user_start_position_.get_list_id().is_valid())
        return false;

    /* do it and check validity in active mode */
    return user_start_position_.list_invalidate(list_id, replacement_id)
        ? !replacement_id.is_valid()
        : traversal_list_.get_list_id() == list_id;
}

bool Playlist::DirectoryCrawler::retrieve_item_information_impl(RetrieveItemInfoCallback callback)
{
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
                  directory_depth_, get_active_direction());
}

bool Playlist::DirectoryCrawler::set_direction_from_marked_position()
{
    switch(marked_position_.get_arived_direction())
    {
      case Direction::NONE:
      case Direction::FORWARD:
        break;

      case Direction::BACKWARD:
        return set_direction_backward();
    }

    return set_direction_forward();
}

void Playlist::DirectoryCrawler::switch_direction()
{
    if(!traversal_list_.cancel_all_async_calls())
        return;

    is_waiting_for_async_enter_list_completion_ = false;
    is_waiting_for_async_get_list_item_completion_ = false;

    if(!marked_position_.get_list_id().is_valid())
        return;

    if(traversal_list_.get_list_id() != marked_position_.get_list_id())
    {
        static constexpr auto cid(List::QueryContextEnterList::CallerID::CRAWLER_RESET_POSITION);
        const auto result =
            traversal_list_.enter_list_async(marked_position_.get_list_id(),
                                             marked_position_.get_line(), cid);

        switch(result)
        {
          case List::AsyncListIface::OpResult::STARTED:
          case List::AsyncListIface::OpResult::SUCCEEDED:
            break;

          case List::AsyncListIface::OpResult::FAILED:
          case List::AsyncListIface::OpResult::CANCELED:
            msg_error(0, LOG_NOTICE, "Failed resetting crawler position due to direction switch");
            call_callback(failure_callback_enter_list_, *this, cid, result);
            break;
        }

        return;
    }

    if(navigation_.get_cursor() != marked_position_.get_line())
    {
        navigation_.set_cursor_by_line_number(marked_position_.get_line());
        set_dbuslist_hint(traversal_list_, navigation_, is_crawling_forward(),
                          List::QueryContextGetItem::CallerID::CRAWLER_FIND_MARKED);
    }
}

static Playlist::CrawlerIface::FindNextItemResult
map_opresult_to_find_next_result(const List::AsyncListIface::OpResult &op_result)
{
    switch(op_result)
    {
      case List::AsyncListIface::OpResult::STARTED:
        break;

      case List::AsyncListIface::OpResult::SUCCEEDED:
        return Playlist::CrawlerIface::FindNextItemResult::FOUND;

      case List::AsyncListIface::OpResult::FAILED:
        return Playlist::CrawlerIface::FindNextItemResult::FAILED;

      case List::AsyncListIface::OpResult::CANCELED:
        return Playlist::CrawlerIface::FindNextItemResult::CANCELED;
    }

    return Playlist::CrawlerIface::FindNextItemResult::FAILED;
}

static Playlist::CrawlerIface::RetrieveItemInfoResult
map_asyncresult_to_retrieve_item_result(const DBus::AsyncResult &async_result)
{
    switch(async_result)
    {
      case DBus::AsyncResult::INITIALIZED:
      case DBus::AsyncResult::IN_PROGRESS:
      case DBus::AsyncResult::READY:
      case DBus::AsyncResult::FAILED:
        break;

      case DBus::AsyncResult::DONE:
        return Playlist::CrawlerIface::RetrieveItemInfoResult::FOUND;

      case DBus::AsyncResult::CANCELED:
      case DBus::AsyncResult::RESTARTED:
        return Playlist::CrawlerIface::RetrieveItemInfoResult::CANCELED;
    }

    return Playlist::CrawlerIface::RetrieveItemInfoResult::FAILED;
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

          case RecurseResult::ASYNC_CANCELED:
            op_result = List::AsyncListIface::OpResult::CANCELED;
            break;

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
 */
static std::shared_ptr<Playlist::DirectoryCrawler::AsyncGetURIs>
mk_async_get_uris(tdbuslistsNavigation *proxy,
                  DBus::AsyncResultAvailableFunction &&result_available_fn)
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
            GVariant *image_stream_key = NULL;

            async_ready =
                tdbus_lists_navigation_call_get_uris_finish(p, &error_code, &uri_list,
                                                            &image_stream_key,
                                                            async_result, &error)
                ? DBus::AsyncResult::READY
                : DBus::AsyncResult::FAILED;

            if(async_ready == DBus::AsyncResult::FAILED)
                msg_error(0, LOG_NOTICE,
                          "Async D-Bus method call failed: %s",
                          error != nullptr ? error->message : "*NULL*");

            promise.set_value(std::move(std::make_tuple(error_code, uri_list,
                                                        std::move(GVariantWrapper(image_stream_key,
                                                                                  GVariantWrapper::Transfer::JUST_MOVE)))));
        },
        std::move(result_available_fn),
        [] (Playlist::DirectoryCrawler::AsyncGetURIs::PromiseReturnType &values)
        {
            gchar **const strings = std::get<1>(values);

            if(strings == NULL)
                return;

            for(gchar **ptr = strings; *ptr != NULL; ++ptr)
                g_free(*ptr);

            g_free(strings);
        },
        [] () { return true; },
        "AsyncGetURIs", MESSAGE_LEVEL_DEBUG);
}

/*!
 * Start retrieving ranked stream links associated with selected item.
 */
static std::shared_ptr<Playlist::DirectoryCrawler::AsyncGetStreamLinks>
mk_async_get_stream_links(tdbuslistsNavigation *proxy,
                          DBus::AsyncResultAvailableFunction &&result_available_fn)
{
    return std::make_shared<Playlist::DirectoryCrawler::AsyncGetStreamLinks>(
        proxy,
        [] (GObject *source_object) { return TDBUS_LISTS_NAVIGATION(source_object); },
        [] (DBus::AsyncResult &async_ready,
            Playlist::DirectoryCrawler::AsyncGetStreamLinks::PromiseType &promise,
            tdbuslistsNavigation *p, GAsyncResult *async_result, GError *&error)
        {
            guchar error_code = 0;
            GVariant *link_list = NULL;
            GVariant *image_stream_key = NULL;

            async_ready =
                tdbus_lists_navigation_call_get_ranked_stream_links_finish(
                    p, &error_code, &link_list, &image_stream_key,
                    async_result, &error)
                ? DBus::AsyncResult::READY
                : DBus::AsyncResult::FAILED;

            if(async_ready == DBus::AsyncResult::FAILED)
                msg_error(0, LOG_NOTICE,
                          "Async D-Bus method call failed: %s",
                          error != nullptr ? error->message : "*NULL*");

            promise.set_value(std::move(std::make_tuple(error_code,
                                                        std::move(GVariantWrapper(link_list,
                                                                                  GVariantWrapper::Transfer::JUST_MOVE)),
                                                        std::move(GVariantWrapper(image_stream_key,
                                                                                  GVariantWrapper::Transfer::JUST_MOVE)))));
        },
        std::move(result_available_fn),
        [] (Playlist::DirectoryCrawler::AsyncGetStreamLinks::PromiseReturnType &values) {},
        [] () { return true; },
        "AsyncGetStreamLinks", MESSAGE_LEVEL_DEBUG);
}

static void update_navigation(List::Nav &nav, List::NavItemFilterIface &item_filter,
                              bool forward, unsigned int line)
{
    item_filter.list_content_changed();

    const unsigned int lines = nav.get_total_number_of_visible_items();

    if(lines == 0)
        line = 0;
    else if(line >= lines)
        line = forward ? lines - 1 : 0;
    else if(!forward)
        line = lines - 1 - line;

    nav.set_cursor_by_line_number(line);
}

static bool determine_forward_or_reverse(const Playlist::CrawlerIface &crawler,
                                         Playlist::CrawlerIface::LineRelative line_relative)
{
    switch(line_relative)
    {
      case Playlist::CrawlerIface::LineRelative::AUTO:
        return crawler.is_crawling_forward();

      case Playlist::CrawlerIface::LineRelative::START_OF_LIST:
        return true;

      case Playlist::CrawlerIface::LineRelative::END_OF_LIST:
        return false;
    }

    BUG("Unreachable: %s(%d)", __func__, __LINE__);

    return true;
}

bool Playlist::DirectoryCrawler::handle_entered_list(unsigned int line,
                                                     LineRelative line_relative,
                                                     bool continue_if_empty)
{
    ++directory_depth_;
    is_first_item_in_list_processed_ = false;

    const bool is_forward(determine_forward_or_reverse(*this, line_relative));

    update_navigation(navigation_, item_flags_, is_forward, line);
    msg_info("Entered list %u at depth %u with %u entries, line %u",
             traversal_list_.get_list_id().get_raw_id(), directory_depth_,
             navigation_.get_total_number_of_visible_items(), line);

    if(navigation_.get_total_number_of_visible_items() == 0)
    {
        if(continue_if_empty)
        {
            /* found empty directory, go up again */
            switch(find_next(pass_on(find_next_callback_)))
            {
              case FindNextFnResult::SEARCHING:
                return true;

              case FindNextFnResult::STOPPED_AT_START_OF_LIST:
              case FindNextFnResult::STOPPED_AT_END_OF_LIST:
              case FindNextFnResult::FAILED:
                break;
            }
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
        const bool is_resetting(is_resetting_to_marked_position_);
        const bool temp(is_first_item_in_list_processed_);

        if(is_resetting)
        {
            is_resetting_to_marked_position_ = false;
            is_first_item_in_list_processed_ = true;
        }

        const auto find_next_result(find_next(pass_on(find_next_callback_)));

        switch(find_next_result)
        {
          case FindNextFnResult::SEARCHING:
            return true;

          case FindNextFnResult::STOPPED_AT_START_OF_LIST:
          case FindNextFnResult::STOPPED_AT_END_OF_LIST:
          case FindNextFnResult::FAILED:
            if(is_resetting)
            {
                is_resetting_to_marked_position_ = true;
                is_first_item_in_list_processed_ = temp;
            }

            break;
        }
    }

    return false;
}

void Playlist::DirectoryCrawler::handle_entered_list_failed(List::QueryContextEnterList::CallerID cid,
                                                            List::AsyncListIface::OpResult op_result)
{
    if(find_next_callback_ == nullptr)
        call_callback(failure_callback_enter_list_, *this, cid, op_result);
    else
        call_callback(pass_on(find_next_callback_), *this,
                      map_opresult_to_find_next_result(op_result));
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

    ID::List list_id;

    try
    {
        list_id =
            ViewFileBrowser::Utils::get_child_item_id(traversal_list_,
                                                      traversal_list_.get_list_id(),
                                                      navigation_, nullptr, true);
    }
    catch(const List::DBusListException &e)
    {
        switch(e.get())
        {
          case ListError::Code::OK:
          case ListError::Code::INTERNAL:
          case ListError::Code::INVALID_ID:
          case ListError::Code::INVALID_URI:
          case ListError::Code::INCONSISTENT:
            break;

          case ListError::Code::BUSY_500:
          case ListError::Code::BUSY_1000:
          case ListError::Code::BUSY_1500:
          case ListError::Code::BUSY_3000:
          case ListError::Code::BUSY_5000:
          case ListError::Code::BUSY:
            BUG("List broker is busy, should retry later");

            /* fall-through */

          case ListError::Code::INTERRUPTED:
          case ListError::Code::PHYSICAL_MEDIA_IO:
          case ListError::Code::NET_IO:
          case ListError::Code::PROTOCOL:
          case ListError::Code::AUTHENTICATION:
          case ListError::Code::NOT_SUPPORTED:
          case ListError::Code::PERMISSION_DENIED:
            return RecurseResult::SKIP;
        }

        return RecurseResult::ERROR;
    }

    if(!list_id.is_valid())
        return RecurseResult::ERROR;

    find_next_callback_ = callback;

    static constexpr auto cid(List::QueryContextEnterList::CallerID::CRAWLER_DESCEND);
    const auto result = traversal_list_.enter_list_async(list_id, 0, cid);

    switch(result)
    {
      case List::AsyncListIface::OpResult::STARTED:
        return RecurseResult::ASYNC_IN_PROGRESS;

      case List::AsyncListIface::OpResult::SUCCEEDED:
        handle_entered_list(0, LineRelative::AUTO, true);
        return RecurseResult::ASYNC_DONE;

      case List::AsyncListIface::OpResult::FAILED:
      case List::AsyncListIface::OpResult::CANCELED:
        msg_error(0, LOG_NOTICE, "Failed entering child list");
        handle_entered_list_failed(cid, result);
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
    Playlist::DirectoryCrawler::RecurseResult fail_retval = RecurseResult::ERROR;

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
              case RecurseResult::ASYNC_DONE:
                break;

              case RecurseResult::ASYNC_CANCELED:
                fail_retval = RecurseResult::ASYNC_CANCELED;
                break;

              case RecurseResult::SKIP:
                find_next(callback);
                break;

              case RecurseResult::ERROR:
                fail_crawler();
                break;
            }

            if(get_crawler_state() == CrawlerState::CRAWLING)
                return result;
        }

        break;

      case List::AsyncListIface::OpResult::CANCELED:
        fail_retval = RecurseResult::ASYNC_CANCELED;
        break;

      case List::AsyncListIface::OpResult::FAILED:
        break;
    }

    return fail_retval;
}

bool Playlist::DirectoryCrawler::do_retrieve_item_information(const RetrieveItemInfoCallback &callback)
{
    const List::Item *item;
    const auto op_result(traversal_list_.get_item_async(navigation_.get_cursor(), item));
    const auto find_next_callback(pass_on(find_next_callback_));

    RetrieveItemInfoResult result_for_callback = RetrieveItemInfoResult::FAILED;

    switch(process_current_ready_item(dynamic_cast<const ViewFileBrowser::FileItem *>(item),
                                      op_result, find_next_callback, true))
    {
      case RecurseResult::FOUND_ITEM:
        msg_info("Retrieving URIs for item \"%s\"",
                 dynamic_cast<const ViewFileBrowser::FileItem *>(item)->get_text());

        AsyncGetURIs::cancel_and_delete(async_get_uris_call_);
        AsyncGetStreamLinks::cancel_and_delete(async_get_stream_links_call_);

        if(traversal_list_.get_context_info().check_flags(List::ContextInfo::HAS_RANKED_STREAMS))
        {
            async_get_stream_links_call_ =
                mk_async_get_stream_links(dbus_proxy_,
                                          std::bind(&Playlist::DirectoryCrawler::process_item_information<AsyncGetStreamLinks>,
                                                    this, std::placeholders::_1,
                                                    traversal_list_.get_list_id(),
                                                    navigation_.get_cursor(),
                                                    directory_depth_, callback));

            if(async_get_stream_links_call_ != nullptr)
            {
                async_get_stream_links_call_->invoke(tdbus_lists_navigation_call_get_ranked_stream_links,
                                                     traversal_list_.get_list_id().get_raw_id(),
                                                     navigation_.get_cursor());
                return true;
            }
        }
        else
        {
            async_get_uris_call_ =
                mk_async_get_uris(dbus_proxy_,
                                  std::bind(&Playlist::DirectoryCrawler::process_item_information<AsyncGetURIs>,
                                            this, std::placeholders::_1,
                                            traversal_list_.get_list_id(),
                                            navigation_.get_cursor(),
                                            directory_depth_, callback));

            if(async_get_uris_call_ != nullptr)
            {
                async_get_uris_call_->invoke(tdbus_lists_navigation_call_get_uris,
                                             traversal_list_.get_list_id().get_raw_id(),
                                             navigation_.get_cursor());
                return true;
            }
        }

        msg_out_of_memory("asynchronous D-Bus call");
        break;

      case RecurseResult::ASYNC_CANCELED:
        result_for_callback = RetrieveItemInfoResult::CANCELED;
        break;

      case RecurseResult::ERROR:
        break;

      case RecurseResult::ASYNC_IN_PROGRESS:
      case RecurseResult::ASYNC_DONE:
      case RecurseResult::SKIP:
        BUG("Unexpected result for ready item");
        break;
    }

    call_callback(callback, *this, result_for_callback);

    return false;
}

static bool is_uri_acceptable(const char *uri)
{
    return uri != NULL && uri[0] != '\0';
}

namespace Playlist
{
    template <>
    struct DirectoryCrawler::ProcessItemTraits<DirectoryCrawler::AsyncGetURIs>
    {
        static bool fill_item_info_from_result(DirectoryCrawler::ItemInfo &item_info,
                                               const DirectoryCrawler::AsyncGetURIs::PromiseReturnType &result)
        {
            gchar **const uri_list(std::get<1>(result));

            if(uri_list == NULL)
                return false;

            for(gchar **ptr = uri_list; *ptr != NULL; ++ptr)
            {
                if(is_uri_acceptable(*ptr))
                {
                    msg_info("URI: \"%s\"", *ptr);
                    item_info.stream_uris_.push_back(*ptr);
                }
            }

            return !item_info.stream_uris_.empty();
        }
    };

    template <>
    struct DirectoryCrawler::ProcessItemTraits<DirectoryCrawler::AsyncGetStreamLinks>
    {
        static bool fill_item_info_from_result(DirectoryCrawler::ItemInfo &item_info,
                                               const DirectoryCrawler::AsyncGetStreamLinks::PromiseReturnType &result)
        {
            const GVariantWrapper &link_list(std::get<1>(result));
            GVariantIter iter;

            if(g_variant_iter_init(&iter, GVariantWrapper::get(link_list)) <= 0)
                return false;

            guint rank;
            guint bitrate;
            const gchar *link;

            while(g_variant_iter_next(&iter, "(uu&s)", &rank, &bitrate, &link))
            {
                if(is_uri_acceptable(link))
                {
                    msg_vinfo(MESSAGE_LEVEL_DIAG,
                              "Link: \"%s\", rank %u, bit rate %u", link, rank, bitrate);
                    item_info.airable_links_.add(Airable::RankedLink(rank, bitrate, link));
                }
            }

            return !item_info.airable_links_.empty();
        }
    };

    template <>
    std::shared_ptr<DirectoryCrawler::AsyncGetURIs> &
    DirectoryCrawler::get_async_call_ptr<DirectoryCrawler::AsyncGetURIs>()
    {
        return async_get_uris_call_;
    }

    template <>
    std::shared_ptr<DirectoryCrawler::AsyncGetStreamLinks> &
    DirectoryCrawler::get_async_call_ptr<DirectoryCrawler::AsyncGetStreamLinks>()
    {
        return async_get_stream_links_call_;
    }
}

template <typename AsyncT, typename Traits>
void Playlist::DirectoryCrawler::process_item_information(DBus::AsyncCall_ &async_call,
                                                          ID::List list_id, unsigned int line,
                                                          unsigned int directory_depth,
                                                          const RetrieveItemInfoCallback &callback)
{
    auto lock_this(lock());

    if(&async_call != get_async_call_ptr<AsyncT>().get())
    {
        call_callback(callback, *this, RetrieveItemInfoResult::CANCELED);
        return;
    }

    auto &async(static_cast<AsyncT &>(async_call));

    DBus::AsyncResult async_result;

    try
    {
        async_result = async.wait_for_result();
    }
    catch(const List::DBusListException &e)
    {
        async_result = DBus::AsyncResult::FAILED;
        msg_error(0, LOG_ERR, "Failed getting stream URIs for item %u in list %u: %s",
                  line, list_id.get_raw_id(), e.what());
    }

    if(!async.success() ||
       async_result != DBus::AsyncResult::DONE)
    {
        call_callback(callback, *this,
                      map_asyncresult_to_retrieve_item_result(async_result));
        get_async_call_ptr<AsyncT>().reset();
        return;
    }

    const auto &result(async.get_result(async_result));
    const ListError error(std::get<0>(result));

    if(error != ListError::Code::OK)
    {
        msg_error(0, LOG_NOTICE,
                  "Got error %s instead of URIs for item %u in list %u",
                  error.to_string(), line, list_id.get_raw_id());
        call_callback(callback, *this, RetrieveItemInfoResult::FAILED);
        return;
    }

    List::AsyncListIface::OpResult op_result;
    const auto *file_item =
        dynamic_cast<const ViewFileBrowser::FileItem *>(get_current_list_item_impl(op_result));

    if(file_item != nullptr)
        current_item_info_.set(list_id, line, directory_depth,
                               get_active_direction(),
                               file_item->get_text(),
                               file_item->get_preloaded_meta_data(),
                               std::move(std::get<2>(const_cast<typename AsyncT::PromiseReturnType &>(result))));
    else
        current_item_info_.set(list_id, line, directory_depth,
                               get_active_direction(),
                               std::move(std::get<2>(const_cast<typename AsyncT::PromiseReturnType &>(result))));

    if(!current_item_info_.position_.get_list_id().is_valid() ||
       !current_item_info_.is_item_info_valid_)
    {
        RetrieveItemInfoResult retrieve_result = RetrieveItemInfoResult::FAILED;

        switch(op_result)
        {
          case List::AsyncListIface::OpResult::STARTED:
            retrieve_result = RetrieveItemInfoResult::DROPPED;
            break;

          case List::AsyncListIface::OpResult::SUCCEEDED:
            BUG("Invalid item, retrieving item info failed");
            break;

          case List::AsyncListIface::OpResult::FAILED:
            break;

          case List::AsyncListIface::OpResult::CANCELED:
            retrieve_result = RetrieveItemInfoResult::CANCELED;
            break;
        }

        call_callback(callback, *this, retrieve_result);
        return;
    }

    if(Traits::fill_item_info_from_result(current_item_info_, result))
        call_callback(callback, *this, RetrieveItemInfoResult::FOUND);
    else
    {
        msg_info("No URI for item %u in list %u", line, list_id.get_raw_id());
        call_callback(callback, *this, RetrieveItemInfoResult::FOUND__NO_URL);
    }
}

Playlist::CrawlerIface::FindNextFnResult
Playlist::DirectoryCrawler::handle_end_of_list(const FindNextCallback &callback)
{
    const auto retval(is_crawling_forward()
                      ? FindNextFnResult::STOPPED_AT_END_OF_LIST
                      : FindNextFnResult::STOPPED_AT_START_OF_LIST);

    call_callback(callback, *this, is_crawling_forward()
                                   ? FindNextItemResult::END_OF_LIST
                                   : FindNextItemResult::START_OF_LIST);

    return retval;
}

List::AsyncListIface::OpResult
Playlist::DirectoryCrawler::back_to_parent(const FindNextCallback &callback)
{
    if(directory_depth_ <= 1)
        return List::AsyncListIface::OpResult::CANCELED;

    unsigned int item_id;
    ID::List list_id;

    try
    {
        list_id =
            ViewFileBrowser::Utils::get_parent_link_id(traversal_list_,
                                                       traversal_list_.get_list_id(),
                                                       item_id);
    }
    catch(const List::DBusListException &e)
    {
        /* leave #list_id invalid, fail below */
    }

    if(!list_id.is_valid())
        return List::AsyncListIface::OpResult::FAILED;

    find_next_callback_ = callback;

    const auto result =
        traversal_list_.enter_list_async(list_id, item_id,
                                         List::QueryContextEnterList::CallerID::CRAWLER_ASCEND);

    switch(result)
    {
      case List::AsyncListIface::OpResult::STARTED:
      case List::AsyncListIface::OpResult::SUCCEEDED:
        break;

      case List::AsyncListIface::OpResult::FAILED:
      case List::AsyncListIface::OpResult::CANCELED:
        msg_error(0, LOG_NOTICE, "Failed entering parent list");
        find_next_callback_ = nullptr;
        break;
    }

    return result;
}

Playlist::CrawlerIface::FindNextFnResult
Playlist::DirectoryCrawler::find_next_impl(FindNextCallback callback)
{
    find_next_callback_ = nullptr;

    bool looping = true;

    while(looping)
    {
        msg_info("Find next %s, depth %u, wait enter %d, wait item %d, go to marked %d, first is %sprocessed",
                 is_crawling_forward() ? "forward" : "backward",
                 directory_depth_,
                 is_waiting_for_async_enter_list_completion_,
                 is_waiting_for_async_get_list_item_completion_,
                 is_resetting_to_marked_position_,
                 is_first_item_in_list_processed_ ? "" : "not ");
        looping = false;

        if(is_waiting_for_async_enter_list_completion_ ||
           is_resetting_to_marked_position_)
        {
            /* let the follow-up handler deal with this */
            find_next_callback_ = callback;
            break;
        }

        if((is_first_item_in_list_processed_ &&
            !is_waiting_for_async_get_list_item_completion_ &&
            !go_to_next_list_item()) ||
           (!is_first_item_in_list_processed_ &&
            navigation_.get_total_number_of_visible_items() == 0))
        {
            if(directory_depth_ <= 1)
                return handle_end_of_list(callback);

            switch(back_to_parent(callback))
            {
              case List::AsyncListIface::OpResult::STARTED:
                return FindNextFnResult::SEARCHING;

              case List::AsyncListIface::OpResult::SUCCEEDED:
                break;

              case List::AsyncListIface::OpResult::FAILED:
                call_callback(callback, *this, FindNextItemResult::FAILED);
                return FindNextFnResult::FAILED;

              case List::AsyncListIface::OpResult::CANCELED:
                call_callback(callback, *this, FindNextItemResult::CANCELED);
                return FindNextFnResult::FAILED;
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
            call_callback(callback, *this, FindNextItemResult::FAILED);
            return FindNextFnResult::FAILED;

          case List::AsyncListIface::OpResult::CANCELED:
            BUG("Unexpected canceled result");
            call_callback(callback, *this, FindNextItemResult::CANCELED);
            return FindNextFnResult::FAILED;
        }
    }

    return FindNextFnResult::SEARCHING;
}

const List::Item *Playlist::DirectoryCrawler::get_current_list_item_impl(List::AsyncListIface::OpResult &op_result)
{
    const List::Item *item;
    op_result = traversal_list_.get_item_async(navigation_.get_cursor(), item);

    if(op_result == List::AsyncListIface::OpResult::SUCCEEDED)
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

    const auto cid(ctx->get_caller_id());

    switch(cid)
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
            handle_entered_list(ctx->parameters_.line_, LineRelative::START_OF_LIST, false);
            break;

          case List::AsyncListIface::OpResult::FAILED:
          case List::AsyncListIface::OpResult::CANCELED:
            handle_entered_list_failed(cid, result);
            fail_crawler();
            break;
        }

        break;

      case List::QueryContextEnterList::CallerID::CRAWLER_RESET_POSITION:
        switch(result)
        {
          case List::AsyncListIface::OpResult::STARTED:
            is_resetting_to_marked_position_ = true;
            break;

          case List::AsyncListIface::OpResult::SUCCEEDED:
            log_assert(marked_position_.get_list_id() == traversal_list_.get_list_id());
            log_assert(marked_position_.get_line() == ctx->parameters_.line_);

            directory_depth_ = marked_position_.get_directory_depth() - 1;

            if(!handle_entered_list(ctx->parameters_.line_, LineRelative::START_OF_LIST, false))
                is_resetting_to_marked_position_ = false;

            break;

          case List::AsyncListIface::OpResult::FAILED:
          case List::AsyncListIface::OpResult::CANCELED:
            is_resetting_to_marked_position_ = false;
            handle_entered_list_failed(cid, result);
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
            handle_entered_list(ctx->parameters_.line_, LineRelative::AUTO, true);
            break;

          case List::AsyncListIface::OpResult::FAILED:
            find_next(pass_on(find_next_callback_));
            break;

          case List::AsyncListIface::OpResult::CANCELED:
            handle_entered_list_failed(cid, result);
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

            update_navigation(navigation_, item_flags_, true, ctx->parameters_.line_);

            if(navigation_.get_total_number_of_visible_items() == 0)
            {
                /* parent directory cannot be empty, must be an error */
                fail_crawler();
            }

            find_next(pass_on(find_next_callback_));

            break;

          case List::AsyncListIface::OpResult::FAILED:
            handle_entered_list_failed(cid, result);
            fail_crawler();
            break;

          case List::AsyncListIface::OpResult::CANCELED:
            handle_entered_list_failed(cid, result);
            break;
        }

        break;
    }
}

void Playlist::DirectoryCrawler::handle_get_item_failed(List::QueryContextGetItem::CallerID cid,
                                                        List::AsyncListIface::OpResult op_result)
{
    if(find_next_callback_ == nullptr &&
       op_result != List::AsyncListIface::OpResult::STARTED &&
       op_result != List::AsyncListIface::OpResult::SUCCEEDED)
        call_callback(failure_callback_get_item_, *this, cid, op_result);
    else
        call_callback(pass_on(find_next_callback_), *this,
                      map_opresult_to_find_next_result(op_result));
}

void Playlist::DirectoryCrawler::handle_get_item_event(List::AsyncListIface::OpResult result,
                                                       const std::shared_ptr<List::QueryContextGetItem> &ctx)
{
    auto lock_this(lock());

    is_waiting_for_async_get_list_item_completion_ =
        (result == List::AsyncListIface::OpResult::STARTED);

    const auto cid(ctx->get_caller_id());

    switch(cid)
    {
      case List::QueryContextGetItem::CallerID::SERIALIZE:
      case List::QueryContextGetItem::CallerID::SERIALIZE_DEBUG:
        BUG("Wrong caller ID in %s()", __PRETTY_FUNCTION__);
        break;

      case List::QueryContextGetItem::CallerID::CRAWLER_FIND_MARKED:
        switch(result)
        {
          case List::AsyncListIface::OpResult::STARTED:
            is_resetting_to_marked_position_ = true;
            break;

          case List::AsyncListIface::OpResult::SUCCEEDED:
            is_resetting_to_marked_position_ = false;

            if(find_next_callback_ != nullptr)
                find_next(pass_on(find_next_callback_));

            break;

          case List::AsyncListIface::OpResult::FAILED:
          case List::AsyncListIface::OpResult::CANCELED:
            is_resetting_to_marked_position_ = false;
            handle_get_item_failed(cid, result);
            break;
        }

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
            if(find_next_callback_ == nullptr)
                handle_get_item_failed(cid, result);
            else
            {
                is_first_item_in_list_processed_ = true;

                const List::Item *item = nullptr;
                auto op_result(result == List::AsyncListIface::OpResult::SUCCEEDED
                               ? traversal_list_.get_item_async(navigation_.get_cursor(), item):
                               result);
                const auto find_next_callback(pass_on(find_next_callback_));

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

                  case RecurseResult::ASYNC_CANCELED:
                    result = List::AsyncListIface::OpResult::CANCELED;
                    break;

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
