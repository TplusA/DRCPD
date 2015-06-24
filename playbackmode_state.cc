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

#include "playbackmode_state.hh"
#include "view_filebrowser.hh"
#include "view_filebrowser_utils.hh"
#include "dbus_iface_deep.h"

enum class SendStatus
{
    OK,
    NO_URI,
    FIFO_FAILURE,
    FIFO_FULL,
    PLAYBACK_FAILURE,
};

static void free_array_of_strings(gchar **const strings)
{
    if(strings == NULL)
        return;

    for(gchar **ptr = strings; *ptr != NULL; ++ptr)
        g_free(*ptr);

    g_free(strings);
}

static SendStatus send_selected_file_uri_to_streamplayer(ID::List list_id,
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
        return SendStatus::NO_URI;
    }

    if(error_code != 0)
    {
        msg_error(0, LOG_NOTICE,
                  "Got error code %u instead of URI for item %u in list %u",
                  error_code, item_id, list_id.get_raw_id());
        free_array_of_strings(uri_list);
        return SendStatus::NO_URI;
    }

    if(uri_list == NULL || uri_list[0] == NULL)
    {
        msg_info("No URI for item %u in list %u", item_id, list_id.get_raw_id());
        free_array_of_strings(uri_list);
        return SendStatus::NO_URI;
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
    SendStatus ret;

    if(!tdbus_splay_urlfifo_call_push_sync(dbus_get_streamplayer_urlfifo_iface(),
                                           1234U, selected_uri, 0, "ms", 0, "ms", 0,
                                           &fifo_overflow, NULL, NULL))
    {
        msg_error(EIO, LOG_NOTICE, "Failed queuing URI to streamplayer");
        ret = SendStatus::FIFO_FAILURE;
    }
    else
    {
        if(fifo_overflow)
        {
            msg_error(EAGAIN, LOG_INFO, "URL FIFO overflow");
            ret = SendStatus::FIFO_FULL;
        }
        else if(!tdbus_splay_playback_call_start_sync(dbus_get_streamplayer_playback_iface(),
                                                      NULL, NULL))
        {
            msg_error(EIO, LOG_NOTICE, "Failed sending start playback message");
            ret = SendStatus::PLAYBACK_FAILURE;
        }
        else if(!tdbus_splay_urlfifo_call_next_sync(dbus_get_streamplayer_urlfifo_iface(),
                                                    NULL, NULL))
        {
            msg_error(EIO, LOG_NOTICE, "Failed activating queued URI in streamplayer");
            ret = SendStatus::PLAYBACK_FAILURE;
        }
        else
            ret = SendStatus::OK;
    }

    free_array_of_strings(uri_list);

    return ret;
}

bool Playback::State::try_start()
{
    start_list_id_ = user_list_id_;
    start_list_line_ = user_list_line_;

    if(!ViewFileBrowser::enter_list_at(*dbus_list_object_, current_list_id_,
                                       item_flags_, navigation_,
                                       start_list_id_, start_list_line_))
        return false;

    if(!try_descend())
        return true;

    start_list_id_ = current_list_id_;
    start_list_line_ = 0;

    directory_depth_ = 1;
    number_of_directories_entered_ = 0;

    return true;
}

bool Playback::State::start(unsigned int start_line)
{
    if(mode_.get() == Playback::Mode::NONE)
        return false;

    user_list_id_ = dbus_list_object_->get_list_id();
    user_list_line_ = start_line;

    directory_depth_ = 1;
    number_of_streams_played_ = 0;
    number_of_streams_skipped_ = 0;
    number_of_directories_entered_ = 0;

    if(try_start())
        return true;

    current_list_id_ = ID::List();

    return false;
}

void Playback::State::enqueue_next()
{
    if(!mode_.is_playing())
        return;

    while(true)
    {
        auto item = dynamic_cast<const ViewFileBrowser::FileItem *>(dbus_list_object_->get_item(navigation_.get_cursor()));

        if(!item)
        {
            if(find_next(nullptr))
                continue;

            revert();

            return;
        }

        if(!item->is_directory())
        {
            item = nullptr;

            switch(send_selected_file_uri_to_streamplayer(current_list_id_,
                                                          navigation_.get_cursor(),
                                                          dbus_list_object_->get_dbus_proxy()))
            {
              case SendStatus::OK:
                /* stream URI is in FIFO now */
                ++number_of_streams_played_;
                break;

              case SendStatus::NO_URI:
                /* that's life... just ignore this entry */
                break;

              case SendStatus::FIFO_FAILURE:
                /* trying to put something into the FIFO failed hard */
                revert();

                return;

              case SendStatus::FIFO_FULL:
                /* try again in a later invokation of this function */
                return;

              case SendStatus::PLAYBACK_FAILURE:
                /* so let's ignore it */
                break;
            }
        }

        if(!find_next(item))
        {
            revert();

            return;
        }
    }
}

bool Playback::State::try_descend()
{
    if(directory_depth_ >= max_directory_depth)
    {
        msg_info("Maximum directory depth of %u reached, not going any further down",
                 max_directory_depth);
        return false;
    }

    ID::List list_id =
        ViewFileBrowser::get_child_item_id(*dbus_list_object_,
                                           current_list_id_, navigation_, true);

    if(list_id.is_valid() &&
       ViewFileBrowser::enter_list_at(*dbus_list_object_, current_list_id_,
                                      item_flags_, navigation_, list_id, 0))
    {
        ++number_of_directories_entered_;
        return true;
    }

    return false;
}

bool Playback::State::find_next(const List::TextItem *directory)
{
    if(!mode_.is_playing())
        return false;

    if(mode_.get() == Playback::Mode::SINGLE_TRACK && number_of_streams_played_ > 0)
    {
        /* nothing more to do, finish playback gracefully */
        mode_.finish();
        return false;
    }

    auto item = dynamic_cast<const ViewFileBrowser::FileItem *>(directory);

    if(item != nullptr && item->is_directory() && try_descend())
        return true;

    while(true)
    {
        if(current_list_id_ == start_list_id_)
        {
            /* we are inside the directory from where we started */
            if(!navigation_.down())
                navigation_.set_cursor_by_line_number(0);

            const int line = navigation_.get_line_number_by_cursor();

            if(line < 0)
                return false;
            else if(static_cast<unsigned int>(navigation_.get_line_number_by_cursor()) == start_list_line_)
            {
                /* wrapped around, we are done */
                mode_.finish();
                return false;
            }
            else
                return true;
        }

        /* we are inside some nested directory (which we started traversing at
         * item 0)  */
        if(navigation_.down())
            return true;

        /* end of directory reached, go up again */
        unsigned int item_id;
        const ID::List list_id =
            ViewFileBrowser::get_parent_link_id(*dbus_list_object_,
                                                current_list_id_, item_id);

        if(!list_id.is_valid())
        {
            BUG("Invalid parent list ID during directory traversal.");
            break;
        }

        if(!ViewFileBrowser::enter_list_at(*dbus_list_object_, current_list_id_,
                                           item_flags_, navigation_,
                                           list_id, item_id))
        {
            BUG("Failed moving up to parent list during directory traversal.");
            break;
        }
    }

    return false;
}

void Playback::State::revert()
{
    if(user_list_id_ == ID::List())
        return;

    if(mode_.get() != Mode::FINISHED)
        msg_error(0, LOG_NOTICE, "Stopped directory traversal due to failure.");

    msg_info("Entered %u directories, played %u streams, failed playing %u streams",
             number_of_directories_entered_,
             number_of_streams_played_, number_of_streams_skipped_);

    dbus_list_object_->enter_list(user_list_id_, user_list_line_);

    user_list_id_ = ID::List();
    start_list_id_ = ID::List();
    current_list_id_ = ID::List();
    mode_.deactivate();
}
