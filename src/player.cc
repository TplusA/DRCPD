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
#include "busy.hh"

static inline void expected_active_mode_bug(const char *what)
{
    BUG("Expected active mode: %s", what);
}

void Playback::Player::start()
{
    stream_enqueuer_ = std::thread(&Playback::Player::worker_main, this);
}

void Playback::Player::shutdown()
{
    {
        auto lock(message_queue_.lock());

        requests_.shutdown_request_ = true;
        message_queue_.wake_up();
    }

    stream_enqueuer_.join();
}

bool Playback::Player::try_take(State &playback_state,
                                const List::DBusList &file_list, int line,
                                IsBufferingCallback buffering_callback)
{
    auto current_state_ref(controller_.update_and_ref(&playback_state));
    LoggedLock::UniqueLock lock_csd(current_stream_data_.lock_);

    current_stream_data_.stream_info_.clear();

    lock_csd.unlock();
    Busy::set(Busy::Source::WAITING_FOR_PLAYER);
    buffering_callback(true);
    lock_csd.lock();

    if(playback_state.start(file_list, line))
    {
        send_message(std::move(std::bind(&Player::do_take, this,
                                         std::placeholders::_1,
                                         &playback_state,
                                         std::ref(file_list),
                                         line, buffering_callback)));
        return true;
    }

    lock_csd.unlock();
    Busy::clear(Busy::Source::WAITING_FOR_PLAYER);
    buffering_callback(false);

    return false;
}

void Playback::Player::take(Playback::State &playback_state,
                            const List::DBusList &file_list, int line,
                            IsBufferingCallback buffering_callback,
                            ReleasedCallback released_callback)
{
    release(false, is_different_active_mode(&playback_state));

    released_callback_ = released_callback;

    if(!try_take(playback_state, file_list, line, buffering_callback))
        release(true);
}

void Playback::Player::do_take(LockWithStopRequest &lockstop,
                               const Playback::State *expected_playback_state,
                               const List::DBusList &file_list, int line,
                               IsBufferingCallback buffering_callback)
{
    log_assert(expected_playback_state != nullptr);

    Busy::clear(Busy::Source::WAITING_FOR_PLAYER);

    if(requests_.release_player_.is_requested())
        return;

    auto current_state_ref(controller_.ref());
    auto current_state(current_state_ref.get());

    if(current_state != expected_playback_state)
        return;

    lockstop.lock();

    current_stream_data_.track_info_.set_buffering();

    if(!current_state->enqueue_next(current_stream_data_.stream_info_,
                                    true, lockstop))
    {
        current_stream_data_.track_info_.set_stopped();
        lockstop.unlock();
        buffering_callback(false);
    }
}

void Playback::Player::release(bool active_stop_command,
                               bool stop_playbackmode_state_if_active)
{
    if(requests_.release_player_.request())
        return;

    message_queue_.drain(requests_.stop_enqueuing_,
                         requests_.shutdown_request_);

    auto lock_req(requests_.release_player_.lock());

    send_message(std::move(std::bind(&Player::do_release, this,
                                     std::placeholders::_1,
                                     active_stop_command,
                                     stop_playbackmode_state_if_active)));

    /* synchronize thread with release of the player so that the messages do
     * not access dangling pointers */
    requests_.release_player_.wait(lock_req);

    if(released_callback_ != nullptr)
    {
        released_callback_();
        released_callback_ = nullptr;
    }
}

void Playback::Player::do_release(LockWithStopRequest &lockstop,
                                  bool active_stop_command,
                                  bool stop_playbackmode_state_if_active)
{
    log_assert(requests_.release_player_.is_requested());

    if(is_active_mode())
    {
        if(stop_playbackmode_state_if_active)
        {
            auto current_state_ref(controller_.ref());
            auto current_state(current_state_ref.get());

            if(current_state != nullptr)
                current_state->stop();
        }

        controller_.update_and_ref(nullptr);
    }

    lockstop.lock();

    if(active_stop_command)
    {
        if(!tdbus_splay_playback_call_stop_sync(dbus_get_streamplayer_playback_iface(),
                                                NULL, NULL))
            msg_error(0, LOG_NOTICE, "Failed sending stop playback message");
    }

    requests_.release_player_.ack();
}

void Playback::Player::append_referenced_lists(std::vector<ID::List> &list_ids)
{
    auto current_state_ref(controller_.ref());
    auto current_state(current_state_ref.get());
    std::lock_guard<LoggedLock::Mutex> lock_csd(current_stream_data_.lock_);

    if(current_state != nullptr)
        current_state->append_referenced_lists(list_ids);

    current_stream_data_.stream_info_.append_referenced_lists(list_ids);
}

bool Playback::Player::start_notification(ID::Stream stream_id,
                                          bool try_enqueue)
{
    log_assert(stream_id.is_valid());

    auto current_state_ref(controller_.ref());
    std::lock_guard<LoggedLock::Mutex> lock_csd(current_stream_data_.lock_);

    const StreamInfoItem *stream_info_item = nullptr;
    bool is_new_stream = true;

    if(current_stream_data_.stream_info_.lookup(stream_id) == nullptr)
    {
        msg_info("Got start notification for unknown stream ID %u",
                 stream_id.get_raw_id());
        current_stream_data_.stream_id_ = ID::Stream::make_invalid();
    }
    else if(stream_id != current_stream_data_.stream_id_)
    {
        const auto maybe_our_stream(ID::OurStream::make_from_generic_id(current_stream_data_.stream_id_));

        if(maybe_our_stream.get().is_valid())
            current_stream_data_.stream_info_.forget(maybe_our_stream);

        current_stream_data_.stream_id_ = stream_id;
        stream_info_item =
            current_stream_data_.stream_info_.lookup(current_stream_data_.stream_id_);
    }
    else
        is_new_stream = false;

    /* this also clears the associated meta data */
    current_stream_data_.track_info_.set_playing(is_new_stream);

    bool retval = false;

    if(stream_info_item != NULL)
    {
        const PreloadedMetaData &preloaded(stream_info_item->preloaded_meta_data_);
        PlayInfo::MetaData &md(current_stream_data_.track_info_.meta_data_);

        if(preloaded.have_anything())
        {
            md.values_[PlayInfo::MetaData::ARTIST] = preloaded.artist_;
            md.values_[PlayInfo::MetaData::ALBUM]  = preloaded.album_;
            md.values_[PlayInfo::MetaData::TITLE]  = preloaded.title_;

            retval = true;
        }

        md.values_[PlayInfo::MetaData::INTERNAL_DRCPD_TITLE] = stream_info_item->alt_name_;
        md.values_[PlayInfo::MetaData::INTERNAL_DRCPD_URL]   = stream_info_item->url_;

        if(!tdbus_dcpd_playback_call_set_stream_info_sync(dbus_get_dcpd_playback_iface(),
                                                          current_stream_data_.stream_id_.get_raw_id(),
                                                          stream_info_item->alt_name_.c_str(),
                                                          stream_info_item->url_.c_str(),
                                                          NULL, NULL))
            msg_error(0, LOG_NOTICE, "Failed sending stream information to dcpd");
    }

    if(is_active_mode())
        send_message(std::move(std::bind(&Player::do_start_notification, this,
                                         std::placeholders::_1,
                                         stream_id, try_enqueue)));

    return retval;
}

void Playback::Player::do_start_notification(LockWithStopRequest &lockstop,
                                             ID::Stream stream_id,
                                             bool try_enqueue)
{
    if(requests_.release_player_.is_requested())
        return;

    auto current_state_ref(controller_.ref());
    auto current_state(current_state_ref.get());

    if(current_state == nullptr)
        return;

    lockstop.lock();

    bool enqueued_anything;
    auto our_stream_id =
        ID::OurStream::make_from_generic_id(current_stream_data_.stream_id_);
    const bool can_enqueue_next =
        (!current_state->set_skip_mode_forward(current_stream_data_.stream_info_,
                                               our_stream_id,
                                               lockstop, false,
                                               enqueued_anything) &&
         try_enqueue && !enqueued_anything);

    current_stream_data_.stream_id_ = our_stream_id.get();

    if(can_enqueue_next)
        current_state->enqueue_next(current_stream_data_.stream_info_,
                                    false, lockstop);
}

void Playback::Player::stop_notification()
{
    if(requests_.release_player_.is_requested())
        return;

    auto queue_lock(message_queue_.drain(requests_.stop_enqueuing_,
                                         requests_.shutdown_request_));

    auto current_state_ref(controller_.ref());
    auto current_state(current_state_ref.get());

    std::lock_guard<LoggedLock::Mutex> lock_csd(current_stream_data_.lock_);

    current_stream_data_.stream_id_ = ID::Stream::make_invalid();
    current_stream_data_.stream_info_.clear();
    current_stream_data_.track_info_.set_stopped();

    incoming_meta_data_.clear(false);

    if(current_state != nullptr)
        current_state->revert();
}

void Playback::Player::pause_notification()
{
    std::lock_guard<LoggedLock::Mutex> lock_csd(current_stream_data_.lock_);
    current_stream_data_.track_info_.set_paused();
}

bool Playback::Player::track_times_notification(const std::chrono::milliseconds &position,
                                                const std::chrono::milliseconds &duration)
{
    std::lock_guard<LoggedLock::Mutex> lock_csd(current_stream_data_.lock_);

    if(current_stream_data_.track_info_.stream_position_ == position &&
       current_stream_data_.track_info_.stream_duration_ == duration)
        return false;

    current_stream_data_.track_info_.stream_position_ = position;
    current_stream_data_.track_info_.stream_duration_ = duration;

    return true;
}

std::pair<const PlayInfo::MetaData *, LoggedLock::UniqueLock>
Playback::Player::get_track_meta_data__locked() const
{
    return std::make_pair(&current_stream_data_.track_info_.meta_data_,
                          std::move(LoggedLock::UniqueLock(const_cast<Player *>(this)->current_stream_data_.lock_)));
}

PlayInfo::Data::StreamState Playback::Player::get_assumed_stream_state__locked() const
{
    std::lock_guard<LoggedLock::Mutex> lock_csd(const_cast<Player *>(this)->current_stream_data_.lock_);
    return get_assumed_stream_state__unlocked();
}

PlayInfo::Data::StreamState Playback::Player::get_assumed_stream_state__unlocked() const
{
    return current_stream_data_.track_info_.get_assumed_state();
}

std::pair<std::chrono::milliseconds, std::chrono::milliseconds> Playback::Player::get_times__locked() const
{
    std::lock_guard<LoggedLock::Mutex> lock_csd(const_cast<Player *>(this)->current_stream_data_.lock_);
    return get_times__unlocked();
}

std::pair<std::chrono::milliseconds, std::chrono::milliseconds>
Playback::Player::get_times__unlocked() const
{
    return std::pair<std::chrono::milliseconds, std::chrono::milliseconds>(
               current_stream_data_.track_info_.stream_position_,
               current_stream_data_.track_info_.stream_duration_);
}

std::pair<const StreamInfoItem *, LoggedLock::UniqueLock>
Playback::Player::get_stream_info__locked(ID::Stream id) const
{
    auto ret =
        std::make_pair(static_cast<const StreamInfoItem *>(nullptr),
                       std::move(LoggedLock::UniqueLock(const_cast<Player *>(this)->current_stream_data_.lock_)));

    ret.first = current_stream_data_.stream_info_.lookup(id);

    if(ret.first == nullptr)
        ret.second.unlock();

    return ret;
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

    if(requests_.release_player_.is_requested())
        return;

    message_queue_.drain(requests_.stop_enqueuing_,
                         requests_.shutdown_request_);

    std::lock_guard<LoggedLock::Mutex> lock_csd(current_stream_data_.lock_);

    if(get_assumed_stream_state__unlocked() == PlayInfo::Data::STREAM_BUFFERING)
        return;

    if(!ID::OurStream::compatible_with(current_stream_data_.stream_id_))
    {
        BUG("Got skip back command for invalid stream ID %u",
            current_stream_data_.stream_id_.get_raw_id());
        return;
    }

    const bool allow_restart_stream(rewind_threshold > rewind_threshold.zero());

    if(allow_restart_stream &&
       current_stream_data_.track_info_.stream_position_ >= rewind_threshold)
    {
        restart_stream();
        return;
    }

    send_message(std::move(std::bind(&Player::do_skip_to_previous, this,
                                     std::placeholders::_1,
                                     allow_restart_stream)));
}

void Playback::Player::do_skip_to_previous(LockWithStopRequest &lockstop,
                                           bool allow_restart_stream)
{
    if(requests_.release_player_.is_requested())
        return;

    auto current_state_ref(controller_.ref());
    auto current_state(current_state_ref.get());

    if(current_state == nullptr)
        return;

    lockstop.lock();

    bool enqueued_anything;
    auto our_stream_id =
        ID::OurStream::make_from_generic_id(current_stream_data_.stream_id_);
    const bool can_restart_stream =
        (!current_state->set_skip_mode_reverse(current_stream_data_.stream_info_,
                                               our_stream_id,
                                               lockstop, true,
                                               enqueued_anything) &&
         allow_restart_stream && !enqueued_anything);

    current_stream_data_.stream_id_ = our_stream_id.get();

    if(can_restart_stream)
        restart_stream();
}

static bool do_skip_to_next__unlocked()
{
    guint next_id;
    gboolean is_playing;

    if(!tdbus_splay_urlfifo_call_next_sync(dbus_get_streamplayer_urlfifo_iface(),
                                           &next_id, &is_playing,
                                           NULL, NULL))
        msg_error(0, LOG_NOTICE, "Failed sending skip track message");
    else if(is_playing && next_id != UINT32_MAX)
        return true;

    return false;
}

bool Playback::Player::try_fast_skip()
{
    std::lock_guard<LoggedLock::Mutex> lock_csd(current_stream_data_.lock_);

    if(get_assumed_stream_state__unlocked() == PlayInfo::Data::STREAM_BUFFERING)
        return true;

    const auto maybe_our_stream(ID::OurStream::make_from_generic_id(current_stream_data_.stream_id_));

    if(!maybe_our_stream.get().is_valid())
    {
        BUG("Got skip forward command for invalid stream ID %u",
            current_stream_data_.stream_id_.get_raw_id());
        return true;
    }

    if(current_stream_data_.stream_info_.lookup_own(maybe_our_stream) != nullptr &&
       current_stream_data_.stream_info_.get_number_of_known_streams() < 2)
    {
        /* currently playing stream is ours and we have nothing queued, so we
         * may need to wait */
        return false;
    }

    do_skip_to_next__unlocked();

    return true;
}

void Playback::Player::skip_to_next()
{
    if(!is_active_mode())
        return expected_active_mode_bug(__PRETTY_FUNCTION__);

    if(requests_.release_player_.is_requested())
        return;

    if(try_fast_skip())
        return;

    if(enqueuing_in_progress_.load())
    {
        /* skip to next track as soon as it is queued */
        send_message(std::move(std::bind(&Player::do_skip_to_next, this,
                                         std::placeholders::_1)));
    }
}

void Playback::Player::do_skip_to_next(LockWithStopRequest &lockstop) const
{
    if(requests_.release_player_.is_requested())
        return;

    lockstop.lock();

    do_skip_to_next__unlocked();
}

void Playback::Player::meta_data_add_begin()
{
    incoming_meta_data_.clear(true);
}

void Playback::Player::meta_data_add(const char *key, const char *value)
{
    incoming_meta_data_.add(key, value, meta_data_reformatters_);
}

bool Playback::Player::meta_data_add_end__locked(PlayInfo::MetaData::CopyMode mode)
{
    std::lock_guard<LoggedLock::Mutex> lock_csd(current_stream_data_.lock_);
    return do_meta_data_add_end(mode);
}

bool Playback::Player::meta_data_add_end__unlocked(PlayInfo::MetaData::CopyMode mode)
{
    return do_meta_data_add_end(mode);
}

bool Playback::Player::do_meta_data_add_end(PlayInfo::MetaData::CopyMode mode)
{
    if(incoming_meta_data_ == current_stream_data_.track_info_.meta_data_)
    {
        incoming_meta_data_.clear(true);
        return false;
    }
    else
    {
        current_stream_data_.track_info_.meta_data_.copy_from(incoming_meta_data_,
                                                              mode);
        incoming_meta_data_.clear(true);
        return true;
    }
}

void Playback::Player::set_external_stream_meta_data(ID::Stream stream_id,
                                                     const std::string &artist,
                                                     const std::string &album,
                                                     const std::string &title,
                                                     const std::string &alttrack,
                                                     const std::string &url)
{
    if(!stream_id.is_valid())
    {
        BUG("Got invalid external stream ID %u (rejected)",
            stream_id.get_raw_id());
        return;
    }

    if(ID::OurStream::compatible_with(stream_id))
    {
        BUG("Got external stream ID %u which looks like our own (rejected)",
            stream_id.get_raw_id());
        return;
    }

    std::lock_guard<LoggedLock::Mutex> lock_csd(current_stream_data_.lock_);
    current_stream_data_.stream_info_.set_external_stream_meta_data(
        stream_id, PreloadedMetaData(artist, album, title), alttrack, url);
}

bool Playback::Player::is_active_mode(const Playback::State *new_state)
{
    auto current_state_ref(controller_.ref());
    auto current_state(current_state_ref.get());

    return current_state != new_state;
}

bool Playback::Player::is_different_active_mode(const Playback::State *new_state)
{
    auto current_state_ref(controller_.ref());
    auto current_state(current_state_ref.get());

    return current_state != nullptr && current_state != new_state;
}

bool Playback::Player::send_message(Message &&message)
{
    if(requests_.shutdown_request_)
        return false;

    auto lock(message_queue_.lock());
    message_queue_.messages_.emplace_back(message);
    message_queue_.wake_up();

    return true;
}

bool Playback::Player::get_next_message(MessageQueue &queue,
                                        std::atomic_bool &shutdown_request,
                                        Message &message)
{
    auto queue_lock(queue.lock());

    queue.wait(queue_lock, shutdown_request);

    if(shutdown_request)
        return false;

    message = queue.messages_.front();
    queue.messages_.pop_front();

    return true;
}

void Playback::Player::worker_main()
{
    while(1)
    {
        Message message;

        if(!get_next_message(message_queue_, requests_.shutdown_request_, message))
            break;

        {
            LockWithStopRequest lockstop(current_stream_data_, requests_, enqueuing_in_progress_);
            message(lockstop);
        }

        message_queue_.message_processed();
    }
}
