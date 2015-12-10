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

#include "player.hh"
#include "playbackmode_state.hh"
#include "streamplayer_dbus.h"
#include "dbus_iface_deep.h"

static inline void no_context_bug(const char *what)
{
    BUG("Got %s, but have no context", what);
}

bool Playback::Player::take(Playback::State &playback_state,
                            const List::DBusList &file_list, int line)
{
    if(current_state_ != nullptr)
        release();

    current_state_ = &playback_state;

    set_assumed_stream_state(PlayInfo::Data::STREAM_STOPPED);

    if(!current_state_->start(file_list, line))
    {
        set_assumed_stream_state(PlayInfo::Data::STREAM_UNAVAILABLE);
        return false;
    }

    current_state_->enqueue_next(stream_info_, true);

    return true;
}

void Playback::Player::clear()
{
    stream_info_.clear();
    track_info_.meta_data_.clear(true);
    incoming_meta_data_.clear(true);

    if(current_state_ != nullptr)
        current_state_->revert();
}

void Playback::Player::release()
{
    if(current_state_ == nullptr)
        no_context_bug("release command");

    if(!tdbus_splay_playback_call_stop_sync(dbus_get_streamplayer_playback_iface(),
                                            NULL, NULL))
        msg_error(0, LOG_NOTICE, "Failed sending stop playback message");

    clear();

    current_state_ = nullptr;
    set_assumed_stream_state(PlayInfo::Data::STREAM_UNAVAILABLE);
}

void Playback::Player::start_notification()
{
    set_assumed_stream_state(PlayInfo::Data::STREAM_PLAYING);

    if(current_state_ == nullptr)
        return no_context_bug("start notification from player");

    current_state_->enqueue_next(stream_info_, false);
}

void Playback::Player::stop_notification()
{
    set_assumed_stream_state(PlayInfo::Data::STREAM_STOPPED);

    if(current_state_ == nullptr)
        no_context_bug("stop notification from player");

    clear();
}

void Playback::Player::pause_notification()
{
    set_assumed_stream_state(PlayInfo::Data::STREAM_PAUSED);

    if(current_state_ == nullptr)
        no_context_bug("pause notification from player");
}

bool Playback::Player::track_times_notification(const std::chrono::milliseconds &position,
                                                const std::chrono::milliseconds &duration)
{
    if(current_state_ == nullptr)
    {
        no_context_bug("new times from player");
        return true;
    }

    if(track_info_.stream_position_ == position && track_info_.stream_duration_ == duration)
        return false;

    track_info_.stream_position_ = position;
    track_info_.stream_duration_ = duration;

    return true;
}

const PlayInfo::MetaData *const Playback::Player::get_track_meta_data() const
{
    if(current_state_ != nullptr)
        return &track_info_.meta_data_;

    no_context_bug("meta data query");

    return nullptr;
}

PlayInfo::Data::StreamState Playback::Player::get_assumed_stream_state() const
{
    return track_info_.assumed_stream_state_;
}

std::pair<std::chrono::milliseconds, std::chrono::milliseconds> Playback::Player::get_times() const
{
    if(current_state_ != nullptr)
        return std::pair<std::chrono::milliseconds, std::chrono::milliseconds>(
            track_info_.stream_position_, track_info_.stream_duration_);
    else
        return std::pair<std::chrono::milliseconds, std::chrono::milliseconds>(
            std::chrono::milliseconds(-1), std::chrono::milliseconds(-1));
}

const std::string *Playback::Player::get_original_stream_name(uint16_t id)
{
    return stream_info_.lookup_and_activate(id);
}

void Playback::Player::skip_to_next()
{
    if(current_state_ == nullptr)
        return no_context_bug("skip to next track command");

    if(!tdbus_splay_urlfifo_call_next_sync(dbus_get_streamplayer_urlfifo_iface(),
                                           NULL, NULL))
        msg_error(0, LOG_NOTICE, "Failed sending skip track message");
}

void Playback::Player::meta_data_add_begin(bool is_update)
{
    if(current_state_ == nullptr)
        return no_context_bug("start adding meta data command");

    incoming_meta_data_.clear(is_update);
}

void Playback::Player::meta_data_add(const char *key, const char *value)
{
    if(current_state_ == nullptr)
        return no_context_bug("add meta data command");

    incoming_meta_data_.add(key, value, meta_data_reformatters_);
}

bool Playback::Player::meta_data_add_end()
{
    if(current_state_ == nullptr)
    {
        no_context_bug("add meta data command");
        return false;
    }

    if(incoming_meta_data_ == track_info_.meta_data_)
    {
        incoming_meta_data_.clear(true);
        return false;
    }
    else
    {
        track_info_.meta_data_ = incoming_meta_data_;
        incoming_meta_data_.clear(true);
        return true;
    }
}

void Playback::Player::set_assumed_stream_state(PlayInfo::Data::StreamState state)
{
    track_info_.assumed_stream_state_ = state;
}
