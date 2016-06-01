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

#include "view_filebrowser_airable.hh"
#include "view_filebrowser_utils.hh"
#include "de_tahifi_lists_context.h"
#include "messages.h"

void ViewFileBrowser::AirableView::logged_out_from_service_notification(const char *service_id,
                                                                        enum ActorID actor_id)
{
    msg_info("Logged out from \"%s\" by %u\n", service_id, actor_id);

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

void ViewFileBrowser::AirableView::handle_enter_list_event(List::AsyncListIface::OpResult result,
                                                           const std::shared_ptr<List::QueryContextEnterList> &ctx)
{
    View::handle_enter_list_event(result, ctx);

    root_list_id_ = current_list_id_;
}

bool ViewFileBrowser::AirableView::point_to_child_directory(const SearchParameters *search_parameters)
{
    if(current_list_id_ != root_list_id_ || search_parameters != nullptr)
        return View::point_to_child_directory(search_parameters);

    const unsigned int selected_line_from_root = navigation_.get_cursor();

    if(!View::point_to_child_directory())
        return false;

    log_assert(current_list_id_.is_valid());

    if(current_list_id_ == root_list_id_)
        return true;

    const List::context_id_t ctx_id(DBUS_LISTS_CONTEXT_GET(current_list_id_.get_raw_id()));

    if(search_forms_.find(ctx_id) != search_forms_.end())
    {
        /* already know the search form */
        return true;
    }

    const auto &ctx(list_contexts_[ctx_id]);

    if(!ctx.is_valid())
    {
        BUG("Attempted to find search form in invalid context %u", ctx_id);
        return true;
    }

    if(!ctx.check_flags(List::ContextInfo::HAS_PROPER_SEARCH_FORM) ||
       ctx.check_flags(List::ContextInfo::SEARCH_NOT_POSSIBLE))
    {
        BUG("Attempted to find nonexistent search form link in context %s",
            ctx.string_id_.c_str());
        return true;
    }

    const unsigned int num = file_list_.get_number_of_items();

    for(unsigned int i = 0; i < num; ++i)
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

            return true;
        }

        if(item == nullptr)
        {
            msg_error(0, LOG_ERR,
                      "Empty entry while searching for search form in context %s",
                      ctx.string_id_.c_str());

            return true;
        }

        if(item->get_kind().get() == ListItemKind::SEARCH_FORM)
        {
            msg_info("Found search form link for context %s: \"%s\" at /%u/%u",
                     ctx.string_id_.c_str(), item->get_text(),
                     selected_line_from_root, i);

            search_forms_.emplace(ctx_id, std::make_pair(selected_line_from_root, i));

            return true;
        }
    }

    BUG("Expected to find search form link for context %s in list %u",
        ctx.string_id_.c_str(), current_list_id_.get_raw_id());

    return true;
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

    const auto &path(form->second);

    const ID::List revert_to_list_id = current_list_id_;;
    const unsigned int revert_to_cursor = navigation_.get_cursor();

    try
    {
        Utils::enter_list_at(file_list_, item_flags_, navigation_,
                             root_list_id_, path.first);
        current_list_id_ = root_list_id_;

        const ID::List list_id =
            Utils::get_child_item_id(file_list_, current_list_id_,
                                     navigation_, nullptr);

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
    const auto &ctx(list_contexts_[context]);
    tdbus_airable_call_external_service_logout_sync(dbus_get_airable_sec_iface(),
                                                    ctx.string_id_.c_str(), "",
                                                    true, ACTOR_ID_LOCAL_UI,
                                                    NULL, NULL);
}
