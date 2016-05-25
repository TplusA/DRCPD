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

#include <cstring>
#include <future>

#include "playbackmode_state.hh"
#include "streaminfo.hh"
#include "view_filebrowser.hh"
#include "view_filebrowser_utils.hh"
#include "dbus_iface_deep.h"
#include "dbus_async.hh"
#include "de_tahifi_lists_errors.hh"

enum class SendStatus
{
    OK,
    INVALID_STREAM_ID,
    NO_URI,
    BROKER_FAILURE,
    FIFO_FAILURE,
    FIFO_FULL,
    PLAYBACK_FAILURE,
};

static const std::string empty_string;

static gchar *select_uri_from_list(gchar **uri_list)
{
    for(gchar **ptr = uri_list; *ptr != NULL; ++ptr)
        msg_info("URI: \"%s\"", *ptr);

    for(gchar **ptr = uri_list; *ptr != NULL; ++ptr)
    {
        const size_t len = strlen(*ptr);

        if(len < 4)
            continue;

        const gchar *const suffix = &(*ptr)[len - 4];

        if(strncasecmp(".m3u", suffix, 4) == 0 ||
           strncasecmp(".pls", suffix, 4) == 0)
            continue;

        return *ptr;
    }

    return NULL;
}

static std::string copy_selected_uri(gchar **const uri_list,
                                     const ListError error,
                                     ID::List list_id, unsigned int item_id,
                                     SendStatus &send_status)
{
    if(error != ListError::Code::OK)
    {
        msg_error(0, LOG_NOTICE,
                  "Got error %s instead of URI for item %u in list %u",
                  error.to_string(), item_id, list_id.get_raw_id());
        send_status = SendStatus::BROKER_FAILURE;
        return empty_string;
    }

    if(uri_list == NULL || uri_list[0] == NULL)
    {
        msg_info("No URI for item %u in list %u", item_id, list_id.get_raw_id());
        send_status = SendStatus::NO_URI;
        return empty_string;
    }

    gchar *const selected_uri = select_uri_from_list(uri_list);

    if(selected_uri == NULL || selected_uri[0] == '\0')
    {
        msg_info("No suitable URI found for item %u in list %u",
                 item_id, list_id.get_raw_id());
        send_status = SendStatus::NO_URI;
        return empty_string;
    }

    send_status = SendStatus::OK;

    return selected_uri;
}

/*!
 * Retrieve URI associated with selected item.
 *
 * \param list_id, item_id
 *     Which list item's URI to retrieve for sending to the stream player.
 *
 * \param proxy
 *     D-Bus proxy to the list broker the list ID and item ID belong to.
 *
 * \param abort_enqueue
 *     Periodically checked so that slow operations can be cancelled.
 *
 * \param send_status
 *     Error code.
 *
 * \returns
 *     An URI, or an empty string in case of failure.
 *
 * \bug This function only returns a single URI. See tickets #13 and #285.
 */
static std::string get_selected_uri(ID::List list_id, unsigned int item_id,
                                    tdbuslistsNavigation *proxy,
                                    Playback::AbortEnqueueIface &abort_enqueue,
                                    SendStatus &send_status)
{
    using AsyncCallType = DBus::AsyncCall<tdbuslistsNavigation, std::tuple<guchar, gchar **>>;

    auto *async_call = new AsyncCallType(
        proxy,
        [] (GObject *source_object) { return TDBUS_LISTS_NAVIGATION(source_object); },
        [] (bool &was_successful, AsyncCallType::PromiseType &promise,
            tdbuslistsNavigation *p, GAsyncResult *async_result, GError *&error)
        {
            guchar error_code = 0;
            gchar **uri_list = NULL;

            was_successful =
                tdbus_lists_navigation_call_get_uris_finish(p, &error_code, &uri_list,
                                                            async_result, &error);

            if(!was_successful)
                msg_error(0, LOG_NOTICE,
                          "Async D-Bus method call failed: %s",
                          error != nullptr ? error->message : "*NULL*");

            /*
             * For correct synchronization, this call must be the last
             * statement in this function; in particular, it must not be done
             * before the assignment to \p was_successful.
             */
            promise.set_value(std::move(std::make_tuple(error_code, uri_list)));
        },
        [] (AsyncCallType::PromiseReturnType &values)
        {
            gchar **const strings = std::get<1>(values);

            if(strings == NULL)
                return;

            for(gchar **ptr = strings; *ptr != NULL; ++ptr)
                g_free(*ptr);

            g_free(strings);
        },
        [&abort_enqueue] () { return abort_enqueue.may_continue(); });

    if(async_call == nullptr)
    {
        msg_out_of_memory("asynchronous D-Bus call");
        return empty_string;
    }

    async_call->invoke(tdbus_lists_navigation_call_get_uris,
                       list_id.get_raw_id(), item_id);
    async_call->wait_for_result();

    if(AsyncCallType::cleanup_if_failed(async_call))
    {
        msg_info("Failed obtaining URI for item %u in list %u", item_id, list_id.get_raw_id());
        send_status = SendStatus::BROKER_FAILURE;

        return empty_string;
    }

    DBus::AsyncResult async_result;
    const auto &result(async_call->get_result(async_result));

    const ListError error(std::get<0>(result));
    gchar **const uri_list(std::get<1>(result));

    const std::string selected_uri =
        copy_selected_uri(uri_list, error, list_id, item_id, send_status);

    delete async_call;

    return selected_uri;
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
 * \param stream_id
 *     Internal ID of the stream for mapping it to extra information maintained
 *     by us.
 *
 * \param play_immediately
 *     If true, request immediate playback of the selected list entry.
 *     Otherwise, the entry is just pushed into the player's internal queue.
 *
 * \param[out] queued_url
 *     Which URL was chosen for this stream.
 *
 * \returns
 *     #SendStatus::OK in case of success, detailed error code otherwise.
 */
static SendStatus send_selected_file_uri_to_streamplayer(ID::OurStream stream_id,
                                                         bool play_immediately,
                                                         const std::string &queued_url)
{
    msg_info("Queuing URI: \"%s\"", queued_url.c_str());

    gboolean fifo_overflow;
    gboolean is_playing;
    SendStatus ret;

    if(!tdbus_splay_urlfifo_call_push_sync(dbus_get_streamplayer_urlfifo_iface(),
                                           stream_id.get().get_raw_id(),
                                           queued_url.c_str(),
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
            msg_error(0, LOG_INFO, "URL FIFO overflow");
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

    return ret;
}

static bool try_clear_url_fifo_for_set_skip_mode(const Playback::CurrentMode &mode,
                                                 StreamInfo &sinfo,
                                                 ID::OurStream &current_stream_id)
{
    log_assert(current_stream_id.get().is_valid());

    if(mode.get() != Playback::Mode::LINEAR)
        return false;

    guint current_raw_id;
    GVariant *queued_ids_array;
    GVariant *removed_ids_array;

    const gboolean ok =
        tdbus_splay_urlfifo_call_clear_sync(dbus_get_streamplayer_urlfifo_iface(), 0,
                                            &current_raw_id,
                                            &queued_ids_array, &removed_ids_array,
                                            NULL, NULL);

    if(!ok)
        msg_error(0, LOG_NOTICE, "Failed clearing player URL FIFO");
    else
    {
        /* current_raw_id is a 32 bit entity because UINT32_MAX is a special
         * value outside of the regular 16 bit stream ID range, indicating that
         * no stream is playing */
        const ID::Stream current_id((current_raw_id == UINT32_MAX)
                                    ? ID::Stream::make_invalid()
                                    : ID::Stream::make_from_raw_id(current_raw_id));

        for(size_t i = 0; i < g_variant_n_children(removed_ids_array); ++i)
        {
            GVariant *id_variant = g_variant_get_child_value(removed_ids_array, i);
            const auto stream_id(ID::Stream::make_from_raw_id(g_variant_get_uint16(id_variant)));

            if(stream_id != current_id)
                sinfo.forget(ID::OurStream::make_from_generic_id(stream_id));

            g_variant_unref(id_variant);
        }

        for(size_t i = 0; i < g_variant_n_children(queued_ids_array); ++i)
        {
            GVariant *id_variant = g_variant_get_child_value(queued_ids_array, i);

            const auto stream_id =
                ID::Stream::make_from_raw_id(g_variant_get_uint16(id_variant));
            const auto should_be_ours =
                ID::OurStream::make_from_generic_id(stream_id);

            if(sinfo.lookup_own(should_be_ours) == nullptr)
                BUG("Stream %u is still queued in stream player, "
                    "but does not belong to us", stream_id.get_raw_id());

            g_variant_unref(id_variant);
        }

        current_stream_id = ID::OurStream::make_from_generic_id(current_id);
    }

    g_variant_unref(queued_ids_array);
    g_variant_unref(removed_ids_array);

    return ok;
}

bool Playback::State::try_set_position(const StreamInfoItem &info)
{
    try
    {
        ViewFileBrowser::Utils::enter_list_at(dbus_list_, item_flags_,
                                              navigation_,
                                              info.list_id_, info.line_,
                                              false);
    }
    catch(const List::DBusListException &e)
    {
        msg_error(0, LOG_ERR,
                  "Failed enter reverse skipping mode: %s", e.what());
        return false;
    }

    return true;
}

bool Playback::State::set_skip_mode_reverse(StreamInfo &sinfo,
                                            ID::OurStream &current_stream_id,
                                            AbortEnqueueIface &abort_enqueue,
                                            bool skip_to_next,
                                            bool &enqueued_anything)
{
    enqueued_anything = false;

    if(!current_stream_id.get().is_valid())
        return false;

    {
        auto unlock(abort_enqueue.temporary_data_unlock());

        if(is_reverse_traversal_)
        {
            /* already in reverse mode */
            return true;
        }

        const auto *const info = sinfo.lookup_own(current_stream_id);

        if(info == nullptr)
        {
            /* don't know where we are, user skips too fast */
            return false;
        }

        if(info->list_id_ == user_list_id_ && info->line_ == 0)
        {
            /* already on first track */
            return false;
        }

        if(!try_set_position(*info))
        {
            /* something wrong with the list */
            return false;
        }
    }

    if(!abort_enqueue.may_continue())
        return false;

    if(!try_clear_url_fifo_for_set_skip_mode(mode_, sinfo, current_stream_id))
    {
        /* wrong playback mode or D-Bus error */
        return false;
    }

    is_reverse_traversal_ = true;
    is_list_processed_ = false;

    enqueued_anything = enqueue_next(sinfo, skip_to_next, abort_enqueue, true);

    return true;
}

bool Playback::State::set_skip_mode_forward(StreamInfo &sinfo,
                                            ID::OurStream &current_stream_id,
                                            AbortEnqueueIface &abort_enqueue,
                                            bool skip_to_next,
                                            bool &enqueued_anything)
{
    enqueued_anything = false;

    if(!current_stream_id.get().is_valid())
        return false;

    {
        auto unlock(abort_enqueue.temporary_data_unlock());

        if(!is_reverse_traversal_)
        {
            /* already in forward mode */
            return false;
        }

        const auto *const info = sinfo.lookup_own(current_stream_id);

        if(info == nullptr)
        {
            /* don't know where we are, user skips too fast */
            return false;
        }

        is_reverse_traversal_ = false;

        if(!try_set_position(*info))
        {
            /* something wrong with the list */
            return false;
        }
    }

    if(!abort_enqueue.may_continue())
        return false;

    if(!try_clear_url_fifo_for_set_skip_mode(mode_, sinfo, current_stream_id))
    {
        /* wrong playback mode or D-Bus error */
        return false;
    }

    is_list_processed_ = false;

    enqueued_anything = enqueue_next(sinfo, skip_to_next, abort_enqueue, true);

    return true;
}

bool Playback::State::try_start()
    throw(List::DBusListException)
{
    item_flags_.list_content_changed();
    navigation_.set_cursor_by_line_number(user_list_line_);

    auto item = dynamic_cast<const ViewFileBrowser::FileItem *>(dbus_list_.get_item(navigation_.get_cursor()));
    if(item == nullptr)
        return false;

    if(!item->get_kind().is_directory())
        return true;

    if(mode_.get() != Playback::Mode::LINEAR)
        return false;

    ViewFileBrowser::Utils::enter_list_at(dbus_list_, item_flags_, navigation_,
                                          user_list_id_, user_list_line_,
                                          is_reverse_traversal_);

    if(!try_descend())
        return true;

    user_list_id_ = dbus_list_.get_list_id();
    directory_depth_ = 1;

    return true;
}

bool Playback::State::start(const List::DBusList &user_list,
                            unsigned int start_line)
{
    if(mode_.get() == Playback::Mode::NONE)
        return false;

    user_list_id_ = user_list.get_list_id();
    user_list_line_ = start_line;

    is_list_processed_ = false;
    is_reverse_traversal_ = false;

    directory_depth_ = 1;
    is_any_stream_queued_ = false;

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

    return false;
}

static inline bool can_queue_item(const ViewFileBrowser::FileItem &item)
{
    switch(item.get_kind().get())
    {
      case ListItemKind::OPAQUE:
      case ListItemKind::REGULAR_FILE:
        return true;

      case ListItemKind::LOCKED:
      case ListItemKind::DIRECTORY:
      case ListItemKind::PLAYLIST_FILE:
      case ListItemKind::PLAYLIST_DIRECTORY:
      case ListItemKind::SERVER:
      case ListItemKind::STORAGE_DEVICE:
      case ListItemKind::SEARCH_FORM:
      case ListItemKind::LOGOUT_LINK:
        return false;
    }

    BUG("Unhandled item kind %u, still pushing it to streamplayer",
        item.get_kind().get_raw_code());

    return true;
}

bool Playback::State::enqueue_next(StreamInfo &sinfo, bool skip_to_next,
                                   AbortEnqueueIface &abort_enqueue,
                                   bool just_switched_direction)
{
    if(is_list_processed_ && !just_switched_direction)
        return false;

    AbortEnqueueIface::EnqueuingInProgressMarker in_progress(abort_enqueue);

    bool queued_for_immediate_playback = false;

    while(abort_enqueue.may_continue())
    {
        const ViewFileBrowser::FileItem *item;

        {
            auto unlock(abort_enqueue.temporary_data_unlock());

            if(just_switched_direction)
            {
                just_switched_direction = false;

                try
                {
                    if(!find_next(nullptr, abort_enqueue))
                        return false;
                }
                catch(const List::DBusListException &e)
                {
                    msg_info("Enqueue next failed (0): %s %d", e.what(), e.is_dbus_error());
                }
            }

            try
            {
                item = dynamic_cast<decltype(item)>(dbus_list_.get_item(navigation_.get_cursor()));
            }
            catch(const List::DBusListException &e)
            {
                msg_info("Enqueue next failed (1): %s %d", e.what(), e.is_dbus_error());
                revert();

                return queued_for_immediate_playback;
            }

            if(item == nullptr)
            {
                try
                {
                    if(find_next(nullptr, abort_enqueue))
                        continue;
                }
                catch(const List::DBusListException &e)
                {
                    /* revert and return below */
                    msg_info("Enqueue next failed (2): %s %d", e.what(), e.is_dbus_error());
                }

                revert();

                return queued_for_immediate_playback;
            }
        }

        if(abort_enqueue.may_continue() && can_queue_item(*item))
        {
            const unsigned int current_line = navigation_.get_cursor();
            SendStatus send_status;
            std::string queued_url;

            {
                auto unlock(abort_enqueue.temporary_data_unlock());

                queued_url =
                    get_selected_uri(dbus_list_.get_list_id(), current_line,
                                     dbus_list_.get_dbus_proxy(),
                                     abort_enqueue, send_status);
            }

            if(send_status == SendStatus::OK)
            {
                const ID::OurStream fallback_title_id =
                    sinfo.insert(item->get_preloaded_meta_data(), item->get_text(),
                                 dbus_list_.get_list_id(), current_line);
                StreamInfoItem *info_item = sinfo.lookup_for_update(fallback_title_id);

                info_item->url_.swap(queued_url);

                send_status =
                    (fallback_title_id.get().is_valid()
                     ? send_selected_file_uri_to_streamplayer(fallback_title_id,
                                                              skip_to_next,
                                                              info_item->url_)
                     : SendStatus::INVALID_STREAM_ID);

                if(send_status != SendStatus::OK &&
                   send_status != SendStatus::INVALID_STREAM_ID)
                    sinfo.forget(fallback_title_id);

                item = nullptr;
                info_item = nullptr;
            }

            switch(send_status)
            {
              case SendStatus::OK:
                /* stream URI is in FIFO now */
                is_any_stream_queued_ = true;

                if(skip_to_next)
                    queued_for_immediate_playback = true;

                /* next stream, if any, shouldn't be skipped to */
                skip_to_next = false;
                break;

              case SendStatus::INVALID_STREAM_ID:
                /* stream info container full, try again later */
                return queued_for_immediate_playback;

              case SendStatus::NO_URI:
                /* that's life... just ignore this entry */
                break;

              case SendStatus::BROKER_FAILURE:
              case SendStatus::FIFO_FAILURE:
                /* trying to put something into the FIFO failed hard */
                revert();

                return queued_for_immediate_playback;

              case SendStatus::FIFO_FULL:
                /* try again in a later invokation of this function */
                return queued_for_immediate_playback;

              case SendStatus::PLAYBACK_FAILURE:
                /* so let's ignore it */
                break;
            }
        }

        try
        {
            auto unlock(abort_enqueue.temporary_data_unlock());

            if(find_next(item, abort_enqueue))
                continue;
        }
        catch(const List::DBusListException &e)
        {
            /* revert and return below */
            msg_info("Enqueue next failed (3): %s %d", e.what(), e.is_dbus_error());
        }

        if(!is_list_processed_)
            BUG("List should have been processed at this point");

        return queued_for_immediate_playback;
    }

    return queued_for_immediate_playback;
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
        ViewFileBrowser::Utils::get_child_item_id(dbus_list_,
                                                  dbus_list_.get_list_id(),
                                                  navigation_, nullptr, true);

    if(!list_id.is_valid())
        return false;

    ViewFileBrowser::Utils::enter_list_at(dbus_list_,
                                          item_flags_, navigation_, list_id, 0,
                                          is_reverse_traversal_);

    return true;
}

bool Playback::State::find_next(const List::TextItem *directory,
                                AbortEnqueueIface &abort_enqueue)
    throw(List::DBusListException)
{
    if(!mode_.is_playing())
        return false;

    if(mode_.get() == Playback::Mode::SINGLE_TRACK && is_any_stream_queued_)
    {
        /* nothing more to do, finish playback gracefully */
        is_list_processed_ = true;
        return false;
    }

    auto item = dynamic_cast<const ViewFileBrowser::FileItem *>(directory);

    if(abort_enqueue.may_continue() &&
       item != nullptr && item->get_kind().is_directory() && try_descend())
        return true;

    while(abort_enqueue.may_continue())
    {
        bool retval;
        if(is_reverse_traversal_ ? find_next_reverse(retval) : find_next_forward(retval))
            return retval;

        /* end of directory reached, go up again */
        unsigned int item_id;
        const ID::List list_id =
            ViewFileBrowser::Utils::get_parent_link_id(dbus_list_,
                                                       dbus_list_.get_list_id(),
                                                       item_id);

        if(!list_id.is_valid())
        {
            BUG("Invalid parent list ID during directory traversal.");
            break;
        }

        ViewFileBrowser::Utils::enter_list_at(dbus_list_, item_flags_,
                                              navigation_, list_id, item_id,
                                              false);
    }

    return false;
}

bool Playback::State::find_next_forward(bool &found_candidate)
    throw(List::DBusListException)
{
    if(navigation_.down())
    {
        is_list_processed_ = false;
        found_candidate = true;
        return true;
    }

    if(dbus_list_.get_list_id() == user_list_id_)
    {
        /* tried to go beyond last entry in directory from where we started:
         * we are done here */
        is_list_processed_ = true;
        found_candidate = false;
        return true;
    }

    /* we are not done yet */
    return false;
}

bool Playback::State::find_next_reverse(bool &found_candidate)
    throw(List::DBusListException)
{
    if(navigation_.up())
    {
        is_list_processed_ = false;
        found_candidate = true;
        return true;
    }

    if(dbus_list_.get_list_id() == user_list_id_)
    {
        /* tried to go before first entry in directory from where we started:
         * we are done here */
        is_list_processed_ = true;
        found_candidate = false;
        return true;
    }

    /* we are not done yet */
    return false;
}

void Playback::State::revert()
{
    if(!user_list_id_.is_valid())
        return;

    if(!is_list_processed_ && mode_.is_playing())
        msg_error(0, LOG_NOTICE, "Stopped directory traversal due to failure.");

    msg_info("Finished sending URIs from list to streamplayer");

    try
    {
        dbus_list_.enter_list(user_list_id_, user_list_line_);
    }
    catch(const List::DBusListException &e)
    {
        msg_info("Failed resetting traversal list to start position: %s", e.what());
    }

    user_list_id_ = ID::List();
    mode_.deactivate();
}

bool Playback::State::list_invalidate(ID::List list_id, ID::List replacement_id)
{
    log_assert(list_id.is_valid());

    if(!user_list_id_.is_valid())
        return false;

    if(user_list_id_ == list_id)
    {
        if(replacement_id.is_valid())
            user_list_id_ = replacement_id;
        else
            return true;
    }

    if(user_list_id_ == list_id)
        return true;

    /* we could set #Playback::State::current_list_id_ here and continue, but
     * it seems risky to do so without any further testing (read: I didn't test
     * this case) */
    if(dbus_list_.get_list_id() == list_id)
        return true;

    return false;
}

void Playback::State::append_referenced_lists(std::vector<ID::List> &list_ids)
{
    if(user_list_id_.is_valid())
        list_ids.push_back(user_list_id_);

    const ID::List temp(dbus_list_.get_list_id());
    if(temp.is_valid())
        list_ids.push_back(temp);
}
