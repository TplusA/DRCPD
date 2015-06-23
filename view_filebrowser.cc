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

#include <cstring>

#include "view_filebrowser.hh"
#include "dbus_iface_deep.h"
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

static void free_array_of_strings(gchar **const strings)
{
    if(strings == NULL)
        return;

    for(gchar **ptr = strings; *ptr != NULL; ++ptr)
        g_free(*ptr);

    g_free(strings);
}

static void send_selected_file_uri_to_streamplayer(ID::List list_id,
                                                   unsigned int item_id,
                                                   tdbuslistsNavigation *proxy)
{
    gchar **uri_list;
    guchar error_code;

    if(!tdbus_lists_navigation_call_get_uris_sync(proxy,
                                                  list_id.get_raw_id(), item_id,
                                                  &error_code,
                                                  &uri_list, NULL, NULL))
    {
        msg_info("Failed obtaining URI for item %u in list %u", item_id, list_id.get_raw_id());
        return;
    }

    if(error_code != 0)
    {
        msg_error(0, LOG_NOTICE,
                  "Got error code %u instead of URI for item %u in list %u",
                  error_code, item_id, list_id.get_raw_id());
        free_array_of_strings(uri_list);
        return;
    }

    if(uri_list == NULL || uri_list[0] == NULL)
    {
        msg_info("No URI for item %u in list %u", item_id, list_id.get_raw_id());
        free_array_of_strings(uri_list);
        return;
    }

    for(gchar **ptr = uri_list; *ptr != NULL; ++ptr)
        msg_info("URI: \"%s\"", *ptr);

    gchar *selected_uri = NULL;

    for(gchar **ptr = uri_list; *ptr != NULL; ++ptr)
    {
        const size_t len = strlen(*ptr);

        if(len < 4)
            continue;

        const gchar *const suffix = &(*ptr)[len - 4];

        if(strncasecmp(".m3u", suffix, 4) == 0 ||
           strncasecmp(".pls", suffix, 4) == 0)
            continue;

        if(selected_uri == NULL)
            selected_uri = *ptr;
    }

    msg_info("Queuing URI: \"%s\"", selected_uri);

    gboolean fifo_overflow;

    if(!tdbus_splay_urlfifo_call_push_sync(dbus_get_streamplayer_urlfifo_iface(),
                                           1234U, selected_uri, 0, "ms", 0, "ms", 0,
                                           &fifo_overflow, NULL, NULL))
        msg_error(EIO, LOG_NOTICE, "Failed queuing URI to streamplayer");
    else
    {
        if(fifo_overflow)
            msg_error(EAGAIN, LOG_INFO, "URL FIFO overflow");
        else if(!tdbus_splay_playback_call_start_sync(dbus_get_streamplayer_playback_iface(),
                                                      NULL, NULL))
            msg_error(EIO, LOG_NOTICE, "Failed sending start playback message");
        else if(!tdbus_splay_urlfifo_call_next_sync(dbus_get_streamplayer_urlfifo_iface(),
                                                    NULL, NULL))
            msg_error(EIO, LOG_NOTICE, "Failed activating queued URI in streamplayer");
    }

    free_array_of_strings(uri_list);
}

ViewIface::InputResult ViewFileBrowser::View::input(DrcpCommand command)
{
    switch(command)
    {
      case DrcpCommand::SELECT_ITEM:
      case DrcpCommand::KEY_OK_ENTER:
      case DrcpCommand::PLAYBACK_START:
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
                send_selected_file_uri_to_streamplayer(current_list_id_,
                                                       navigation_.get_cursor(),
                                                       file_list_.get_dbus_proxy());
        }

        return InputResult::OK;

      case DrcpCommand::PLAYBACK_STOP:
        if(!tdbus_splay_playback_call_stop_sync(dbus_get_streamplayer_playback_iface(),
                                                NULL, NULL))
            msg_error(EIO, LOG_NOTICE, "Failed sending stop playback message");

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
                     ID::List(list_id), 0))
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
                     ID::List(list_id), item_id))
        return true;
    else
        return point_to_root_directory();
}
