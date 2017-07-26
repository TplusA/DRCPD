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

void ViewFileBrowser::AirableView::logged_out_from_service_notification(const char *service_id,
                                                                        enum ActorID actor_id)
{
    msg_vinfo(MESSAGE_LEVEL_IMPORTANT, "Logged out from \"%s\" by %u\n",
              service_id, actor_id);

    List::context_id_t ctx_id;
    const auto &ctx(list_contexts_.get_context_info_by_string_id(service_id, ctx_id));

    if(!ctx.is_valid())
        return;

    const List::context_id_t current_browse_context =
        DBUS_LISTS_CONTEXT_GET(current_list_id_.get_raw_id());

    if(current_browse_context == ctx_id)
        point_to_root_directory();

    search_forms_.erase(ctx_id);
}

bool ViewFileBrowser::AirableView::register_audio_sources()
{
    log_assert(default_audio_source_name_ == nullptr);

    if(list_contexts_.empty())
    {
        BUG("No list contexts, cannot create audio sources");
        return false;
    }

    selected_audio_source_index_ = 0;

    for(const auto &ctx : list_contexts_)
        audio_sources_.emplace_back(Player::AudioSource(ctx.string_id_.c_str()));

    auto *const pview = static_cast<ViewPlay::View *>(play_view_);
    size_t i = 0;

    for(const auto &ctx : list_contexts_)
    {
        pview->register_audio_source(audio_sources_[i], *this);
        tdbus_aupath_manager_call_register_source(dbus_audiopath_get_manager_iface(),
                                                  audio_sources_[i].id_,
                                                  ctx.description_.c_str(),
                                                  "strbo",
                                                  "/de/tahifi/Drcpd",
                                                  nullptr,
                                                  audio_source_registered,
                                                  &audio_sources_[i]);
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

    if(!is_login)
        logged_out_from_service_notification(service_id.c_str(), actor_id);
    else
        msg_vinfo(MESSAGE_LEVEL_IMPORTANT, "Logged into \"%s\" by %u\n",
                  service_id.c_str(), actor_id);

    return InputResult::OK;
}

bool ViewFileBrowser::AirableView::list_invalidate(ID::List list_id, ID::List replacement_id)
{
    if(replacement_id == root_list_id_)
    {
        root_list_id_ = list_id;
        search_forms_.clear();
    }

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
    if(current_list_id_ == root_list_id_)
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
      case List::QueryContextEnterList::CallerID::ENTER_PARENT:
      case List::QueryContextEnterList::CallerID::RELOAD_LIST:
      case List::QueryContextEnterList::CallerID::CRAWLER_RESTART:
      case List::QueryContextEnterList::CallerID::CRAWLER_RESET_POSITION:
      case List::QueryContextEnterList::CallerID::CRAWLER_DESCEND:
      case List::QueryContextEnterList::CallerID::CRAWLER_ASCEND:
        break;

      case List::QueryContextEnterList::CallerID::ENTER_CHILD:
        finish_async_point_to_child_directory();
        break;

      case List::QueryContextEnterList::CallerID::ENTER_ROOT:
        root_list_id_ = current_list_id_;
        break;
    }

    View::handle_enter_list_event_update_after_finish(result, ctx);
}

bool ViewFileBrowser::AirableView::point_to_child_directory(const SearchParameters *search_parameters)
{
    if(current_list_id_ != root_list_id_ || search_parameters != nullptr)
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
                             root_list_id_, path.first);
        current_list_id_ = root_list_id_;

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

void ViewFileBrowser::AirableView::cancel_and_delete_all_async_calls()
{
    View::cancel_and_delete_all_async_calls();
}
