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

#ifndef VIEW_FILEBROWSER_UTILS_HH
#define VIEW_FILEBROWSER_UTILS_HH

#include "dbuslist.hh"
#include "de_tahifi_lists_errors.hh"
#include "listnav.hh"

/*!
 * \addtogroup view_filesystem
 */
/*!@{*/

namespace ViewFileBrowser
{

/*!
 * Change cursor or enter new list.
 *
 * After moving the cursor, this function notifies the list filter and
 * updates the navigation state.
 */
static void enter_list_at(List::DBusList &file_list,
                          List::NavItemNoFilter &item_flags,
                          List::Nav &navigation,
                          ID::List list_id, unsigned int line)
    throw(List::DBusListException)
{
    file_list.enter_list(list_id, line);
    item_flags.list_content_changed();
    navigation.set_cursor_by_line_number(line);
}

static ID::List get_child_item_id(const List::DBusList &file_list,
                                  ID::List current_list_id,
                                  List::Nav &navigation,
                                  bool suppress_error_if_file = false)
    throw(List::DBusListException)
{
    if(file_list.empty())
        return ID::List();

    guint list_id;
    guchar error_code;

    if(!tdbus_lists_navigation_call_get_list_id_sync(file_list.get_dbus_proxy(),
                                                     current_list_id.get_raw_id(),
                                                     navigation.get_cursor(),
                                                     &error_code,
                                                     &list_id, NULL, NULL))
    {
        msg_info("Failed obtaining ID for item %u in list %u",
                 navigation.get_cursor(), current_list_id.get_raw_id());

        throw List::DBusListException(ListError::Code::INTERNAL, true);
    }

    const ListError error(error_code);

    if(list_id == 0 && !suppress_error_if_file)
    {
        msg_error(0, LOG_NOTICE,
                  "Error obtaining ID for item %u in list %u, error code %s",
                  navigation.get_cursor(), current_list_id.get_raw_id(),
                  error.to_string());

        if(error != ListError::Code::OK)
            throw List::DBusListException(error);

        return ID::List();
    }
    else
        return ID::List(list_id);
}

static ID::List get_parent_link_id(const List::DBusList &file_list,
                                   ID::List current_list_id,
                                   unsigned int &item_id)
    throw(List::DBusListException)
{
    guint list_id;

    if(!tdbus_lists_navigation_call_get_parent_link_sync(file_list.get_dbus_proxy(),
                                                         current_list_id.get_raw_id(),
                                                         &list_id, &item_id,
                                                         NULL, NULL))
    {
        msg_info("Failed obtaining parent for list %u", current_list_id.get_raw_id());

        throw List::DBusListException(ListError::Code::INTERNAL, true);
    }

    if(list_id != 0)
        return ID::List(list_id);

    if(item_id == 1)
    {
        /* requested parent of root node */
        return ID::List();
    }

    msg_error(0, LOG_NOTICE,
              "Error obtaining parent for list %u", current_list_id.get_raw_id());

    throw List::DBusListException(ListError::Code::INVALID_ID);
}

};

/*!@}*/

#endif /* !VIEW_FILEBROWSER_UTILS_HH */