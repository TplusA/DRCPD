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

#include "view_filebrowser_airable.hh"
#include "view_filebrowser_fileitem.hh"
#include "view_filebrowser_utils.hh"
#include "view_play.hh"
#include "ui_parameters_predefined.hh"
#include "de_tahifi_lists_context.h"
#include "messages.h"

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

ViewIface::InputResult
ViewFileBrowser::AirableView::process_event(UI::ViewEventID event_id,
                                            std::unique_ptr<const UI::Parameters> parameters)
{
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
    file_list_.push_cache_state();

    for(i = 0; i < num; ++i)
    {
        const FileItem *item;

        try
        {
            item = dynamic_cast<const FileItem *>(file_list_.get_item(i));
        }
        catch(const List::DBusListException &e)
        {
            msg_error(0, LOG_ERR,
                      "Failed finding search form in context %s, "
                      "got hard %s error: %s",
                      ctx.string_id_.c_str(),
                      e.is_dbus_error() ? "D-Bus" : "list retrieval", e.what());

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

    file_list_.pop_cache_state();
}

void ViewFileBrowser::AirableView::handle_enter_list_event(List::AsyncListIface::OpResult result,
                                                           const std::shared_ptr<List::QueryContextEnterList> &ctx)
{
    if(!View::handle_enter_list_event_finish(result, ctx))
        return;

    switch(ctx->get_caller_id())
    {
      case List::QueryContextEnterList::CallerID::SYNC_WRAPPER:
      case List::QueryContextEnterList::CallerID::ENTER_ROOT:
      case List::QueryContextEnterList::CallerID::ENTER_PARENT:
      case List::QueryContextEnterList::CallerID::RELOAD_LIST:
      case List::QueryContextEnterList::CallerID::CRAWLER_RESTART:
      case List::QueryContextEnterList::CallerID::CRAWLER_RESET_POSITION:
      case List::QueryContextEnterList::CallerID::CRAWLER_RESUME_FROM_POSITION:
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

    async_calls_deco_.point_to_child_directory_.selected_line_from_root_ = navigation_.get_cursor();

    if(View::point_to_child_directory())
        return true;

    async_calls_deco_.point_to_child_directory_.selected_line_from_root_ = UINT_MAX;

    return false;
}

ViewFileBrowser::View::GoToSearchForm
ViewFileBrowser::AirableView::point_to_search_form(List::context_id_t ctx_id)
{
    const auto &ctx(list_contexts_[ctx_id]);

    if(!ctx.check_flags(List::ContextInfo::HAS_PROPER_SEARCH_FORM))
        return GoToSearchForm::NOT_SUPPORTED;

    const auto &form(search_forms_.find(ctx_id));

    if(form == search_forms_.end())
        return GoToSearchForm::NOT_AVAILABLE;

    {
        auto lock(lock_async_calls());
        cancel_and_delete_all_async_calls();
    }

    const auto &path(form->second);

    const ID::List revert_to_list_id = current_list_id_;;
    const unsigned int revert_to_cursor = navigation_.get_cursor();

    try
    {
        Utils::enter_list_at(file_list_, item_flags_, navigation_,
                             get_root_list_id(), path.first);
        current_list_id_ = get_root_list_id();

        std::string list_title;
        const ID::List list_id =
            Utils::get_child_item_id(file_list_, current_list_id_,
                                     navigation_, nullptr, list_title);

        if(list_id.is_valid())
        {
            Utils::enter_list_at(file_list_, item_flags_, navigation_,
                                 list_id, path.second);
            current_list_id_ = list_id;

            if(navigation_.get_cursor() == path.second)
                return GoToSearchForm::FOUND;
        }
    }
    catch(const List::DBusListException &e)
    {
        /* handled below */
    }

    try
    {
        /* in case of any failure, try go back to old location */
        Utils::enter_list_at(file_list_, item_flags_, navigation_,
                             revert_to_list_id, revert_to_cursor);
        current_list_id_ = revert_to_list_id;
    }
    catch(const List::DBusListException &e)
    {
        /* pity... */
    }

    return GoToSearchForm::NOT_AVAILABLE;
}

void ViewFileBrowser::AirableView::log_out_from_context(List::context_id_t context)
{
    GError *error = NULL;
    const auto &ctx(list_contexts_[context]);
    tdbus_airable_call_external_service_logout_sync(dbus_get_airable_sec_iface(),
                                                    ctx.string_id_.c_str(), "",
                                                    true, ACTOR_ID_LOCAL_UI,
                                                    NULL, &error);
    dbus_common_handle_error(&error, "Logout from service");
}

static inline List::context_id_t
determine_ctx_id(bool have_audio_source,
                 const List::context_id_t restricted_ctx,
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
    else
        BUG("Airable: what are we supposed to display here?!");

    os << "</text>";

    return true;
}

void ViewFileBrowser::AirableView::cancel_and_delete_all_async_calls()
{
    View::cancel_and_delete_all_async_calls();
}
