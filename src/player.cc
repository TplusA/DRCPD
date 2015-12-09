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

bool Playback::Player::take(Playback::State &playback_state,
                            const List::DBusList &file_list, int line)
{
    if(current_state_ != nullptr)
        release();

    current_state_ = &playback_state;

    if(!current_state_->start(file_list, line))
        return false;

    current_state_->enqueue_next(stream_info_, true);

    return true;
}

void Playback::Player::release()
{
    stop_notification();
    current_state_ = nullptr;
}

static inline void no_context_bug(const char *what)
{
    BUG("Got %s notification from player, but have no context", what);
}

void Playback::Player::start_notification()
{
    set_assumed_stream_state(PlayInfo::Data::STREAM_PLAYING);

    if(current_state_ == nullptr)
        no_context_bug("start");
}

void Playback::Player::stop_notification()
{
    set_assumed_stream_state(PlayInfo::Data::STREAM_STOPPED);

    if(current_state_ == nullptr)
    {
        no_context_bug("stop");
        return;
    }

    stream_info_.clear();
    track_info_.meta_data_.clear(true);
    incoming_meta_data_.clear(true);
    current_state_->revert();
}

void Playback::Player::pause_notification()
{
    set_assumed_stream_state(PlayInfo::Data::STREAM_PAUSED);

    if(current_state_ == nullptr)
        no_context_bug("pause");
}

bool Playback::Player::track_times_notification(const std::chrono::milliseconds &position,
                                                const std::chrono::milliseconds &duration)
{
    if(current_state_ == nullptr)
    {
        no_context_bug("position");
        return true;
    }

    if(track_info_.stream_position_ == position && track_info_.stream_duration_ == duration)
        return false;

    track_info_.stream_position_ = position;
    track_info_.stream_duration_ = duration;

    return true;
}

void Playback::Player::enqueue_next()
{
    if(current_state_ != nullptr)
        current_state_->enqueue_next(stream_info_, false);
}

const PlayInfo::MetaData *const Playback::Player::get_track_meta_data() const
{
    if(current_state_ != nullptr)
        return &track_info_.meta_data_;
    else
        return nullptr;
}

PlayInfo::Data::StreamState Playback::Player::get_assumed_stream_state() const
{
    if(current_state_ != nullptr)
        return track_info_.assumed_stream_state_;
    else
        return PlayInfo::Data::STREAM_STOPPED;
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

void Playback::Player::meta_data_add_begin(bool is_update)
{
    if(current_state_ != nullptr)
        incoming_meta_data_.clear(is_update);
}

void Playback::Player::meta_data_add(const char *key, const char *value)
{
    if(current_state_ != nullptr)
        incoming_meta_data_.add(key, value, meta_data_reformatters_);
}

bool Playback::Player::meta_data_add_end()
{
    if(current_state_ == nullptr)
        return false;

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
