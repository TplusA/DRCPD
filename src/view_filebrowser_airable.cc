/*
 * Copyright (C) 2016--2020  T+A elektroakustik GmbH & Co. KG
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

#include "view_filebrowser_airable.hh"
#include "view_filebrowser_fileitem.hh"
#include "view_filebrowser_utils.hh"
#include "view_play.hh"
#include "ui_parameters_predefined.hh"
#include "de_tahifi_lists_context.h"
#include "messages.h"

bool ViewFileBrowser::AirableView::try_jump_to_stored_position(StoredPosition &pos)
{
    if(!pos.is_set())
        return false;

    if(point_to_any_location(get_viewport().get(),
                             pos.get_list_id(), pos.get_line_number(),
                             pos.get_context_root()))
        return true;

    pos.clear();

    return false;
}

void ViewFileBrowser::AirableView::append_referenced_lists(std::vector<ID::List> &list_ids) const
{
    for(const auto &pos : audio_source_navigation_stash_)
    {
        if(pos.is_set() && !pos.is_keep_alive_suppressed())
            list_ids.push_back(pos.get_list_id());
    }
}

void ViewFileBrowser::AirableView::audio_source_state_changed(
        const Player::AudioSource &audio_source,
        Player::AudioSourceState prev_state)
{
    switch(audio_source.get_state())
    {
      case Player::AudioSourceState::DESELECTED:
        if(current_list_id_.is_valid())
        {
            auto &stash(audio_source_navigation_stash_[get_audio_source_index(audio_source)]);

            stash.set(current_list_id_, browse_navigation_.get_line_number_by_cursor(),
                      context_restriction_.get_root_list_id(),
                      get_dynamic_title());
        }

        break;

      case Player::AudioSourceState::REQUESTED:
        break;

      case Player::AudioSourceState::SELECTED:
        {
            const auto idx(get_audio_source_index(audio_source));

            if(select_audio_source(idx))
            {
                if(idx > 0)
                    set_list_context_root(audio_source_index_to_list_context(idx));
                else
                    set_list_context_root(List::ContextMap::INVALID_ID);

                auto &stash(audio_source_navigation_stash_[idx]);

                if(try_jump_to_stored_position(stash))
                {
                    /* already there */
                    stash.suppress_keep_alive();
                    set_dynamic_title(stash.get_list_title());
                }
                else
                    point_to_root_directory();
            }
        }

        break;
    }
}

ViewIface::InputResult
ViewFileBrowser::AirableView::logged_into_service_notification(const std::string &service_id,
                                                               enum ActorID actor_id,
                                                               const ListError &error)
{
    if(error.failed())
        msg_vinfo(MESSAGE_LEVEL_IMPORTANT,
                  "Failed logging into \"%s\" by %u (%s)\n",
                  service_id.c_str(), actor_id, error.to_string());
    else
        msg_vinfo(MESSAGE_LEVEL_IMPORTANT, "Logged into \"%s\" by %u\n",
                  service_id.c_str(), actor_id);

    List::context_id_t ctx_id;
    const auto &ctx(list_contexts_.get_context_info_by_string_id(service_id, ctx_id));

    if(error.failed())
    {
        switch(actor_id)
        {
          case ACTOR_ID_LOCAL_UI:
            switch(may_access_list_for_serialization())
            {
              case ListAccessPermission::ALLOWED:
              case ListAccessPermission::DENIED__BLOCKED:
              case ListAccessPermission::DENIED__NO_LIST_ID:
                StandardError::service_authentication_failure(list_contexts_, ctx_id);
                break;

              case ListAccessPermission::DENIED__LOADING:
                /* suppress duplicate error emission */
                break;
            }

            break;

          case ACTOR_ID_INVALID:
          case ACTOR_ID_UNKNOWN:
          case ACTOR_ID_SMARTPHONE_APP:
            break;
        }

        return InputResult::OK;
    }

    if(!ctx.is_valid() || ctx_id != context_restriction_.get_context_id())
        return InputResult::OK;

    point_to_root_directory();

    return InputResult::UPDATE_NEEDED;
}

ViewIface::InputResult
ViewFileBrowser::AirableView::logged_out_from_service_notification(const std::string &service_id,
                                                                   enum ActorID actor_id,
                                                                   const ListError &error)
{
    if(error.failed())
    {
        msg_vinfo(MESSAGE_LEVEL_IMPORTANT,
                  "Failed logging out from \"%s\" by %u\n",
                  service_id.c_str(), actor_id);
        return InputResult::OK;
    }

    msg_vinfo(MESSAGE_LEVEL_IMPORTANT, "Logged out from \"%s\" by %u\n",
              service_id.c_str(), actor_id);

    List::context_id_t ctx_id;
    const auto &ctx(list_contexts_.get_context_info_by_string_id(service_id, ctx_id));

    if(!ctx.is_valid())
        return InputResult::OK;

    const List::context_id_t current_browse_context =
        DBUS_LISTS_CONTEXT_GET(current_list_id_.get_raw_id());

    InputResult result;

    if(current_browse_context == ctx_id)
    {
        point_to_root_directory();
        result = InputResult::UPDATE_NEEDED;
    }
    else
        result = InputResult::OK;

    search_forms_.erase(ctx_id);

    return result;
}

bool ViewFileBrowser::AirableView::register_audio_sources()
{
    log_assert(default_audio_source_name_ == nullptr);

    if(list_contexts_.empty())
    {
        BUG("No list contexts, cannot create audio sources");
        return false;
    }

    audio_source_navigation_stash_.resize(list_contexts_.size());

    for(const auto &ctx : list_contexts_)
    {
        static const char prefix[] = "airable.";

        auto cb([this] (const Player::AudioSource &src, Player::AudioSourceState prev_state)
                {
                    audio_source_state_changed(src, prev_state);
                });

        if(ctx.string_id_.length() >= sizeof(prefix) - 2 &&
           std::equal(prefix, prefix + sizeof(prefix) - 2, ctx.string_id_.begin()) &&
           (ctx.string_id_.length() == sizeof(prefix) - 2 ||
            ctx.string_id_[sizeof(prefix) - 2] == '.'))
        {
            /* take any string as is if it begins with "airable." or if it is
             * exactly the string "airable" */
            new_audio_source(std::move(std::string(ctx.string_id_)), std::move(cb));
        }
        else
        {
            /* put "airable." in front of the name */
            std::string temp(prefix);
            temp += ctx.string_id_;
            new_audio_source(std::move(temp), std::move(cb));
        }
    }

    /* for the time being, we need the root audio source in the first slot */
    log_assert(audio_source_index_to_list_context(0) == 0);
    log_assert(get_audio_source_by_index(0).id_ == "airable");

    select_audio_source(0);

    auto *const pview = static_cast<ViewPlay::View *>(play_view_);
    size_t i = 0;

    for(const auto &ctx : list_contexts_)
    {
        pview->register_audio_source(get_audio_source_by_index(i), *this);
        register_own_source_with_audio_path_manager(i, ctx.description_.c_str());
        ++i;
    }

    return true;
}

static void patch_event_id_for_deezer(UI::ViewEventID &event_id)
{
    switch(event_id)
    {
      case UI::ViewEventID::PLAYBACK_COMMAND_START:
      case UI::ViewEventID::NAV_SELECT_ITEM:
      case UI::ViewEventID::NAV_SCROLL_LINES:
      case UI::ViewEventID::NAV_SCROLL_PAGES:
      case UI::ViewEventID::SEARCH_COMMENCE:
      case UI::ViewEventID::SEARCH_STORE_PARAMETERS:
      case UI::ViewEventID::PLAYBACK_TRY_RESUME:
      case UI::ViewEventID::STRBO_URL_RESOLVED:
        event_id = UI::ViewEventID::NOP;
        break;

      case UI::ViewEventID::NOP:
      case UI::ViewEventID::PLAYBACK_COMMAND_STOP:
      case UI::ViewEventID::PLAYBACK_COMMAND_PAUSE:
      case UI::ViewEventID::PLAYBACK_PREVIOUS:
      case UI::ViewEventID::PLAYBACK_NEXT:
      case UI::ViewEventID::PLAYBACK_FAST_WIND_SET_SPEED:
      case UI::ViewEventID::PLAYBACK_SEEK_STREAM_POS:
      case UI::ViewEventID::PLAYBACK_MODE_REPEAT_TOGGLE:
      case UI::ViewEventID::PLAYBACK_MODE_SHUFFLE_TOGGLE:
      case UI::ViewEventID::NAV_GO_BACK_ONE_LEVEL:
      case UI::ViewEventID::STORE_STREAM_META_DATA:
      case UI::ViewEventID::STORE_PRELOADED_META_DATA:
      case UI::ViewEventID::NOTIFY_AIRABLE_SERVICE_LOGIN_STATUS_UPDATE:
      case UI::ViewEventID::NOTIFY_NOW_PLAYING:
      case UI::ViewEventID::NOTIFY_STREAM_STOPPED:
      case UI::ViewEventID::NOTIFY_STREAM_PAUSED:
      case UI::ViewEventID::NOTIFY_STREAM_UNPAUSED:
      case UI::ViewEventID::NOTIFY_STREAM_POSITION:
      case UI::ViewEventID::NOTIFY_SPEED_CHANGED:
      case UI::ViewEventID::NOTIFY_PLAYBACK_MODE_CHANGED:
      case UI::ViewEventID::AUDIO_SOURCE_SELECTED:
      case UI::ViewEventID::AUDIO_SOURCE_DESELECTED:
      case UI::ViewEventID::AUDIO_PATH_CHANGED:
        break;
    }
}

static bool is_deezer(const List::ContextMap &list_contexts,
                      ID::List current_list_id)
{
    List::context_id_t deezer_id;
    list_contexts.get_context_info_by_string_id("deezer", deezer_id);

    return (deezer_id != List::ContextMap::INVALID_ID &&
            deezer_id == DBUS_LISTS_CONTEXT_GET(current_list_id.get_raw_id()));
}

ViewIface::InputResult
ViewFileBrowser::AirableView::process_event(UI::ViewEventID event_id,
                                            std::unique_ptr<UI::Parameters> parameters)
{
    if(is_deezer(list_contexts_, current_list_id_))
        patch_event_id_for_deezer(event_id);

    if(event_id != UI::ViewEventID::NOTIFY_AIRABLE_SERVICE_LOGIN_STATUS_UPDATE)
        return ViewFileBrowser::View::process_event(event_id, std::move(parameters));

    const auto params =
        UI::Events::downcast<UI::ViewEventID::NOTIFY_AIRABLE_SERVICE_LOGIN_STATUS_UPDATE>(parameters);

    if(params == nullptr)
        return InputResult::OK;

    const auto &plist = params->get_specific();
    const auto &service_id(std::get<0>(plist));
    const enum ActorID actor_id(std::get<1>(plist));
    const bool is_login(std::get<2>(plist));
    const ListError error(std::get<3>(plist));

    return is_login
        ? logged_into_service_notification(service_id, actor_id, error)
        : logged_out_from_service_notification(service_id, actor_id, error);
}

bool ViewFileBrowser::AirableView::list_invalidate(ID::List list_id, ID::List replacement_id)
{
    if(is_root_list(list_id))
        search_forms_.clear();

    for(auto &stash : audio_source_navigation_stash_)
        stash.list_invalidate(list_id, replacement_id);

    return View::list_invalidate(list_id, replacement_id);
}

void ViewFileBrowser::AirableView::finish_async_point_to_child_directory()
{
    log_assert(current_list_id_.is_valid());

    const unsigned int selected_line_from_root =
        async_calls_deco_.point_to_child_directory_.selected_line_from_root_;

    async_calls_deco_.point_to_child_directory_.selected_line_from_root_ = UINT_MAX;

    if(selected_line_from_root == UINT_MAX)
        return;

    /*
     * Find the search form link in the current list.
     */
    if(is_root_list(current_list_id_))
        return;

    const List::context_id_t ctx_id(DBUS_LISTS_CONTEXT_GET(current_list_id_.get_raw_id()));

    if(search_forms_.find(ctx_id) != search_forms_.end())
    {
        /* already know the search form */
        return;
    }

    const auto &ctx(list_contexts_[ctx_id]);

    if(!ctx.is_valid())
    {
        BUG("Attempted to find search form in invalid context %u", ctx_id);
        return;
    }

    if(!ctx.check_flags(List::ContextInfo::HAS_PROPER_SEARCH_FORM) ||
       ctx.check_flags(List::ContextInfo::SEARCH_NOT_POSSIBLE))
    {
        BUG("Attempted to find nonexistent search form link in context %s",
            ctx.string_id_.c_str());
        return;
    }

    const unsigned int num = file_list_.get_number_of_items();
    unsigned int i;

    /* use synchronous API to successively load all items in the list and find
     * the search form---list should be empty anyway */
    auto viewport = file_list_.mk_viewport(10, "find search form");;

    for(i = 0; i < num; ++i)
    {
        const FileItem *item;

        try
        {
            item = dynamic_cast<const FileItem *>(file_list_.get_item(viewport, i));
        }
        catch(const List::DBusListException &e)
        {
            msg_error(0, LOG_ERR,
                      "Failed finding search form in context %s, "
                      "got hard %s error: %s",
                      ctx.string_id_.c_str(),
                      e.get_internal_detail_string_or_fallback("list retrieval"),
                      e.what());

            break;
        }

        if(item == nullptr)
        {
            msg_error(0, LOG_ERR,
                      "Empty entry while searching for search form in context %s",
                      ctx.string_id_.c_str());

            break;
        }

        if(item->get_kind().get() == ListItemKind::SEARCH_FORM)
        {
            msg_vinfo(MESSAGE_LEVEL_DEBUG,
                      "Found search form link for context %s: \"%s\" at /%u/%u",
                      ctx.string_id_.c_str(), item->get_text(),
                      selected_line_from_root, i);

            search_forms_.emplace(ctx_id, std::make_pair(selected_line_from_root, i));

            break;
        }
    }

    if(i >= num)
        BUG("Expected to find search form link for context %s in list %u",
            ctx.string_id_.c_str(), current_list_id_.get_raw_id());

    file_list_.detach_viewport(std::move(viewport));
}

void ViewFileBrowser::AirableView::handle_enter_list_event(List::AsyncListIface::OpResult result,
                                                           const std::shared_ptr<List::QueryContextEnterList> &ctx)
{
    if(!View::handle_enter_list_event_finish(result, ctx))
        return;

    switch(ctx->get_caller_id())
    {
      case List::QueryContextEnterList::CallerID::ENTER_ROOT:
      case List::QueryContextEnterList::CallerID::ENTER_PARENT:
      case List::QueryContextEnterList::CallerID::ENTER_ANYWHERE:
      case List::QueryContextEnterList::CallerID::RELOAD_LIST:
      case List::QueryContextEnterList::CallerID::CRAWLER_RESET_POSITION:
      case List::QueryContextEnterList::CallerID::CRAWLER_FIRST_ENTRY:
      case List::QueryContextEnterList::CallerID::CRAWLER_DESCEND:
      case List::QueryContextEnterList::CallerID::CRAWLER_ASCEND:
        break;

      case List::QueryContextEnterList::CallerID::ENTER_CHILD:
      case List::QueryContextEnterList::CallerID::ENTER_CONTEXT_ROOT:
        finish_async_point_to_child_directory();
        break;
    }

    View::handle_enter_list_event_update_after_finish(result, ctx);
}

bool ViewFileBrowser::AirableView::point_to_child_directory(const SearchParameters *search_parameters)
{
    if(!is_root_list(current_list_id_) || search_parameters != nullptr)
    {
        async_calls_deco_.point_to_child_directory_.selected_line_from_root_ = UINT_MAX;
        return View::point_to_child_directory(search_parameters);
    }

    async_calls_deco_.point_to_child_directory_.selected_line_from_root_ =
        browse_navigation_.get_cursor();

    if(View::point_to_child_directory())
        return true;

    async_calls_deco_.point_to_child_directory_.selected_line_from_root_ = UINT_MAX;

    return false;
}

/*!
 * Chained from #ViewFileBrowser::AirableView::point_to_search_form().
 *
 * Called when the ID of the root list has been determined.
 */
void ViewFileBrowser::AirableView::point_to_search_form__got_root_list_id(
        DBusRNF::GetListIDCall &call, List::context_id_t ctx_id)
{
    auto lock(lock_async_calls());

    if(&call != async_calls_.get_get_list_id().get())
        return;

    async_calls_.delete_get_list_id();

    ID::List root_id;

    try
    {
        auto result(call.get_result_unlocked());

        if(result.error_ != ListError::Code::OK)
            msg_error(0, LOG_NOTICE,
                      "Got error for root list ID, error code %s",
                      result.error_.to_string());
        else if(!result.list_id_.is_valid())
            BUG("Got invalid list ID for root list, but no error code");
        else
            root_id = result.list_id_;
    }
    catch(const List::DBusListException &e)
    {
        msg_error(0, LOG_ERR,
                  "Failed obtaining ID for root list for search: %s error: %s",
                  e.get_internal_detail_string_or_fallback("async call"),
                  e.what());
    }
    catch(const std::exception &e)
    {
        msg_error(0, LOG_ERR,
                  "Failed obtaining ID for root list for search: %s", e.what());
    }
    catch(...)
    {
        msg_error(0, LOG_ERR, "Failed obtaining ID for root list for search");
    }

    if(!root_id.is_valid())
    {
        point_to_root_directory();
        return;
    }

    auto chain_call =
        std::make_unique<DBusRNF::Chain<DBusRNF::GetListIDCall>>(
            [this, ctx_id, root_id] (auto &c, DBusRNF::CallState)
            {
                this->point_to_search_form__got_service_list_id(c, ctx_id,
                                                                root_id);
            });

    auto next_call(async_calls_.set_call(std::make_shared<DBusRNF::GetListIDCall>(
        file_list_.get_cookie_manager(), file_list_.get_dbus_proxy(),
        root_id, search_forms_[ctx_id].first, std::move(chain_call), nullptr)));

    if(next_call == nullptr)
    {
        msg_out_of_memory("async go to service for search");
        point_to_root_directory();
        return;
    }

    switch(next_call->request())
    {
      case DBusRNF::CallState::WAIT_FOR_NOTIFICATION:
      case DBusRNF::CallState::RESULT_FETCHED:
        return;

      case DBusRNF::CallState::ABORTING:
        break;

      case DBusRNF::CallState::INITIALIZED:
      case DBusRNF::CallState::READY_TO_FETCH:
      case DBusRNF::CallState::ABOUT_TO_DESTROY:
        BUG("GetListIDCall for service list for search ended up in unexpected state");
        async_calls_.delete_get_list_id();
        break;

      case DBusRNF::CallState::ABORTED_BY_LIST_BROKER:
      case DBusRNF::CallState::FAILED:
        async_calls_.delete_get_list_id();
        break;
    }

    point_to_root_directory();
}

/*!
 * Chained from #ViewFileBrowser::AirableView::point_to_search_form__got_root_list_id().
 *
 * Called when the ID of the service's list has been determined.
 */
void ViewFileBrowser::AirableView::point_to_search_form__got_service_list_id(
        DBusRNF::GetListIDCall &call, List::context_id_t ctx_id,
        ID::List context_root)
{
    auto lock(lock_async_calls());

    if(&call != async_calls_.get_get_list_id().get())
        return;

    async_calls_.delete_get_list_id();

    try
    {
        auto result(call.get_result_unlocked());

        if(result.error_ != ListError::Code::OK)
            msg_error(0, LOG_NOTICE,
                      "Got error for root list ID, error code %s",
                      result.error_.to_string());
        else if(!result.list_id_.is_valid())
            BUG("Got invalid list ID for root list, but no error code");
        else if(point_to_any_location(get_viewport().get(), result.list_id_,
                                      search_forms_[ctx_id].second,
                                      result.list_id_))
            return;
    }
    catch(const List::DBusListException &e)
    {
        msg_error(0, LOG_ERR,
                  "Failed obtaining ID for root list for search: %s error: %s",
                  e.get_internal_detail_string_or_fallback("async call"),
                  e.what());
    }
    catch(const std::exception &e)
    {
        msg_error(0, LOG_ERR,
                  "Failed obtaining ID for root list for search: %s", e.what());
    }
    catch(...)
    {
        msg_error(0, LOG_ERR, "Failed obtaining ID for root list for search");
    }

    point_to_root_directory();
}

ViewFileBrowser::View::GoToSearchForm
ViewFileBrowser::AirableView::point_to_search_form(List::context_id_t ctx_id)
{
    auto lock(lock_async_calls());
    cancel_and_delete_all_async_calls();

    const auto &ctx(list_contexts_[ctx_id]);
    if(!ctx.check_flags(List::ContextInfo::HAS_PROPER_SEARCH_FORM))
        return GoToSearchForm::NOT_SUPPORTED;

    const auto &form(search_forms_.find(ctx_id));
    if(form == search_forms_.end())
        return GoToSearchForm::NOT_AVAILABLE;

    auto chain_call =
        std::make_unique<DBusRNF::Chain<DBusRNF::GetListIDCall>>(
            [this, ctx_id] (auto &call, DBusRNF::CallState)
            {
                this->point_to_search_form__got_root_list_id(call, ctx_id);
            });

    auto call(async_calls_.set_call(std::make_shared<DBusRNF::GetListIDCall>(
        file_list_.get_cookie_manager(), file_list_.get_dbus_proxy(),
        ID::List(), 0, std::move(chain_call), nullptr)));

    if(call == nullptr)
    {
        msg_out_of_memory("async go to root for search");
        return GoToSearchForm::NOT_AVAILABLE;
    }

    switch(call->request())
    {
      case DBusRNF::CallState::WAIT_FOR_NOTIFICATION:
      case DBusRNF::CallState::RESULT_FETCHED:
        return GoToSearchForm::NAVIGATING;

      case DBusRNF::CallState::ABORTING:
        break;

      case DBusRNF::CallState::INITIALIZED:
      case DBusRNF::CallState::READY_TO_FETCH:
      case DBusRNF::CallState::ABOUT_TO_DESTROY:
        BUG("GetListIDCall for root for search ended up in unexpected state");
        async_calls_.delete_get_list_id();
        break;

      case DBusRNF::CallState::ABORTED_BY_LIST_BROKER:
      case DBusRNF::CallState::FAILED:
        async_calls_.delete_get_list_id();
        break;
    }

    return GoToSearchForm::NOT_AVAILABLE;
}

void ViewFileBrowser::AirableView::log_out_from_context(List::context_id_t context)
{
    GErrorWrapper error;
    const auto &ctx(list_contexts_[context]);
    tdbus_airable_call_external_service_logout_sync(DBus::get_airable_sec_iface(),
                                                    ctx.string_id_.c_str(), "",
                                                    true, ACTOR_ID_LOCAL_UI,
                                                    NULL, error.await());
    error.log_failure("Logout from service");
}

uint32_t ViewFileBrowser::AirableView::about_to_write_xml(const DCP::Queue::Data &data) const
{
    uint32_t bits = ViewFileBrowser::View::about_to_write_xml(data);

    if(is_deezer(list_contexts_, current_list_id_))
        bits |= WRITE_FLAG__IS_LOCKED;

    return bits;
}

static inline List::context_id_t
determine_ctx_id(bool have_audio_source,
                 const List::context_id_t &restricted_ctx,
                 const ID::List current_list_id)
{
    auto result(have_audio_source ? restricted_ctx : List::ContextMap::INVALID_ID);

    if(result == List::ContextMap::INVALID_ID && current_list_id.is_valid())
        result = DBUS_LISTS_CONTEXT_GET(current_list_id.get_raw_id());

    return result;
}

bool ViewFileBrowser::AirableView::write_xml(std::ostream &os, uint32_t bits,
                                             const DCP::Queue::Data &data)
{
    if((bits & WRITE_FLAG_GROUP__AS_MSG_NO_GET_ITEM_HINT_NEEDED) == 0)
        return ViewFileBrowser::View::write_xml(os, bits, data);

    const auto ctx_id(determine_ctx_id(have_audio_source(),
                                       context_restriction_.get_context_id(),
                                       current_list_id_));
    const auto &ctx(list_contexts_[ctx_id]);

    os << "<text id=\"cbid\">" << int(drcp_browse_id_) << "</text>"
       << "<context>"
       << ctx.string_id_.c_str()
       << "</context>";

    os << "<text id=\"line0\">" << XmlEscape(ctx.description_) << "</text>"
       << "<text id=\"line1\">";

    if((bits & WRITE_FLAG__IS_LOADING) != 0)
        os << XmlEscape(_("Accessing")) << "...";
    else if((bits & WRITE_FLAG__IS_UNAVAILABLE) != 0)
        os << XmlEscape(_("Unavailable"));
    else if((bits & WRITE_FLAG__IS_LOCKED) != 0)
        os << XmlEscape(_("Please use our app."));
    else
        BUG("Airable: what are we supposed to display here?!");

    os << "</text>";

    return true;
}

void ViewFileBrowser::AirableView::cancel_and_delete_all_async_calls()
{
    View::cancel_and_delete_all_async_calls();
}
