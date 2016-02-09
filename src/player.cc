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

#include "player.hh"
#include "playbackmode_state.hh"
#include "streamplayer_dbus.h"
#include "dbus_iface_deep.h"

static inline void expected_active_mode_bug(const char *what)
{
    BUG("Expected active mode: %s", what);
}

bool Playback::Player::take(Playback::State &playback_state,
                            const List::DBusList &file_list, int line,
                            std::function<void(bool)> buffering_callback)
{
    if(is_active_mode(&playback_state))
        release(true);

    current_state_ = &playback_state;
    stream_info_.clear();

    waiting_for_start_notification_ = true;
    buffering_callback(true);

    if(!current_state_->start(file_list, line))
    {
        waiting_for_start_notification_ = false;
        buffering_callback(false);
        return false;
    }

    waiting_for_start_notification_ =
        current_state_->enqueue_next(stream_info_, true);

    if(!waiting_for_start_notification_)
        buffering_callback(false);

    return true;
}

void Playback::Player::release(bool active_stop_command)
{
    waiting_for_start_notification_ = false;

    if(is_active_mode())
    {
        current_state_->stop();
        current_state_ = nullptr;
    }

    if(active_stop_command)
    {
        if(!tdbus_splay_playback_call_stop_sync(dbus_get_streamplayer_playback_iface(),
                                                NULL, NULL))
            msg_error(0, LOG_NOTICE, "Failed sending stop playback message");
    }
}

void Playback::Player::start_notification(ID::Stream stream_id,
                                          bool try_enqueue)
{
    log_assert(stream_id.is_valid());

    waiting_for_start_notification_ = false;

    auto maybe_our_stream(ID::OurStream::make_from_generic_id(stream_id));

    if(stream_info_.lookup(maybe_our_stream) == nullptr)
    {
        msg_info("Got start notification for unknown stream ID %u",
                 stream_id.get_raw_id());
        current_stream_id_ = ID::OurStream::make_invalid();
    }
    else if(maybe_our_stream.get() != current_stream_id_.get())
    {
        if(current_stream_id_.get().is_valid())
            stream_info_.forget(current_stream_id_);

        current_stream_id_ = maybe_our_stream;

        const StreamInfoItem *const info =
            stream_info_.lookup(current_stream_id_);

        if(info != NULL &&
           !tdbus_dcpd_playback_call_set_stream_info_sync(dbus_get_dcpd_playback_iface(),
                                                          current_stream_id_.get().get_raw_id(),
                                                          info->alt_name_.c_str(),
                                                          info->url_.c_str(),
                                                          NULL, NULL))
            msg_error(0, LOG_NOTICE, "Failed sending stream information to dcpd");
    }

    track_info_.set_playing();

    if(!is_active_mode())
        return;

    if(!current_state_->set_skip_mode_forward(stream_info_,
                                              current_stream_id_, false,
                                              waiting_for_start_notification_) &&
       try_enqueue &&
       !waiting_for_start_notification_)
    {
        waiting_for_start_notification_ =
            current_state_->enqueue_next(stream_info_, false);
    }
}

void Playback::Player::stop_notification()
{
    current_stream_id_ = ID::OurStream::make_invalid();
    stream_info_.clear();
    incoming_meta_data_.clear(false);
    track_info_.set_stopped();

    if(is_active_mode())
    {
        waiting_for_start_notification_ = false;
        current_state_->revert();
    }
}

void Playback::Player::pause_notification()
{
    set_assumed_stream_state(PlayInfo::Data::STREAM_PAUSED);
}

bool Playback::Player::track_times_notification(const std::chrono::milliseconds &position,
                                                const std::chrono::milliseconds &duration)
{
    if(track_info_.stream_position_ == position && track_info_.stream_duration_ == duration)
        return false;

    track_info_.stream_position_ = position;
    track_info_.stream_duration_ = duration;

    return true;
}

const PlayInfo::MetaData &Playback::Player::get_track_meta_data() const
{
    return track_info_.meta_data_;
}

PlayInfo::Data::StreamState Playback::Player::get_assumed_stream_state() const
{
    return track_info_.assumed_stream_state_;
}

std::pair<std::chrono::milliseconds, std::chrono::milliseconds> Playback::Player::get_times() const
{
    return std::pair<std::chrono::milliseconds, std::chrono::milliseconds>(
               track_info_.stream_position_, track_info_.stream_duration_);
}

const std::string *Playback::Player::get_original_stream_name(ID::Stream id) const
{
    const auto our_id(ID::OurStream::make_from_generic_id(id));

    if(our_id.get().is_valid())
        return get_original_stream_name(our_id);
    else
        return nullptr;
}

const std::string *Playback::Player::get_original_stream_name(ID::OurStream id) const
{
    const auto *const info = stream_info_.lookup(id);
    return (info != nullptr) ? &info->alt_name_ : nullptr;
}

static bool restart_stream()
{
    if(tdbus_splay_playback_call_seek_sync(dbus_get_streamplayer_playback_iface(),
                                           0, "ms", NULL, NULL))
        return true;

    msg_error(0, LOG_NOTICE, "Failed restarting stream");

    return false;
}

void Playback::Player::skip_to_previous(std::chrono::milliseconds rewind_threshold)
{
    log_assert(rewind_threshold >= rewind_threshold.zero());

    if(!is_active_mode())
        return expected_active_mode_bug(__PRETTY_FUNCTION__);

    if(waiting_for_start_notification_)
        return;

    if(!current_stream_id_.get().is_valid())
    {
        BUG("Got skip back command for invalid stream ID");
        return;
    }

    if(rewind_threshold > rewind_threshold.zero() &&
       track_info_.stream_position_ >= rewind_threshold)
    {
        waiting_for_start_notification_ = restart_stream();
        return;
    }

    if(!current_state_->set_skip_mode_reverse(stream_info_,
                                              current_stream_id_, true,
                                              waiting_for_start_notification_) &&
       rewind_threshold > rewind_threshold.zero() &&
       !waiting_for_start_notification_)
    {
        waiting_for_start_notification_ = restart_stream();
    }
}

void Playback::Player::skip_to_next()
{
    if(!is_active_mode())
        return expected_active_mode_bug(__PRETTY_FUNCTION__);

    if(waiting_for_start_notification_)
        return;

    if(!current_stream_id_.get().is_valid())
    {
        BUG("Got skip forward command for invalid stream ID");
        return;
    }

    guint next_id;
    gboolean is_playing;

    if(!tdbus_splay_urlfifo_call_next_sync(dbus_get_streamplayer_urlfifo_iface(),
                                           &next_id, &is_playing,
                                           NULL, NULL))
        msg_error(0, LOG_NOTICE, "Failed sending skip track message");
    else if(is_playing && next_id != UINT32_MAX)
        waiting_for_start_notification_ = true;
}

void Playback::Player::meta_data_add_begin(bool is_update)
{
    incoming_meta_data_.clear(is_update);
}

void Playback::Player::meta_data_add(const char *key, const char *value)
{
    incoming_meta_data_.add(key, value, meta_data_reformatters_);
}

bool Playback::Player::meta_data_add_end()
{
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

bool Playback::Player::is_active_mode(const Playback::State *new_state) const
{
    return current_state_ != new_state;
}

void Playback::Player::set_assumed_stream_state(PlayInfo::Data::StreamState state)
{
    track_info_.assumed_stream_state_ = state;
}
