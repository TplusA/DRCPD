/*
 * Copyright (C) 2015--2017, 2019, 2020  T+A elektroakustik GmbH & Co. KG
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

#ifndef VIEW_FILEBROWSER_UTILS_HH
#define VIEW_FILEBROWSER_UTILS_HH

#include "listnav.hh"
#include "search_parameters.hh"
#include "rnfcall_get_list_id.hh"

namespace
{
    /*!
     * Get child item ID.
     *
     * WARNING: Do not write new client code which calls this function. It
     *     makes a blocking D-Bus method call which may block for a long time
     *     (usually seconds, potentially forever)! Please use non-blocking
     *     calls instead.
     */
    static DBusRNF::GetListIDResult
    get_child_item_internal(List::DBusList &file_list, ID::List current_list_id,
                            List::Nav &navigation,
                            const SearchParameters *search_parameters,
                            DBusRNF::StatusWatcher &&status_watcher)
    {
        if(search_parameters == nullptr)
        {
            DBusRNF::GetListIDCall
                call(file_list.get_cookie_manager(), file_list.get_dbus_proxy(),
                     current_list_id, navigation.get_cursor(),
                     nullptr, std::move(status_watcher));
            call.request();
            call.fetch_blocking();

            try
            {
                return call.get_result_locked();
            }
            catch(const DBusRNF::AbortedError &)
            {
                throw List::DBusListException(ListError::INTERRUPTED);
            }
            catch(const DBusRNF::BadStateError &)
            {
                throw List::DBusListException(ListError::INTERNAL);
            }
            catch(const DBusRNF::NoResultError &)
            {
                throw List::DBusListException(ListError::INTERNAL);
            }
        }
        else
        {
            DBusRNF::GetParameterizedListIDCall
                call(file_list.get_cookie_manager(), file_list.get_dbus_proxy(),
                     current_list_id, navigation.get_cursor(),
                     std::string(search_parameters->get_query()),
                     nullptr, std::move(status_watcher));
            call.request();
            call.fetch_blocking();

            try
            {
                return call.get_result_locked();
            }
            catch(const DBusRNF::AbortedError &)
            {
                throw List::DBusListException(ListError::INTERRUPTED);
            }
            catch(const DBusRNF::BadStateError &)
            {
                throw List::DBusListException(ListError::INTERNAL);
            }
            catch(const DBusRNF::NoResultError &)
            {
                throw List::DBusListException(ListError::INTERNAL);
            }
        }
    }
}

/*!
 * \addtogroup view_filesystem
 */
/*!@{*/

namespace ViewFileBrowser
{

class Utils
{
  private:
    explicit Utils();

  public:
    /*!
     * Change cursor or enter new list.
     *
     * After moving the cursor, this function notifies the list filter and
     * updates the navigation state.
     */
    static void enter_list_at(List::DBusList &file_list,
                              List::NavItemNoFilter &item_flags,
                              List::Nav &navigation,
                              ID::List list_id, unsigned int line,
                              bool reverse = false)
    {
        file_list.enter_list(list_id);
        item_flags.list_content_changed();

        const unsigned int lines = navigation.get_total_number_of_visible_items();

        if(lines == 0)
            line = 0;
        else if(!reverse)
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

        navigation.set_cursor_by_line_number(line);
    }

    /*
     * Get child item ID, synchronously.
     *
     * \bug Synchronous D-Bus call of potentially long-running method.
     */
    static ID::List get_child_item_id(List::DBusList &file_list,
                                      ID::List current_list_id,
                                      List::Nav &navigation,
                                      const SearchParameters *search_parameters,
                                      DBusRNF::StatusWatcher &&status_watcher,
                                      std::string &child_list_title,
                                      bool suppress_error_if_file = false)
    {
        if(file_list.empty())
            return ID::List();

        try
        {
            auto result(get_child_item_internal(file_list, current_list_id,
                                                navigation, search_parameters,
                                                std::move(status_watcher)));

            if(result.list_id_.is_valid())
            {
                child_list_title = std::move(std::string(result.title_.get_text()));
                return result.list_id_;
            }

            if(!suppress_error_if_file)
            {
                msg_error(0, LOG_NOTICE,
                          "Error obtaining ID for item %u in list %u, error code %s",
                          navigation.get_cursor(), current_list_id.get_raw_id(),
                          result.error_.to_string());

                if(result.error_ != ListError::Code::OK)
                    throw List::DBusListException(result.error_);
            }
        }
        catch(...)
        {
            if(!suppress_error_if_file)
                throw;
        }

        child_list_title.clear();
        return ID::List();
    }

    static ID::List get_parent_link_id(const List::DBusList &file_list,
                                       ID::List current_list_id,
                                       unsigned int &item_id,
                                       std::string &parent_list_title)
    {
        Busy::set(Busy::Source::GETTING_PARENT_LINK);

        guint list_id;
        gchar *list_title = nullptr;
        gboolean list_title_translatable = FALSE;
        GErrorWrapper error;

        tdbus_lists_navigation_call_get_parent_link_sync(file_list.get_dbus_proxy(),
                                                         current_list_id.get_raw_id(),
                                                         &list_id, &item_id, &list_title,
                                                         &list_title_translatable,
                                                         nullptr, error.await());

        if(error.log_failure("Get parent link"))
        {
            Busy::clear(Busy::Source::GETTING_PARENT_LINK);

            msg_vinfo(MESSAGE_LEVEL_IMPORTANT,
                      "Failed obtaining parent for list %u",
                      current_list_id.get_raw_id());

            throw List::DBusListException(error);
        }

        Busy::clear(Busy::Source::GETTING_PARENT_LINK);

        if(list_id != 0)
        {
            if(list_title != nullptr)
                parent_list_title = list_title;
            else
                parent_list_title.clear();

            return ID::List(list_id);
        }

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

}

/*!@}*/

#endif /* !VIEW_FILEBROWSER_UTILS_HH */
