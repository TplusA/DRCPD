/*
 * Copyright (C) 2015  T+A elektroakustik GmbH & Co. KG
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
#include "xmlescape.hh"
#include "messages.h"

List::Item *ViewFileBrowser::construct_file_item(const char *name,
                                                 bool is_directory)
{
    return new FileItem(name, 0, is_directory);
}

bool ViewFileBrowser::View::init()
{
    point_to_root_directory();
    return true;
}

void ViewFileBrowser::View::focus()
{
    if(!current_list_id_.is_valid())
        (void)point_to_root_directory();
}

void ViewFileBrowser::View::defocus()
{
}

ViewIface::InputResult ViewFileBrowser::View::input(DrcpCommand command)
{
    switch(command)
    {
      case DrcpCommand::SELECT_ITEM:
      case DrcpCommand::KEY_OK_ENTER:
        if(file_list_.empty())
            return InputResult::OK;

        if(auto item = dynamic_cast<const FileItem *>(file_list_.get_item(navigation_.get_cursor())))
        {
            if(item->is_directory())
            {
                if(point_to_child_directory())
                    return InputResult::UPDATE_NEEDED;
            }
            else
                command = DrcpCommand::PLAYBACK_START;
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

        if(playback_current_state_.start(file_list_,
                                         navigation_.get_line_number_by_cursor()))
            playback_current_state_.enqueue_next();
        else
            playback_current_mode_.deactivate();

        return InputResult::OK;

      case DrcpCommand::PLAYBACK_STOP:
        if(!tdbus_splay_playback_call_stop_sync(dbus_get_streamplayer_playback_iface(),
                                                NULL, NULL))
            msg_error(0, LOG_NOTICE, "Failed sending stop playback message");

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

bool ViewFileBrowser::View::write_xml(std::ostream &os, bool is_full_view)
{
    os << "    <text id=\"cbid\">" << int(drcp_browse_id_) << "</text>\n";

    size_t displayed_line = 0;

    for(auto it : navigation_)
    {
        auto item = dynamic_cast<const FileItem *>(file_list_.get_item(it));
        std::string flags;

        if(item != nullptr)
        {
            if(item->is_directory())
                flags.push_back('d');
            else
                flags.push_back('p');
        }

        if(it == navigation_.get_cursor())
            flags.push_back('s');

        os << "    <text id=\"line" << displayed_line << "\" flag=\"" << flags << "\">"
           << XmlEscape(item != nullptr ? item->get_text() : "-----") << "</text>\n";

        ++displayed_line;
    }

    os << "    <value id=\"listpos\" min=\"1\" max=\""
       << navigation_.get_total_number_of_visible_items() << "\">"
       << navigation_.get_line_number_by_cursor() + 1
       << "</value>\n";

    return true;
}

bool ViewFileBrowser::View::serialize(DcpTransaction &dcpd, std::ostream *debug_os)
{
    const bool retval = ViewIface::serialize(dcpd);

    if(!debug_os)
        return retval;

    for(auto it : navigation_)
    {
        auto item = dynamic_cast<const FileItem *>(file_list_.get_item(it));

        if(it == navigation_.get_cursor())
            *debug_os << "--> ";
        else
            *debug_os << "    ";

        if(item != nullptr)
            *debug_os << (item->is_directory() ? "Dir " : "File") << " " << it << ": "
                      << item->get_text() << std::endl;
        else
            *debug_os << "*NULL ENTRY* " << it << std::endl;
    }

    return retval;
}

bool ViewFileBrowser::View::update(DcpTransaction &dcpd, std::ostream *debug_os)
{
    return serialize(dcpd, debug_os);
}

void ViewFileBrowser::View::notify_stream_start(uint32_t id,
                                                const std::string &url,
                                                bool url_fifo_is_full)
{
    playback_current_state_.enqueue_next();
}

void ViewFileBrowser::View::notify_stream_stop()
{
    playback_current_state_.revert();
}

static bool go_to_root_directory(List::DBusList &file_list,
                                 ID::List &current_list_id,
                                 List::NavItemNoFilter &item_flags,
                                 List::Nav &navigation)
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
        current_list_id = ID::List();
        return false;
    }

    const ListError error(error_code);

    if(error == ListError::Code::OK)
    {
        if(list_id > 0)
            return ViewFileBrowser::enter_list_at(file_list, current_list_id,
                                                  item_flags,
                                                  navigation, ID::List(list_id), 0);

        BUG("Got invalid list ID for root list, but no error code");
    }
    else
        msg_error(0, LOG_NOTICE,
                  "Got error for root list ID, error code %s", error.to_string());

    return false;
}

bool ViewFileBrowser::View::point_to_root_directory()
{
    return go_to_root_directory(file_list_, current_list_id_,
                                item_flags_, navigation_);
}

bool ViewFileBrowser::View::point_to_child_directory()
{
    ID::List list_id =
        get_child_item_id(file_list_, current_list_id_, navigation_);

    if(!list_id.is_valid())
        return false;

    if(enter_list_at(file_list_, current_list_id_, item_flags_, navigation_,
                     list_id, 0))
        return true;
    else
        return point_to_root_directory();
}

bool ViewFileBrowser::View::point_to_parent_link()
{
    unsigned int item_id;
    ID::List list_id = get_parent_link_id(file_list_, current_list_id_, item_id);

    if(!list_id.is_valid())
    {
        if(item_id == 1)
            return false;
        else
            return point_to_root_directory();
    }

    if(enter_list_at(file_list_, current_list_id_, item_flags_, navigation_,
                     list_id, item_id))
        return true;
    else
        return point_to_root_directory();
}
