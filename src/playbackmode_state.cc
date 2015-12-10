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
#include "streaminfo.hh"
#include "view_filebrowser.hh"
#include "view_filebrowser_utils.hh"
#include "dbus_iface_deep.h"
#include "de_tahifi_lists_errors.hh"

enum class SendStatus
{
    OK,
    NO_URI,
    BROKER_FAILURE,
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

/*!
 * Try to fill up the streamplayer FIFO.
 *
 * The function fetches the URIs for the selected item from the list broker,
 * then sends the first URI which doesn't look like a playlist to the stream
 * player's queue.
 *
 * No exception thrown in here because the caller needs to react to specific
 * situations.
 *
 * \param list_id, item_id, proxy
 *     Which list item's URI to send to the stream player, and a D-Bus proxy to
 *     the list broker.
 *
 * \param stream_id
 *     Internal ID of the stream for mapping it to extra information maintained
 *     by us.
 *
 * \param play_immediately
 *     If true, request immediate playback of the selected list entry.
 *     Otherwise, the entry is just pushed into the player's internal queue.
 *
 * \returns
 *     #SendStatus::OK in case of success, detailed error code otherwise.
 */
static SendStatus send_selected_file_uri_to_streamplayer(ID::List list_id,
                                                         unsigned int item_id,
                                                         uint16_t stream_id,
                                                         tdbuslistsNavigation *proxy,
                                                         bool play_immediately)
{
    gchar **uri_list;
    guchar error_code;

    if(!tdbus_lists_navigation_call_get_uris_sync(proxy,
                                                  list_id.get_raw_id(), item_id,
                                                  &error_code,
                                                  &uri_list, NULL, NULL))
    {
        msg_info("Failed obtaining URI for item %u in list %u", item_id, list_id.get_raw_id());
        return SendStatus::BROKER_FAILURE;
    }

    const ListError error(error_code);

    if(error != ListError::Code::OK)
    {
        msg_error(0, LOG_NOTICE,
                  "Got error %s instead of URI for item %u in list %u",
                  error.to_string(), item_id, list_id.get_raw_id());
        free_array_of_strings(uri_list);
        return SendStatus::BROKER_FAILURE;
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
    gboolean is_playing;
    SendStatus ret;

    if(!tdbus_splay_urlfifo_call_push_sync(dbus_get_streamplayer_urlfifo_iface(),
                                           stream_id, selected_uri,
                                           0, "ms", 0, "ms",
                                           play_immediately ? -2 : -1,
                                           &fifo_overflow, &is_playing,
                                           NULL, NULL))
    {
        msg_error(0, LOG_NOTICE, "Failed queuing URI to streamplayer");
        ret = SendStatus::FIFO_FAILURE;
    }
    else
    {
        if(fifo_overflow)
        {
            msg_error(EAGAIN, LOG_INFO, "URL FIFO overflow");
            ret = SendStatus::FIFO_FULL;
        }
        else if(!is_playing &&
                !tdbus_splay_playback_call_start_sync(dbus_get_streamplayer_playback_iface(),
                                                      NULL, NULL))
        {
            msg_error(0, LOG_NOTICE, "Failed sending start playback message");
            ret = SendStatus::PLAYBACK_FAILURE;
        }
        else
            ret = SendStatus::OK;
    }

    free_array_of_strings(uri_list);

    return ret;
}

bool Playback::State::try_start()
    throw(List::DBusListException)
{
    start_list_id_ = user_list_id_;
    start_list_line_ = user_list_line_;

    item_flags_.list_content_changed();
    navigation_.set_cursor_by_line_number(start_list_line_);

    auto item = dynamic_cast<const ViewFileBrowser::FileItem *>(dbus_list_.get_item(navigation_.get_cursor()));
    if(item == nullptr)
        return false;

    if(item->is_directory())
    {
        if(mode_.get() != Playback::Mode::LINEAR)
            return false;

        ViewFileBrowser::enter_list_at(dbus_list_, item_flags_, navigation_,
                                       start_list_id_, start_list_line_);

        current_list_id_ = start_list_id_;

        if(!try_descend())
            return true;

        start_list_id_ = current_list_id_;
        start_list_line_ = 0;

        directory_depth_ = 1;
        number_of_directories_entered_ = 0;
    }
    else
        current_list_id_ = start_list_id_;

    return true;
}

bool Playback::State::start(const List::DBusList &user_list,
                            unsigned int start_line)
{
    if(mode_.get() == Playback::Mode::NONE)
        return false;

    user_list_id_ = user_list.get_list_id();
    user_list_line_ = start_line;

    directory_depth_ = 1;
    number_of_streams_played_ = 0;
    number_of_streams_skipped_ = 0;
    number_of_directories_entered_ = 0;

    try
    {
        dbus_list_.clone_state(user_list);

        if(try_start())
            return true;

        msg_error(0, LOG_NOTICE, "Failed start playing");
    }
    catch(const List::DBusListException &e)
    {
        msg_error(0, LOG_NOTICE,
                  "Failed start playing, got hard %s error: %s",
                  e.is_dbus_error() ? "D-Bus" : "list retrieval", e.what());
    }

    current_list_id_ = ID::List();

    return false;
}

void Playback::State::enqueue_next(StreamInfo &sinfo, bool skip_to_next)
{
    if(!mode_.is_playing())
        return;

    while(true)
    {
        const ViewFileBrowser::FileItem *item;

        try
        {
            item = dynamic_cast<decltype(item)>(dbus_list_.get_item(navigation_.get_cursor()));
        }
        catch(const List::DBusListException &e)
        {
            msg_info("Enqueue next failed (1): %s %d", e.what(), e.is_dbus_error());
            revert();

            return;
        }

        if(!item)
        {
            try
            {
                if(find_next(nullptr))
                    continue;
            }
            catch(const List::DBusListException &e)
            {
                /* revert and return below */
                msg_info("Enqueue next failed (2): %s %d", e.what(), e.is_dbus_error());
            }

            revert();

            return;
        }

        if(!item->is_directory())
        {
            const uint16_t fallback_title_id = sinfo.insert(item->get_text());

            item = nullptr;

            const auto send_status =
                send_selected_file_uri_to_streamplayer(current_list_id_,
                                                       navigation_.get_cursor(),
                                                       fallback_title_id,
                                                       dbus_list_.get_dbus_proxy(),
                                                       skip_to_next);

            if(send_status != SendStatus::OK)
                sinfo.forget(fallback_title_id);

            switch(send_status)
            {
              case SendStatus::OK:
                /* stream URI is in FIFO now */
                ++number_of_streams_played_;

                /* next stream, if any, shouldn't be skipped to */
                skip_to_next = false;
                break;

              case SendStatus::NO_URI:
                /* that's life... just ignore this entry */
                ++number_of_streams_skipped_;
                break;

              case SendStatus::BROKER_FAILURE:
              case SendStatus::FIFO_FAILURE:
                /* trying to put something into the FIFO failed hard */
                ++number_of_streams_skipped_;
                revert();

                return;

              case SendStatus::FIFO_FULL:
                /* try again in a later invokation of this function */
                return;

              case SendStatus::PLAYBACK_FAILURE:
                /* so let's ignore it */
                ++number_of_streams_skipped_;
                break;
            }
        }

        try
        {
            if(find_next(item))
                continue;
        }
        catch(const List::DBusListException &e)
        {
            /* revert and return below */
            msg_info("Enqueue next failed (3): %s %d", e.what(), e.is_dbus_error());
        }

        revert();

        return;
    }
}

bool Playback::State::try_descend()
    throw(List::DBusListException)
{
    if(directory_depth_ >= max_directory_depth)
    {
        msg_info("Maximum directory depth of %u reached, not going down any further",
                 max_directory_depth);
        return false;
    }

    ID::List list_id =
        ViewFileBrowser::get_child_item_id(dbus_list_,
                                           current_list_id_, navigation_, true);

    if(!list_id.is_valid())
        return false;

    ViewFileBrowser::enter_list_at(dbus_list_,
                                   item_flags_, navigation_, list_id, 0);

    current_list_id_ = list_id;
    ++number_of_directories_entered_;

    return true;
}

bool Playback::State::find_next(const List::TextItem *directory)
    throw(List::DBusListException)
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
            ViewFileBrowser::get_parent_link_id(dbus_list_,
                                                current_list_id_, item_id);

        if(!list_id.is_valid())
        {
            BUG("Invalid parent list ID during directory traversal.");
            break;
        }

        ViewFileBrowser::enter_list_at(dbus_list_, item_flags_, navigation_,
                                       list_id, item_id);
        current_list_id_ = list_id;
    }

    return false;
}

void Playback::State::revert()
{
    if(user_list_id_ == ID::List())
        return;

    if(mode_.get() != Mode::FINISHED)
        msg_error(0, LOG_NOTICE, "Stopped directory traversal due to failure.");

    msg_info("Finished sending URIs from list to streamplayer");
    msg_info("Entered %u directories, played %u streams, failed playing %u streams",
             number_of_directories_entered_,
             number_of_streams_played_, number_of_streams_skipped_);

    try
    {
        dbus_list_.enter_list(user_list_id_, user_list_line_);
    }
    catch(const List::DBusListException &e)
    {
        msg_info("Failed resetting traversal list to start position: %s", e.what());
    }

    user_list_id_ = ID::List();
    start_list_id_ = ID::List();
    current_list_id_ = ID::List();
    mode_.deactivate();
}
