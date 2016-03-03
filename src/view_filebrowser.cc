/*
 * Copyright (C) 2015, 2016  T+A elektroakustik GmbH & Co. KG
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

#include "view_filebrowser.hh"
#include "view_filebrowser_utils.hh"
#include "view_search.hh"
#include "view_manager.hh"
#include "view_names.hh"
#include "player.hh"
#include "xmlescape.hh"
#include "messages.h"

List::Item *ViewFileBrowser::construct_file_item(const char *name,
                                                 ListItemKind kind)
{
    return new FileItem(name, 0, kind);
}

bool ViewFileBrowser::View::init()
{
    (void)point_to_root_directory();
    return true;
}

bool ViewFileBrowser::View::late_init()
{
    search_parameters_view_=
        dynamic_cast<ViewSearch::View *>(view_manager_->get_view_by_name(ViewNames::SEARCH_OPTIONS));

    return search_parameters_view_ != nullptr;
}

void ViewFileBrowser::View::focus()
{
    if(!current_list_id_.is_valid())
        (void)point_to_root_directory();
}

static inline void stop_waiting_for_search_parameters(ViewIface &view)
{
    static_cast<ViewSearch::View &>(view).forget_parameters();
}

void ViewFileBrowser::View::defocus()
{
    waiting_for_search_parameters_ = false;
    stop_waiting_for_search_parameters(*search_parameters_view_);
}

static bool request_search_parameters_from_user(ViewManager::VMIface &vm,
                                                ViewSearch::View &view,
                                                const SearchParameters *&params)
{
    params = view.get_parameters();

    if(params != nullptr)
        return false;

    view.request_parameters_for_context("dummy");
    vm.serialize_view_forced(&view);

    return true;
}

bool ViewFileBrowser::View::apply_search_parameters()
{
    const auto *params =
        static_cast<ViewSearch::View *>(search_parameters_view_)->get_parameters();
    log_assert(params != nullptr);

    const bool retval = point_to_child_directory(params);

    stop_waiting_for_search_parameters(*search_parameters_view_);

    return retval;
}

class WaitForParametersHelper
{
  private:
    bool &waiting_state_;
    const bool have_preloaded_parameters_;
    const std::function<void(void)> stop_waiting_fn_;

    bool wait_on_exit_;
    bool keep_preloaded_parameters_;

  public:
    WaitForParametersHelper(const WaitForParametersHelper &) = delete;
    WaitForParametersHelper &operator=(const WaitForParametersHelper &) = delete;

    explicit WaitForParametersHelper(bool &waiting_state,
                                     bool have_preloaded_parameters,
                                     const std::function<void(void)> &stop_waiting_fn):
        waiting_state_(waiting_state),
        have_preloaded_parameters_(have_preloaded_parameters),
        stop_waiting_fn_(stop_waiting_fn),
        wait_on_exit_(false),
        keep_preloaded_parameters_(false)
    {}

    ~WaitForParametersHelper()
    {
        if((waiting_state_ && !wait_on_exit_) ||
           (have_preloaded_parameters_ && !keep_preloaded_parameters_))
        {
            waiting_state_ = false;
            stop_waiting_fn_();
        }

        if(wait_on_exit_)
            waiting_state_ = true;
    }

    bool was_waiting() const { return waiting_state_; }
    void keep_waiting() { wait_on_exit_ = true; }
    void keep_parameters() { keep_preloaded_parameters_ = true; }
};

ViewIface::InputResult ViewFileBrowser::View::input(DrcpCommand command,
                                                    std::unique_ptr<const UI::Parameters> parameters)
{
    WaitForParametersHelper wait_helper(
        waiting_for_search_parameters_,
        static_cast<ViewSearch::View *>(search_parameters_view_)->get_parameters() != nullptr,
        [this] ()
        {
            stop_waiting_for_search_parameters(*search_parameters_view_);
        });

    switch(command)
    {
      case DrcpCommand::X_TA_SEARCH_PARAMETERS:
        if(!wait_helper.was_waiting())
        {
            wait_helper.keep_parameters();
            break;
        }

        if(apply_search_parameters())
            return InputResult::UPDATE_NEEDED;

        break;

      case DrcpCommand::SELECT_ITEM:
      case DrcpCommand::KEY_OK_ENTER:
        if(file_list_.empty())
            return InputResult::OK;

        const FileItem *item;

        try
        {
            item = dynamic_cast<decltype(item)>(file_list_.get_item(navigation_.get_cursor()));
        }
        catch(const List::DBusListException &e)
        {
            item = nullptr;
        }

        if(item)
        {
            switch(item->get_kind().get())
            {
              case ListItemKind::LOCKED:
                /* don't even try */
                break;

              case ListItemKind::DIRECTORY:
              case ListItemKind::PLAYLIST_DIRECTORY:
              case ListItemKind::SERVER:
              case ListItemKind::STORAGE_DEVICE:
                if(point_to_child_directory())
                    return InputResult::UPDATE_NEEDED;

                break;

              case ListItemKind::REGULAR_FILE:
              case ListItemKind::PLAYLIST_FILE:
              case ListItemKind::OPAQUE:
                command = DrcpCommand::PLAYBACK_START;
                break;

              case ListItemKind::SEARCH_FORM:
                if(wait_helper.was_waiting())
                {
                    /* quick/impatient user, keep up waiting state */
                    wait_helper.keep_waiting();
                    break;
                }

                const SearchParameters *params = nullptr;

                if(request_search_parameters_from_user(*view_manager_,
                                                       *static_cast<ViewSearch::View *>(search_parameters_view_),
                                                       params))
                    wait_helper.keep_waiting();

                if(params == nullptr)
                {
                    /* just do nothing, i.e., keep idling, wait until the
                     * search parameters are sent, or stop waiting when the
                     * user does something else */
                    break;
                }

                /* got preloaded parameters */
                if(apply_search_parameters())
                    return InputResult::UPDATE_NEEDED;

                break;
            }
        }

        if(command != DrcpCommand::PLAYBACK_START)
            return InputResult::OK;

        /* fall-through: command was changed to #DrcpCommand::PLAYBACK_START
         *               because the item below the cursor was not a
         *               directory */

      case DrcpCommand::PLAYBACK_START:
        if(file_list_.empty())
            return InputResult::OK;

        playback_current_mode_.activate_selected_mode();

        if(!player_.take(playback_current_state_, file_list_,
                         navigation_.get_line_number_by_cursor(),
                         [this] (bool is_buffering)
                         {
                             view_manager_->activate_view_by_name(is_buffering
                                                                  ? ViewNames::PLAYER
                                                                  : name_);
                         }))
            player_.release(true);

        return InputResult::OK;

      case DrcpCommand::GO_BACK_ONE_LEVEL:
        return point_to_parent_link() ? InputResult::UPDATE_NEEDED : InputResult::OK;

      case DrcpCommand::SCROLL_DOWN_ONE:
        return navigation_.down() ? InputResult::UPDATE_NEEDED : InputResult::OK;

      case DrcpCommand::SCROLL_UP_ONE:
        return navigation_.up() ? InputResult::UPDATE_NEEDED : InputResult::OK;

      case DrcpCommand::SCROLL_PAGE_DOWN:
      {
        bool moved =
            ((navigation_.distance_to_bottom() == 0)
             ? navigation_.down(navigation_.maximum_number_of_displayed_lines_)
             : navigation_.down(navigation_.distance_to_bottom()));
        return moved ? InputResult::UPDATE_NEEDED : InputResult::OK;
      }

      case DrcpCommand::SCROLL_PAGE_UP:
      {
        bool moved =
            ((navigation_.distance_to_top() == 0)
             ? navigation_.up(navigation_.maximum_number_of_displayed_lines_)
             : navigation_.up(navigation_.distance_to_top()));
        return moved ? InputResult::UPDATE_NEEDED : InputResult::OK;
      }

      default:
        break;
    }

    return InputResult::OK;
}

bool ViewFileBrowser::View::write_xml(std::ostream &os,
                                      const DCP::Queue::Data &data)
{
    os << "<text id=\"cbid\">" << int(drcp_browse_id_) << "</text>";

    size_t displayed_line = 0;

    for(auto it : navigation_)
    {
        const FileItem *item;

        try
        {
            item = dynamic_cast<decltype(item)>(file_list_.get_item(it));
        }
        catch(const List::DBusListException &e)
        {
            item = nullptr;
        }

        if(item == nullptr)
        {
            /* we do not abort the serialization even in case of error,
             * otherwise the user would see no update at all */
            return true;
        }

        std::string flags;

        switch(item->get_kind().get())
        {
          case ListItemKind::DIRECTORY:
            flags.push_back('d');
            break;

          case ListItemKind::SERVER:
            flags.push_back('S');
            break;

          case ListItemKind::STORAGE_DEVICE:
            flags.push_back('D');
            break;

          case ListItemKind::REGULAR_FILE:
            flags.push_back('p');
            break;

          case ListItemKind::LOCKED:
            flags += "ul";
            break;

          case ListItemKind::PLAYLIST_FILE:
            flags += "pL";
            break;

          case ListItemKind::PLAYLIST_DIRECTORY:
            flags += "dL";
            break;

          case ListItemKind::OPAQUE:
            flags.push_back('u');
            break;

          case ListItemKind::SEARCH_FORM:
            flags.push_back('q');
            break;
        }

        if(it == navigation_.get_cursor())
            flags.push_back('s');

        os << "<text id=\"line" << displayed_line << "\" flag=\"" << flags << "\">"
           << XmlEscape(item->get_text()) << "</text>";

        ++displayed_line;
    }

    os << "<value id=\"listpos\" min=\"1\" max=\""
       << navigation_.get_total_number_of_visible_items() << "\">"
       << navigation_.get_line_number_by_cursor() + 1
       << "</value>";

    return true;
}

void ViewFileBrowser::View::serialize(DCP::Queue &queue, std::ostream *debug_os)
{
    ViewSerializeBase::serialize(queue);

    if(!debug_os)
        return;

    for(auto it : navigation_)
    {
        const FileItem *item;

        try
        {
            item = dynamic_cast<decltype(item)>(file_list_.get_item(it));
        }
        catch(const List::DBusListException &e)
        {
            item = nullptr;
        }

        if(it == navigation_.get_cursor())
            *debug_os << "--> ";
        else
            *debug_os << "    ";

        if(item != nullptr)
            *debug_os << "Type " << item->get_kind().get_raw_code() << " " << it << ": "
                      << item->get_text() << std::endl;
        else
            *debug_os << "*NULL ENTRY* " << it << std::endl;
    }
}

void ViewFileBrowser::View::update(DCP::Queue &queue, std::ostream *debug_os)
{
    serialize(queue, debug_os);
}

bool ViewFileBrowser::View::owns_dbus_proxy(const void *dbus_proxy) const
{
    return dbus_proxy == file_list_.get_dbus_proxy();
}

bool ViewFileBrowser::View::list_invalidate(ID::List list_id, ID::List replacement_id)
{
    log_assert(list_id.is_valid());

    if(playback_current_state_.list_invalidate(list_id, replacement_id))
        player_.release(true);

    if(list_id != current_list_id_)
        return false;

    if(replacement_id.is_valid())
    {
        msg_info("Reloading list %u (was %u)",
                 replacement_id.get_raw_id(), current_list_id_.get_raw_id());

        current_list_id_ = replacement_id;
        reload_list();
    }
    else
    {
        msg_info("Current list %u got removed, going back to root list",
                 current_list_id_.get_raw_id());
        point_to_root_directory();
    }

    return true;
}

static ID::List go_to_root_directory(List::DBusList &file_list,
                                     List::NavItemNoFilter &item_flags,
                                     List::Nav &navigation)
    throw(List::DBusListException)
{
    guint list_id;
    guchar error_code;

    if(!tdbus_lists_navigation_call_get_list_id_sync(file_list.get_dbus_proxy(),
                                                     0, 0, &error_code,
                                                     &list_id, NULL, NULL))
    {
        /* this is not a hard error, it may only mean that the list broker
         * hasn't started up yet */
        msg_info("Failed obtaining ID for root list");

        throw List::DBusListException(ListError::Code::INTERNAL, true);
    }

    const ListError error(error_code);

    if(error != ListError::Code::OK)
    {
        msg_error(0, LOG_NOTICE,
                  "Got error for root list ID, error code %s", error.to_string());

        throw List::DBusListException(error);
    }

    if(list_id == 0)
    {
        BUG("Got invalid list ID for root list, but no error code");

        throw List::DBusListException(ListError::Code::INVALID_ID);
    }

    const ID::List id(list_id);

    ViewFileBrowser::enter_list_at(file_list, item_flags, navigation, id, 0);

    return id;
}

bool ViewFileBrowser::View::point_to_root_directory()
{
    try
    {
        current_list_id_ =
            go_to_root_directory(file_list_, item_flags_, navigation_);

        return true;
    }
    catch(const List::DBusListException &e)
    {
        /* uh-oh... */
        if(!e.is_dbus_error())
            current_list_id_ = ID::List();
    }

    return false;
}

bool ViewFileBrowser::View::point_to_child_directory(const SearchParameters *search_parameters)
{
    try
    {
        ID::List list_id =
            get_child_item_id(file_list_, current_list_id_, navigation_ ,
                              search_parameters);

        if(!list_id.is_valid())
            return false;

        enter_list_at(file_list_, item_flags_, navigation_, list_id, 0);
        current_list_id_ = list_id;

        return true;
    }
    catch(const List::DBusListException &e)
    {
        if(e.is_dbus_error())
        {
            /* probably just a temporary problem */
            return false;
        }

        switch(e.get())
        {
          case ListError::Code::INTERRUPTED:
          case ListError::Code::PHYSICAL_MEDIA_IO:
          case ListError::Code::NET_IO:
          case ListError::Code::PROTOCOL:
          case ListError::Code::AUTHENTICATION:
          case ListError::Code::INCONSISTENT:
          case ListError::Code::PERMISSION_DENIED:
          case ListError::Code::NOT_SUPPORTED:
            /* problem: stay right there where you are */
            msg_info("Go to child list error, stay here: %s", e.what());
            return false;

          case ListError::Code::OK:
          case ListError::Code::INTERNAL:
          case ListError::Code::INVALID_ID:
            /* funny problem: better return to root directory */
            msg_info("Go to child list error, try go to root: %s", e.what());
            break;
        }
    }

    return point_to_root_directory();
}

bool ViewFileBrowser::View::point_to_parent_link()
{
    try
    {
        unsigned int item_id;
        ID::List list_id =
            get_parent_link_id(file_list_, current_list_id_, item_id);

        if(list_id.is_valid())
        {
            enter_list_at(file_list_, item_flags_, navigation_, list_id, item_id);
            current_list_id_ = list_id;

            return true;
        }
    }
    catch(const List::DBusListException &e)
    {
        if(e.is_dbus_error())
        {
            /* probably just a temporary problem */
            return false;
        }

        switch(e.get())
        {
          case ListError::Code::INTERRUPTED:
          case ListError::Code::PHYSICAL_MEDIA_IO:
          case ListError::Code::NET_IO:
          case ListError::Code::PROTOCOL:
            /* problem: stay right there where you are */
            return false;

          case ListError::Code::OK:
          case ListError::Code::INTERNAL:
          case ListError::Code::INVALID_ID:
          case ListError::Code::AUTHENTICATION:
          case ListError::Code::INCONSISTENT:
          case ListError::Code::PERMISSION_DENIED:
          case ListError::Code::NOT_SUPPORTED:
            /* funny problem: better return to root directory */
            break;
        }
    }

    return point_to_root_directory();
}

void ViewFileBrowser::View::reload_list()
{
    int line = navigation_.get_line_number_by_cursor();

    if(line >= 0)
    {
        try
        {
            enter_list_at(file_list_, item_flags_, navigation_, current_list_id_, line);
            return;
        }
        catch(const List::DBusListException &e)
        {
            /* handled below */
        }
    }

    point_to_root_directory();
}
