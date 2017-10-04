/*
 * Copyright (C) 2016, 2017  T+A elektroakustik GmbH & Co. KG
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

#include "player_control.hh"
#include "player_stopped_reason.hh"
#include "directory_crawler.hh"
#include "audiosource.hh"
#include "dbus_iface_deep.h"
#include "dbus_common.h"
#include "view_play.hh"
#include "messages.h"

enum class StreamExpected
{
    OURS_AS_EXPECTED,
    OURS_QUEUED,
    EMPTY_AS_EXPECTED,
    NOT_OURS,
    UNEXPECTEDLY_NOT_OURS,
    UNEXPECTEDLY_OURS,
    OURS_WRONG_ID,
    INVALID_ID,
};

enum class ExpectedPlayingCheckMode
{
    STOPPED,
    STOPPED_WITH_ERROR,
    SKIPPED,
};

bool Player::Skipper::set_intention_for_skipping(Player::Data &data)
{
    switch(data.get_intention())
    {
      case Player::UserIntention::NOTHING:
      case Player::UserIntention::STOPPING:
        break;

      case Player::UserIntention::PAUSING:
        data.set_intention(Player::UserIntention::SKIPPING_PAUSED);
        return true;

      case Player::UserIntention::LISTENING:
        data.set_intention(Player::UserIntention::SKIPPING_LIVE);
        return true;

      case Player::UserIntention::SKIPPING_PAUSED:
      case Player::UserIntention::SKIPPING_LIVE:
        return true;
    }

    return false;
}

void Player::Skipper::set_intention_from_skipping(Player::Data &data)
{
    switch(data.get_intention())
    {
      case Player::UserIntention::NOTHING:
      case Player::UserIntention::STOPPING:
      case Player::UserIntention::PAUSING:
      case Player::UserIntention::LISTENING:
        break;

      case Player::UserIntention::SKIPPING_PAUSED:
        data.set_intention(Player::UserIntention::PAUSING);
        break;

      case Player::UserIntention::SKIPPING_LIVE:
        data.set_intention(Player::UserIntention::LISTENING);
        break;
    }
}

bool Player::Skipper::stop_skipping(Player::Data &data, Playlist::CrawlerIface &crawler)
{
    reset();
    set_intention_from_skipping(data);

    return crawler.set_direction_forward();
}

static inline bool should_reject_skip_request(const Player::Data &data)
{
    switch(data.get_current_stream_state())
    {
      case Player::StreamState::STOPPED:
        return true;

      case Player::StreamState::BUFFERING:
      case Player::StreamState::PLAYING:
      case Player::StreamState::PAUSED:
        break;
    }

    return false;
}

Player::Skipper::SkipState
Player::Skipper::forward_request(Player::Data &data, Playlist::CrawlerIface &crawler,
                                 Player::UserIntention &previous_intention)
{
    if(should_reject_skip_request(data))
        return REJECTED;

    previous_intention = data.get_intention();

    if(pending_skip_requests_ >= MAX_PENDING_SKIP_REQUESTS)
        return REJECTED;

    if(pending_skip_requests_ == 0)
    {
        if(is_skipping_)
        {
            ++pending_skip_requests_;
            return SKIPPING;
        }
        else
        {
            if(!set_intention_for_skipping(data))
                return REJECTED;

            is_skipping_ = true;
            crawler.set_direction_forward();

            return FIRST_SKIP_REQUEST;
        }
    }

    if(++pending_skip_requests_ == 0)
    {
        stop_skipping(data, crawler);
        return BACK_TO_NORMAL;
    }

    return SKIPPING;
}

Player::Skipper::SkipState
Player::Skipper::backward_request(Player::Data &data, Playlist::CrawlerIface &crawler,
                                  Player::UserIntention &previous_intention)
{
    if(should_reject_skip_request(data))
        return REJECTED;

    previous_intention = data.get_intention();

    if(pending_skip_requests_ <= -MAX_PENDING_SKIP_REQUESTS)
        return REJECTED;

    if(pending_skip_requests_ == 0)
    {
        if(is_skipping_)
        {
            --pending_skip_requests_;
            return SKIPPING;
        }
        else
        {
            if(!set_intention_for_skipping(data))
                return REJECTED;

            is_skipping_ = true;
            crawler.set_direction_backward();

            return FIRST_SKIP_REQUEST;
        }
    }

    if(--pending_skip_requests_ == 0)
    {
        stop_skipping(data, crawler);
        return BACK_TO_NORMAL;
    }

    return SKIPPING;
}

Player::Skipper::SkippedResult
Player::Skipper::skipped(Data &data, Playlist::CrawlerIface &crawler,
                         bool keep_skipping)
{
    if(pending_skip_requests_ == 0)
    {
        if(is_skipping_)
            crawler.mark_current_position();
        else
        {
            BUG("Got skipped notification, but not skipping");
            keep_skipping = false;
        }

        return (keep_skipping
                ? !crawler.is_crawling_forward()
                : stop_skipping(data, crawler))
            ? SkippedResult::DONE_BACKWARD
            : SkippedResult::DONE_FORWARD;
    }

    log_assert(is_skipping_);

    crawler.mark_current_position();

    if(pending_skip_requests_ > 0)
    {
        --pending_skip_requests_;
        crawler.set_direction_forward();
        return SkippedResult::SKIPPING_FORWARD;
    }
    else
    {
        ++pending_skip_requests_;
        crawler.set_direction_backward();
        return SkippedResult::SKIPPING_BACKWARD;
    }
}

static void source_request_done(GObject *source_object, GAsyncResult *res,
                                gpointer user_data)
{
    auto *audio_source = static_cast<Player::AudioSource *>(user_data);

    GError *error = nullptr;
    gchar *player_id = nullptr;
    gboolean switched = FALSE;

    tdbus_aupath_manager_call_request_source_finish(TDBUS_AUPATH_MANAGER(source_object),
                                                    &player_id, &switched,
                                                    res, &error);

    if(error != nullptr)
    {
        msg_error(0, LOG_EMERG, "Requesting audio source %s failed: %s",
                  audio_source->id_.c_str(), error->message);
        g_error_free(error);

        /* keep audio source state the way it is and leave state switching to
         * the \c de.tahifi.AudioPath.Source() methods */
    }

    if(player_id != nullptr)
    {
        msg_info("%s player %s", switched ? "Activated" : "Still using", player_id);
        g_free(player_id);
    }

    /*
     * Note that we do not notify the audio source about its selected state
     * here since we are doing this in our implementation of
     * \c de.tahifi.AudioPath.Source.Selected() already. It's a bit cleaner and
     * doesn't force us to operate from some bogus GLib context for
     * asynchronous result handling and to deal with locking.
     */
}

bool Player::Control::is_active_controller_for_audio_source(const std::string &audio_source_id) const
{
    return player_ != nullptr &&
           is_any_audio_source_plugged() &&
           audio_source_id == audio_source_->id_;
}

static void set_audio_player_dbus_proxies(const std::string &audio_player_id,
                                          Player::AudioSource &audio_source)
{
    if(audio_player_id == "strbo")
        audio_source.set_proxies(dbus_get_streamplayer_urlfifo_iface(),
                                 dbus_get_streamplayer_playback_iface());
    else if(audio_player_id == "roon")
        audio_source.set_proxies(nullptr,
                                 dbus_get_roonplayer_playback_iface());
    else
        audio_source.set_proxies(nullptr, nullptr);
}

void Player::Control::plug(AudioSource &audio_source,
                           const std::function<void(void)> &stop_playing_notification,
                           const std::string *external_player_id)
{
    log_assert(!is_any_audio_source_plugged());
    log_assert(stop_playing_notification_ == nullptr);
    log_assert(crawler_ == nullptr);
    log_assert(permissions_ == nullptr);
    log_assert(stop_playing_notification != nullptr);

    audio_source_ = &audio_source;
    stop_playing_notification_ = stop_playing_notification;

    switch(audio_source_->get_state())
    {
      case AudioSourceState::DESELECTED:
      case AudioSourceState::REQUESTED:
        msg_info("Requesting source %s", audio_source_->id_.c_str());
        audio_source_->request();
        tdbus_aupath_manager_call_request_source(dbus_audiopath_get_manager_iface(),
                                                 audio_source_->id_.c_str(), NULL,
                                                 source_request_done, audio_source_);
        break;

      case AudioSourceState::SELECTED:
        msg_info("Not requesting source %s, already selected",
                 audio_source_->id_.c_str());
        break;
    }

    if(external_player_id != nullptr)
        set_audio_player_dbus_proxies(*external_player_id, *audio_source_);
}

void Player::Control::plug(Player::Data &player_data)
{
    log_assert(player_ == nullptr);
    log_assert(crawler_ == nullptr);
    log_assert(permissions_ == nullptr);

    player_ = &player_data;
}

void Player::Control::plug(Playlist::CrawlerIface &crawler,
                           const LocalPermissionsIface &permissions)
{
    plug(permissions);

    crawler_ = &crawler;

    auto crawler_lock(crawler_->lock());
    crawler_->attached_to_player_notification();
}

void Player::Control::plug(const LocalPermissionsIface &permissions)
{
    crawler_ = nullptr;
    permissions_ = &permissions;
    skip_requests_.reset();
    queued_streams_.clear();
    prefetch_state_ = PrefetchState::NOT_PREFETCHING;
    retry_data_.reset();
}

void Player::Control::forget_queued_and_playing(bool also_forget_playing)
{
    if(player_ != nullptr)
    {
        for(const auto &id : queued_streams_)
            player_->forget_stream(id.get());

        if(also_forget_playing && retry_data_.get_stream_id().get().is_valid())
            player_->forget_stream(retry_data_.get_stream_id().get());
    }

    queued_streams_.clear();

    if(also_forget_playing)
        retry_data_.reset();
}

void Player::Control::unplug(bool is_complete_unplug)
{
    auto locks(lock());

    if(is_complete_unplug)
    {
        audio_source_ = nullptr;
        stop_playing_notification_ = nullptr;
    }

    forget_queued_and_playing(true);

    if(player_ != nullptr)
    {
        player_->detached_from_player_notification(is_complete_unplug);

        if(is_complete_unplug)
            player_ = nullptr;
    }

    if(crawler_ != nullptr)
    {
        auto crawler_lock(crawler_->lock());
        crawler_->detached_from_player_notification(is_complete_unplug);

        if(is_complete_unplug)
            crawler_ = nullptr;
    }

    if(is_complete_unplug)
        permissions_ = nullptr;
}

static bool send_simple_playback_command(
        const Player::AudioSource *asrc,
        gboolean (*const sync_call)(tdbussplayPlayback *, GCancellable *, GError **),
        const char *error_short, const char *error_long)
{
    if(asrc == nullptr)
        return true;

    auto *proxy = asrc->get_playback_proxy();

    if(proxy == nullptr)
        return true;

    GError *error = NULL;
    sync_call(proxy, NULL, &error);

    if(dbus_common_handle_error(&error, error_short) < 0)
    {
        msg_error(0, LOG_NOTICE, "%s", error_long);
        return false;
    }
    else
        return true;
}

static inline bool send_play_command(const Player::AudioSource *asrc)
{
    return
        send_simple_playback_command(asrc, tdbus_splay_playback_call_start_sync,
                                     "Start playback",
                                     "Failed sending start playback message");
}

static inline bool send_stop_command(const Player::AudioSource *asrc)
{
    log_assert(asrc != nullptr);

    return
        send_simple_playback_command(asrc, tdbus_splay_playback_call_stop_sync,
                                     "Stop playback",
                                     "Failed sending stop playback message");
}

static inline bool send_pause_command(const Player::AudioSource *asrc)
{
    return
        send_simple_playback_command(asrc, tdbus_splay_playback_call_pause_sync,
                                     "Pause playback",
                                     "Failed sending pause playback message");
}

static inline bool send_simple_skip_forward_command(const Player::AudioSource *asrc)
{
    return
        send_simple_playback_command(asrc, tdbus_splay_playback_call_skip_to_next_sync,
                                     "Skip to next stream",
                                     "Failed sending skip forward message");
}

static inline bool send_simple_skip_backward_command(const Player::AudioSource *asrc)
{
    return
        send_simple_playback_command(asrc, tdbus_splay_playback_call_skip_to_previous_sync,
                                     "Skip to previous stream",
                                     "Failed sending skip backward message");
}

static bool send_skip_to_next_command(ID::Stream &removed_stream_from_queue,
                                      Player::StreamState &play_status,
                                      const Player::AudioSource *asrc)
{
    if(asrc == nullptr)
        return true;

    auto *proxy = asrc->get_urlfifo_proxy();
    guint skipped_id = UINT32_MAX;
    guchar raw_play_status = UCHAR_MAX;

    if(proxy != nullptr)
    {
        guint next_id;
        GError *error = NULL;

        tdbus_splay_urlfifo_call_next_sync(proxy,
                                           &skipped_id, &next_id,
                                           &raw_play_status, NULL, &error);

        if(dbus_common_handle_error(&error, "Skip to next") < 0)
        {
            msg_error(0, LOG_NOTICE, "Failed sending skip track message");
            return false;
        }
    }

    removed_stream_from_queue = (skipped_id != UINT32_MAX
                                 ? ID::Stream::make_from_raw_id(skipped_id)
                                 : ID::Stream::make_invalid());

    switch(raw_play_status)
    {
      case 0:
        play_status = Player::StreamState::STOPPED;
        break;

      case 1:
        play_status = Player::StreamState::PLAYING;
        break;

      case 2:
        play_status = Player::StreamState::PAUSED;
        break;

      default:
        BUG("Received unknown play status %u from streamplayer",
            raw_play_status);
        play_status = Player::StreamState::STOPPED;
        break;
    }

    return true;
}

static void resume_paused_stream(Player::Data *player,
                                 const Player::AudioSource *asrc)
{
    if(player != nullptr &&
       player->get_current_stream_state() == Player::StreamState::PAUSED)
        send_play_command(asrc);
}

static void do_deselect_audio_source(Player::AudioSource &audio_source,
                                     bool send_stop)
{
    if(send_stop)
        send_stop_command(&audio_source);

    audio_source.deselected_notification();
}

void Player::Control::repeat_mode_toggle_request() const
{
    if(permissions_ != nullptr && !permissions_->can_toggle_repeat())
    {
        msg_error(EPERM, LOG_INFO, "Ignoring repeat mode toggle request");
        return;
    }

    if(crawler_ != nullptr)
    {
        msg_error(ENOSYS, LOG_NOTICE,
                  "Repeat mode not implemented yet (see ticket #250)");
        return;
    }

    if(audio_source_ == nullptr)
        return;

    switch(audio_source_->get_state())
    {
      case AudioSourceState::DESELECTED:
      case AudioSourceState::REQUESTED:
        return;

      case AudioSourceState::SELECTED:
        break;
    }

    auto *proxy = audio_source_->get_playback_proxy();

    if(proxy != nullptr)
        tdbus_splay_playback_call_set_repeat_mode(proxy, "toggle",
                                                  nullptr, nullptr, nullptr);
}

void Player::Control::shuffle_mode_toggle_request() const
{
    if(permissions_ != nullptr && !permissions_->can_toggle_shuffle())
    {
        msg_error(EPERM, LOG_INFO, "Ignoring shuffle mode toggle request");
        return;
    }

    if(crawler_ != nullptr)
    {
        msg_error(ENOSYS, LOG_NOTICE,
                  "Shuffle mode not implemented yet (see tickets #27, #40, #80)");
        return;
    }

    if(audio_source_ == nullptr)
        return;

    switch(audio_source_->get_state())
    {
      case AudioSourceState::DESELECTED:
      case AudioSourceState::REQUESTED:
        return;

      case AudioSourceState::SELECTED:
        break;
    }

    auto *proxy = audio_source_->get_playback_proxy();

    if(proxy != nullptr)
        tdbus_splay_playback_call_set_shuffle_mode(proxy, "toggle",
                                                   nullptr, nullptr, nullptr);
}

void Player::Control::play_request()
{
    if(permissions_ != nullptr && !permissions_->can_play())
    {
        msg_error(EPERM, LOG_INFO, "Ignoring play request");
        return;
    }

    if(!is_active_controller())
        return;

    const auto astate = audio_source_->get_state();

    switch(astate)
    {
      case AudioSourceState::DESELECTED:
        return;

      case AudioSourceState::REQUESTED:
      case AudioSourceState::SELECTED:
        player_->set_intention(UserIntention::LISTENING);

        if(astate == AudioSourceState::REQUESTED)
            return;

        break;
    }

    switch(player_->get_current_stream_state())
    {
      case Player::StreamState::BUFFERING:
      case Player::StreamState::PLAYING:
        break;

      case Player::StreamState::STOPPED:
        {
            auto crawler_lock(crawler_->lock());
            crawler_->set_direction_forward();
            crawler_->find_next(std::bind(&Player::Control::async_list_entry_for_playing,
                                          this,
                                          std::placeholders::_1,
                                          std::placeholders::_2));
        }

        break;

      case Player::StreamState::PAUSED:
        resume_paused_stream(player_, audio_source_);
        break;
    }
}

void Player::Control::jump_to_crawler_location()
{
    if(!is_active_controller())
        return;

    if(crawler_ == nullptr)
        return;

    auto crawler_lock(crawler_->lock());

    crawler_->set_direction_forward();
    crawler_->find_next(std::bind(&Player::Control::async_list_entry_for_playing,
                                  this,
                                  std::placeholders::_1,
                                  std::placeholders::_2));
}

void Player::Control::stop_request()
{
    if(!is_any_audio_source_plugged())
        return;

    const auto astate = audio_source_->get_state();

    switch(astate)
    {
      case AudioSourceState::DESELECTED:
        break;

      case AudioSourceState::REQUESTED:
      case AudioSourceState::SELECTED:
        if(is_active_controller())
            player_->set_intention(UserIntention::STOPPING);

        if(astate == AudioSourceState::SELECTED)
            send_stop_command(audio_source_);

        break;
    }
}

void Player::Control::pause_request()
{
    if(permissions_ != nullptr && !permissions_->can_pause())
    {
        msg_error(EPERM, LOG_INFO, "Ignoring pause request");
        return;
    }

    if(!is_any_audio_source_plugged())
        return;

    const auto astate = audio_source_->get_state();

    switch(astate)
    {
      case AudioSourceState::DESELECTED:
        break;

      case AudioSourceState::REQUESTED:
      case AudioSourceState::SELECTED:
        if(is_active_controller())
            player_->set_intention(UserIntention::PAUSING);

        if(astate == AudioSourceState::SELECTED)
            send_pause_command(audio_source_);

        break;
    }
}

static void enforce_intention(Player::UserIntention intention,
                              Player::StreamState known_stream_state,
                              const Player::AudioSource *audio_source_)
{
    if(audio_source_ == nullptr ||
       audio_source_->get_state() != Player::AudioSourceState::SELECTED)
    {
        BUG("Cannot enforce intention on %s audio source",
            audio_source_ == nullptr ? "null" : "deselected");
        return;
    }

    switch(intention)
    {
      case Player::UserIntention::NOTHING:
        break;

      case Player::UserIntention::STOPPING:
        switch(known_stream_state)
        {
          case Player::StreamState::STOPPED:
            break;

          case Player::StreamState::BUFFERING:
          case Player::StreamState::PLAYING:
          case Player::StreamState::PAUSED:
            send_stop_command(audio_source_);
            break;
        }

        break;

      case Player::UserIntention::PAUSING:
      case Player::UserIntention::SKIPPING_PAUSED:
        switch(known_stream_state)
        {
          case Player::StreamState::STOPPED:
          case Player::StreamState::BUFFERING:
          case Player::StreamState::PLAYING:
            send_pause_command(audio_source_);
            break;

          case Player::StreamState::PAUSED:
            break;
        }

        break;

      case Player::UserIntention::LISTENING:
      case Player::UserIntention::SKIPPING_LIVE:
        switch(known_stream_state)
        {
          case Player::StreamState::STOPPED:
          case Player::StreamState::PAUSED:
            send_play_command(audio_source_);
            break;

          case Player::StreamState::BUFFERING:
          case Player::StreamState::PLAYING:
            break;
        }

        break;
    }
}

bool Player::Control::source_selected_notification(const std::string &audio_source_id)
{
    auto locks(lock());

    if(!is_any_audio_source_plugged())
    {
        msg_info("Dropped selected notification for audio source %s",
                 audio_source_id.c_str());
        return false;
    }

    if(audio_source_id == audio_source_->id_)
    {
        audio_source_->selected_notification();
        audio_source_->set_proxies(dbus_get_streamplayer_urlfifo_iface(),
                                   dbus_get_streamplayer_playback_iface());

        if(player_ != nullptr)
        {
            switch(player_->get_intention())
            {
              case UserIntention::NOTHING:
              case UserIntention::SKIPPING_PAUSED:
              case UserIntention::SKIPPING_LIVE:
                break;

              case UserIntention::LISTENING:
                play_request();
                break;

              case UserIntention::STOPPING:
                stop_request();
                break;

              case UserIntention::PAUSING:
                pause_request();
                break;
            }
        }

        msg_info("Selected audio source %s", audio_source_id.c_str());
        return true;
    }
    else
    {
        do_deselect_audio_source(*audio_source_, true);
        msg_info("Deselected audio source %s because %s was selected",
                 audio_source_->id_.c_str(), audio_source_id.c_str());
        return false;
    }
}

bool Player::Control::source_deselected_notification(const std::string *audio_source_id)
{
    auto locks(lock());

    if(!is_any_audio_source_plugged() ||
       (audio_source_id != nullptr && *audio_source_id != audio_source_->id_))
        return false;

    do_deselect_audio_source(*audio_source_, audio_source_id != nullptr);
    msg_info("Deselected audio source %s as requested",
             audio_source_->id_.c_str());

    return true;
}

static StreamExpected is_stream_expected(const ID::OurStream our_stream_id,
                                         const ID::Stream &stream_id)
{
    if(!our_stream_id.get().is_valid())
    {
        if(!stream_id.is_valid())
            return StreamExpected::EMPTY_AS_EXPECTED;

        if(!ID::OurStream::compatible_with(stream_id))
            return StreamExpected::NOT_OURS;

        return StreamExpected::UNEXPECTEDLY_OURS;
    }

    if(!stream_id.is_valid())
        return StreamExpected::INVALID_ID;;

    if(!ID::OurStream::compatible_with(stream_id))
        return StreamExpected::UNEXPECTEDLY_NOT_OURS;

    if(our_stream_id.get() != stream_id)
        return StreamExpected::OURS_WRONG_ID;

    return StreamExpected::OURS_AS_EXPECTED;
}

static StreamExpected is_stream_expected_to_start(const std::deque<ID::OurStream> &queued,
                                                  const ID::Stream &stream_id)
{
    const auto our_stream_id =
        queued.empty() ? ID::OurStream::make_invalid() : queued.front();

    StreamExpected result = is_stream_expected(our_stream_id, stream_id);

    switch(result)
    {
      case StreamExpected::OURS_AS_EXPECTED:
        result = StreamExpected::OURS_QUEUED;
        break;

      case StreamExpected::EMPTY_AS_EXPECTED:
      case StreamExpected::NOT_OURS:
        break;

      case StreamExpected::UNEXPECTEDLY_NOT_OURS:
        msg_info("Stream in streamplayer queue (%u) is not ours (expected %u)",
                 stream_id.get_raw_id(), our_stream_id.get().get_raw_id());
        break;

      case StreamExpected::UNEXPECTEDLY_OURS:
        if(!queued.empty())
            BUG("Out of sync: playing our stream %u that we don't know about",
                stream_id.get_raw_id());

        break;

      case StreamExpected::OURS_WRONG_ID:
        BUG("Out of sync: next stream ID should be %u, but streamplayer says it's %u",
            our_stream_id.get().get_raw_id(), stream_id.get_raw_id());
        break;

      case StreamExpected::INVALID_ID:
        BUG("Out of sync: expected stream %u, but have invalid playing stream",
            our_stream_id.get().get_raw_id());
        break;

      case StreamExpected::OURS_QUEUED:
        BUG("Unexpected stream expectation for playing");
        break;
    }

    return result;
}

static StreamExpected is_stream_expected_playing(const ID::OurStream current_stream_id,
                                                 const std::deque<ID::OurStream> &queued,
                                                 const ID::Stream &stream_id,
                                                 ExpectedPlayingCheckMode mode)
{
    const char *mode_name = nullptr;

    switch(mode)
    {
      case ExpectedPlayingCheckMode::STOPPED:
        mode_name = "Stopped";
        break;

      case ExpectedPlayingCheckMode::STOPPED_WITH_ERROR:
        mode_name = "StoppedWithError";
        break;

      case ExpectedPlayingCheckMode::SKIPPED:
        mode_name = "Skipped";
        break;
    }

    StreamExpected result = is_stream_expected(current_stream_id, stream_id);

    switch(result)
    {
      case StreamExpected::OURS_AS_EXPECTED:
      case StreamExpected::NOT_OURS:
        break;

      case StreamExpected::EMPTY_AS_EXPECTED:
        BUG("%s notification for no reason", mode_name);
        break;

      case StreamExpected::UNEXPECTEDLY_NOT_OURS:
        msg_info("%s foreign stream %u, expected our stream %u",
                 mode_name, stream_id.get_raw_id(),
                 current_stream_id.get().get_raw_id());
        break;

      case StreamExpected::UNEXPECTEDLY_OURS:
      case StreamExpected::OURS_WRONG_ID:
        if(!queued.empty() && queued.front().get() == stream_id)
        {
            result = StreamExpected::OURS_QUEUED;
            break;
        }

        if(!queued.empty())
        {
            if(result == StreamExpected::UNEXPECTEDLY_OURS)
                BUG("Out of sync: %s our stream %u that we don't know about",
                    mode_name, stream_id.get_raw_id());
            else
                BUG("Out of sync: %s stream ID should be %u, but streamplayer says it's %u",
                    mode_name,
                    current_stream_id.get().get_raw_id(), stream_id.get_raw_id());
        }

        break;

      case StreamExpected::INVALID_ID:
        BUG("Out of sync: %s invalid stream, expected stream %u",
            mode_name, current_stream_id.get().get_raw_id());
        break;

      case StreamExpected::OURS_QUEUED:
        BUG("Unexpected stream expectation for %s", mode_name);
        break;
    }

    return result;
}

bool Player::Control::skip_forward_request()
{
    if(player_ == nullptr || crawler_ == nullptr)
    {
        send_simple_skip_forward_command(audio_source_);
        return false;
    }

    if(permissions_ != nullptr && !permissions_->can_skip_forward())
    {
        msg_error(EPERM, LOG_INFO, "Ignoring skip forward request");
        return false;
    }

    if(!is_any_audio_source_plugged())
        return false;

    switch(audio_source_->get_state())
    {
      case AudioSourceState::DESELECTED:
      case AudioSourceState::REQUESTED:
        return false;

      case AudioSourceState::SELECTED:
        break;
    }

    auto crawler_lock(crawler_->lock());

    bool should_find_next = false;
    bool retval = false;

    UserIntention previous_intention;
    switch(skip_requests_.forward_request(*player_, *crawler_, previous_intention))
    {
      case Skipper::REJECTED:
      case Skipper::BACK_TO_NORMAL:
      case Skipper::SKIPPING:
        break;

      case Skipper::FIRST_SKIP_REQUEST:
        if(!crawler_->resume_crawler())
            break;

        if(!queued_streams_.empty())
        {
            /* stream player should have something in queue */
            auto skipped_stream_id(ID::Stream::make_invalid());
            Player::StreamState streamplayer_status;

            if(!send_skip_to_next_command(skipped_stream_id,
                                          streamplayer_status, audio_source_))
            {
                player_->set_intention(previous_intention);
                break;
            }

            switch(is_stream_expected_playing(retry_data_.get_stream_id(),
                                              queued_streams_, skipped_stream_id,
                                              ExpectedPlayingCheckMode::SKIPPED))
            {
              case StreamExpected::OURS_AS_EXPECTED:
                break;

              case StreamExpected::OURS_QUEUED:
                player_->forget_stream(queued_streams_.front().get());
                queued_streams_.pop_front();
                break;

              case StreamExpected::OURS_WRONG_ID:
                if(queued_streams_.front().get() == skipped_stream_id)
                break;

              case StreamExpected::EMPTY_AS_EXPECTED:
                break;

              case StreamExpected::NOT_OURS:
              case StreamExpected::UNEXPECTEDLY_NOT_OURS:
              case StreamExpected::UNEXPECTEDLY_OURS:
              case StreamExpected::INVALID_ID:
                BUG("Unexpected expectation result while skipping (1): current %u, skipped %u",
                    retry_data_.get_stream_id().get().get_raw_id(),
                    skipped_stream_id.get_raw_id());
                break;
            }

            if(retry_data_.get_stream_id().get().is_valid())
            {
                player_->forget_stream(retry_data_.get_stream_id().get());
                retry_data_.reset();
            }

            skip_requests_.skipped(*player_, *crawler_, false);
            enforce_intention(player_->get_intention(), streamplayer_status,
                              audio_source_);

            retval = true;
        }
        else
            should_find_next = true;

        break;
    }

    if(should_find_next)
    {
        switch(crawler_->find_next(std::bind(&Player::Control::async_list_entry_to_skip,
                                             this,
                                             std::placeholders::_1,
                                             std::placeholders::_2)))
        {
          case Playlist::CrawlerIface::FindNextFnResult::SEARCHING:
          case Playlist::CrawlerIface::FindNextFnResult::STOPPED_AT_START_OF_LIST:
          case Playlist::CrawlerIface::FindNextFnResult::STOPPED_AT_END_OF_LIST:
            break;

          case Playlist::CrawlerIface::FindNextFnResult::FAILED:
            player_->set_intention(previous_intention);
            break;
        }
    }

    return retval;
}

void Player::Control::skip_backward_request()
{
    if(player_ == nullptr || crawler_ == nullptr)
    {
        send_simple_skip_backward_command(audio_source_);
        return;
    }

    if(permissions_ != nullptr && !permissions_->can_skip_backward())
    {
        msg_error(EPERM, LOG_INFO, "Ignoring skip backward request");
        return;
    }

    if(!is_any_audio_source_plugged())
        return;

    switch(audio_source_->get_state())
    {
      case AudioSourceState::DESELECTED:
      case AudioSourceState::REQUESTED:
        return;

      case AudioSourceState::SELECTED:
        break;
    }

    auto crawler_lock(crawler_->lock());

    UserIntention previous_intention;
    switch(skip_requests_.backward_request(*player_, *crawler_, previous_intention))
    {
      case Skipper::REJECTED:
      case Skipper::BACK_TO_NORMAL:
      case Skipper::SKIPPING:
        break;

      case Skipper::FIRST_SKIP_REQUEST:
        if(!crawler_->resume_crawler())
            break;

        switch(crawler_->find_next(std::bind(&Player::Control::async_list_entry_to_skip,
                                             this,
                                             std::placeholders::_1,
                                             std::placeholders::_2)))
        {
          case Playlist::CrawlerIface::FindNextFnResult::SEARCHING:
          case Playlist::CrawlerIface::FindNextFnResult::STOPPED_AT_START_OF_LIST:
          case Playlist::CrawlerIface::FindNextFnResult::STOPPED_AT_END_OF_LIST:
            break;

          case Playlist::CrawlerIface::FindNextFnResult::FAILED:
            player_->set_intention(previous_intention);
            skip_requests_.stop_skipping(*player_, *crawler_);
            break;
        }

        break;
    }
}

void Player::Control::rewind_request()
{
    if(!is_active_controller())
        return;

    switch(audio_source_->get_state())
    {
      case AudioSourceState::DESELECTED:
      case AudioSourceState::REQUESTED:
        return;

      case AudioSourceState::SELECTED:
        break;
    }

    if(permissions_ != nullptr)
    {
        if(!permissions_->can_fast_wind_backward())
        {
            if(permissions_->can_skip_backward())
                return skip_backward_request();

            msg_error(EPERM, LOG_INFO, "Ignoring rewind request");
            return;
        }
    }

    auto *proxy = audio_source_->get_playback_proxy();

    if(proxy == nullptr)
        return;

    GError *error = NULL;
    tdbus_splay_playback_call_seek_sync(proxy, 0, "ms", NULL, &error);

    if(dbus_common_handle_error(&error, "Seek in stream") < 0)
        msg_error(0, LOG_NOTICE, "Failed restarting stream");
}

static inline bool is_fast_winding_allowed(const Player::LocalPermissionsIface *permissions)
{
    if(permissions == nullptr)
        return true;

    return permissions->can_fast_wind_forward() ||
           permissions->can_fast_wind_backward();
}

static inline bool is_fast_winding_allowed(const Player::LocalPermissionsIface *permissions,
                                           bool is_forward)
{
    if(permissions == nullptr)
        return true;

    return (is_forward && permissions->can_fast_wind_forward()) ||
           (!is_forward && permissions->can_fast_wind_backward());
}

void Player::Control::fast_wind_set_speed_request(double speed_factor)
{
    if(speed_factor > 0.0 && !is_fast_winding_allowed(permissions_))
    {
        msg_error(EPERM, LOG_INFO,
                  "Ignoring fast wind set factor request");
        return;
    }

    tdbus_splay_playback_call_set_speed(dbus_get_streamplayer_playback_iface(),
                                        speed_factor, NULL, NULL, NULL);
}

void Player::Control::seek_stream_request(int64_t value, const std::string &units)
{
    if(!is_fast_winding_allowed(permissions_))
    {
        msg_error(EPERM, LOG_INFO, "Ignoring stream seek request");
        return;
    }

    if(value < 0)
    {
        msg_error(EINVAL, LOG_ERR, "Invalid seek position %" PRId64, value);
        return;
    }

    if(!is_any_audio_source_plugged())
        return;

    switch(audio_source_->get_state())
    {
      case AudioSourceState::DESELECTED:
      case AudioSourceState::REQUESTED:
        return;

      case AudioSourceState::SELECTED:
        break;
    }

    auto *proxy = audio_source_->get_playback_proxy();

    if(proxy != nullptr)
        tdbus_splay_playback_call_seek(proxy, value, units.c_str(),
                                       NULL, NULL, NULL);
}

void Player::Control::play_notification(ID::Stream stream_id,
                                        bool is_new_stream)
{
    if(is_new_stream)
    {
        switch(is_stream_expected_to_start(queued_streams_, stream_id))
        {
          case StreamExpected::OURS_AS_EXPECTED:
            break;

          case StreamExpected::OURS_QUEUED:
            queued_streams_.pop_front();
            break;

          case StreamExpected::NOT_OURS:
          case StreamExpected::OURS_WRONG_ID:
            break;

          case StreamExpected::UNEXPECTEDLY_OURS:
            retry_data_.reset();
            return;

          case StreamExpected::UNEXPECTEDLY_NOT_OURS:
            unplug(false);
            return;

          case StreamExpected::EMPTY_AS_EXPECTED:
            BUG("Got play notification for empty queue and invalid stream");
            return;

          case StreamExpected::INVALID_ID:
            return;
        }
    }

    retry_data_.playing(stream_id);

    if(player_ != nullptr)
    {
        if(crawler_ != nullptr)
        {
            auto crawler_lock(crawler_->lock());
            auto *crawler = dynamic_cast<Playlist::DirectoryCrawler *>(crawler_);
            const auto *const info =
                player_->get_stream_preplay_info(ID::OurStream::make_from_generic_id(stream_id));

            if(info != nullptr)
                crawler->mark_position(info->list_id_, info->line_, info->directory_depth_,
                                       info->is_crawler_direction_reverse_
                                       ? Playlist::CrawlerIface::Direction::BACKWARD
                                       : Playlist::CrawlerIface::Direction::FORWARD);
            else
                BUG("No list position for stream %u", stream_id.get_raw_id());
        }

        enforce_intention(player_->get_intention(), Player::StreamState::PLAYING,
                          audio_source_);
    }
}

Player::Control::StopReaction
Player::Control::stop_notification(ID::Stream stream_id)
{

    if(player_ == nullptr || crawler_ == nullptr)
        return StopReaction::NOT_ATTACHED;

    bool stop_regardless_of_intention = false;

    auto crawler_lock(crawler_->lock());

    switch(is_stream_expected_playing(retry_data_.get_stream_id(),
                                      queued_streams_, stream_id,
                                      ExpectedPlayingCheckMode::STOPPED))
    {
      case StreamExpected::OURS_AS_EXPECTED:
        break;

      case StreamExpected::OURS_QUEUED:
      case StreamExpected::UNEXPECTEDLY_OURS:
      case StreamExpected::OURS_WRONG_ID:
        /* this case is a result of very fast skipping or some internal
         * processing error; in the latter case, we'll just accept the player's
         * stop notification and do not attempt to fight it */
        stop_regardless_of_intention = !crawler_->is_crawling();
        break;

      case StreamExpected::EMPTY_AS_EXPECTED:
      case StreamExpected::NOT_OURS:
      case StreamExpected::UNEXPECTEDLY_NOT_OURS:
      case StreamExpected::INVALID_ID:
        /* so what... */
        return StopReaction::STREAM_IGNORED;
    }

    bool skipping = false;
    const auto intention = stop_regardless_of_intention
        ? UserIntention::STOPPING
        : player_->get_intention();

    /* stream stopped playing with no error---good? */
    switch(intention)
    {
      case UserIntention::NOTHING:
      case UserIntention::STOPPING:
        crawler_->configure_and_restart(crawler_->get_recursive_mode(),
                                        crawler_->get_shuffle_mode());
        return StopReaction::STOPPED;

      case UserIntention::PAUSING:
      case UserIntention::LISTENING:
        break;

      case UserIntention::SKIPPING_PAUSED:
      case UserIntention::SKIPPING_LIVE:
        skipping = true;
        break;
    }

    /* at this point we know for sure that it was indeed our stream that has
     * failed */

    player_->forget_stream(stream_id);
    retry_data_.reset();

    switch(prefetch_state_)
    {
      case PrefetchState::NOT_PREFETCHING:
        /* probably end of list, checked below by finding the next item */
        break;

      case PrefetchState::PREFETCHING_NEXT_LIST_ITEM:
        /* still prefetching next stream information while the current stream
         * has stopped already */
        msg_info("Stream stopped while next stream is still unavailable; "
                 "audible gap is very likely");
        prefetch_state_ = PrefetchState::PREFETCHING_NEXT_LIST_ITEM_AND_PLAY_IT;
        return StopReaction::QUEUED;

      case PrefetchState::PREFETCHING_NEXT_LIST_ITEM_AND_PLAY_IT:
        BUG("This case should not occur");
        return StopReaction::QUEUED;

      case PrefetchState::HAVE_NEXT_LIST_ITEM:
        /* we have the item already, now we need to retrieve its information */
        prefetch_state_ = PrefetchState::PREFETCHING_LIST_ITEM_INFORMATION;

        if(crawler_->retrieve_item_information(std::bind(&Player::Control::async_stream_details_for_playing,
                                                         this,
                                                         std::placeholders::_1,
                                                         std::placeholders::_2)))
            return StopReaction::RETRIEVE_QUEUED;

        prefetch_state_ = PrefetchState::NOT_PREFETCHING;

        if(!permissions_->can_skip_on_error())
            return StopReaction::STOPPED;

        break;

      case PrefetchState::PREFETCHING_LIST_ITEM_INFORMATION:
      case PrefetchState::HAVE_LIST_ITEM_INFORMATION:
        /* item information are fetched or already there, so there is nothing
         * to do here */
        return Player::Control::StopReaction::NOP;
    }

    if(!skipping)
        crawler_->set_direction_forward();

    switch(crawler_->find_next(std::bind(&Player::Control::async_list_entry_for_playing,
                                         this,
                                         std::placeholders::_1,
                                         std::placeholders::_2)))
    {
      case Playlist::CrawlerIface::FindNextFnResult::SEARCHING:
        return StopReaction::QUEUED;

      case Playlist::CrawlerIface::FindNextFnResult::STOPPED_AT_START_OF_LIST:
      case Playlist::CrawlerIface::FindNextFnResult::STOPPED_AT_END_OF_LIST:
      case Playlist::CrawlerIface::FindNextFnResult::FAILED:
        break;
    }

    return StopReaction::STOPPED;
}

/*!
 * Try to fill up the streamplayer FIFO.
 *
 * The function sends the given URI to the stream player's queue.
 *
 * No exception thrown in here because the caller needs to react to specific
 * situations.
 *
 * \param stream_id
 *     Internal ID of the stream for mapping it to extra information maintained
 *     by us.
 *
 * \param stream_key
 *     Stream key as passed in from the stream source.
 *
 * \param queue_mode
 *     How to manipulate the stream player URL FIFO.
 *
 * \param play_new_mode
 *     If set to #Player::Control::PlayNewMode::SEND_PLAY_COMMAND_IF_IDLE or
 *     #Player::Control::PlayNewMode::SEND_PAUSE_COMMAND_IF_IDLE, then request
 *     immediate playback or pause of the selected list entry in case the
 *     player is idle. Otherwise, the entry is just pushed into the player's
 *     internal queue.
 *
 * \param queued_url
 *     Which URL was chosen for this stream.
 *
 * \param asrc
 *     Audio source to take D-Bus proxies from.
 *
 * \returns
 *     True in case of success, false otherwise.
 */
static bool send_selected_file_uri_to_streamplayer(ID::OurStream stream_id,
                                                   const GVariantWrapper &stream_key,
                                                   Player::Control::QueueMode queue_mode,
                                                   Player::Control::PlayNewMode play_new_mode,
                                                   const std::string &queued_url,
                                                   const Player::AudioSource &asrc)
{
    if(queued_url.empty())
        return false;

    msg_info("Passing URI for stream %u to player: \"%s\"",
             stream_id.get().get_raw_id(), queued_url.c_str());

    gboolean fifo_overflow = FALSE;
    gboolean is_playing = FALSE;

    gint16 keep_first_n = -1;

    switch(queue_mode)
    {
      case Player::Control::QueueMode::APPEND:
        break;

      case Player::Control::QueueMode::REPLACE_QUEUE:
        keep_first_n = 0;
        break;

      case Player::Control::QueueMode::REPLACE_ALL:
        keep_first_n = -2;
        break;
    }

    auto *urlfifo_proxy = asrc.get_urlfifo_proxy();

    if(urlfifo_proxy != nullptr)
    {
        GError *error = NULL;
        tdbus_splay_urlfifo_call_push_sync(urlfifo_proxy,
                                           stream_id.get().get_raw_id(),
                                           queued_url.c_str(), GVariantWrapper::get(stream_key),
                                           0, "ms", 0, "ms", keep_first_n,
                                           &fifo_overflow, &is_playing,
                                           NULL, &error);

        if(dbus_common_handle_error(&error, "Push stream") < 0)
        {
            msg_error(0, LOG_NOTICE, "Failed queuing URI to streamplayer");
            return false;
        }
    }

    if(fifo_overflow)
    {
        BUG("URL FIFO overflow, losing item %u", stream_id.get().get_raw_id());
        return false;
    }

    if(is_playing)
        return true;

    switch(play_new_mode)
    {
      case Player::Control::PlayNewMode::KEEP:
        break;

      case Player::Control::PlayNewMode::SEND_PLAY_COMMAND_IF_IDLE:
        if(!send_play_command(&asrc))
        {
            msg_error(0, LOG_NOTICE, "Failed sending start playback message");
            return false;
        }

        break;

      case Player::Control::PlayNewMode::SEND_PAUSE_COMMAND_IF_IDLE:
        if(!send_pause_command(&asrc))
        {
            msg_error(0, LOG_NOTICE, "Failed sending pause playback message");
            return false;
        }

        break;
    }

    return true;
}

static Player::StreamPreplayInfo::OpResult
queue_stream_or_forget(Player::Data &player, ID::OurStream stream_id,
                       Player::Control::QueueMode queue_mode,
                       Player::Control::PlayNewMode play_new_mode,
                       const Player::StreamPreplayInfo::ResolvedRedirectCallback &callback,
                       const Player::AudioSource *asrc)
{
    const GVariantWrapper *stream_key;
    const std::string *uri = nullptr;
    const auto result(player.get_first_stream_uri(stream_id, stream_key, uri, callback));

    if(uri == nullptr)
        return result;

    if(asrc == nullptr ||
       !send_selected_file_uri_to_streamplayer(stream_id, *stream_key,
                                               queue_mode, play_new_mode,
                                               *uri, *asrc))
    {
        player.forget_stream(stream_id.get());
        return Player::StreamPreplayInfo::OpResult::FAILED;
    }

    return result;
}

Player::Control::StopReaction
Player::Control::stop_notification(ID::Stream stream_id,
                                   const std::string &error_id,
                                   bool is_urlfifo_empty)
{
    log_assert(!error_id.empty());

    if(player_ == nullptr || crawler_ == nullptr)
        return StopReaction::NOT_ATTACHED;

    bool stop_regardless_of_intention = false;

    /* stream stopped playing due to some error---why? */
    const StoppedReason reason(error_id);

    const StreamExpected stream_expected_result =
        (reason.get_code() != StoppedReason::Code::FLOW_EMPTY_URLFIFO &&
         reason.get_code() != StoppedReason::Code::FLOW_ALREADY_STOPPED)
        ? is_stream_expected_playing(retry_data_.get_stream_id(),
                                     queued_streams_, stream_id,
                                     ExpectedPlayingCheckMode::STOPPED_WITH_ERROR)
        : StreamExpected::OURS_AS_EXPECTED;

    switch(stream_expected_result)
    {
      case StreamExpected::OURS_AS_EXPECTED:
      case StreamExpected::OURS_QUEUED:
        break;

      case StreamExpected::UNEXPECTEDLY_OURS:
      case StreamExpected::OURS_WRONG_ID:
        /* this case is a result of some internal processing error, so we'll
         * just accept the player's stop notification and do not attempt to
         * fight it */
        stop_regardless_of_intention = true;
        break;

      case StreamExpected::EMPTY_AS_EXPECTED:
      case StreamExpected::NOT_OURS:
      case StreamExpected::UNEXPECTEDLY_NOT_OURS:
      case StreamExpected::INVALID_ID:
        /* so what... */
        return StopReaction::STREAM_IGNORED;
    }

    msg_info("Stream error %s -> %d.%d, URL FIFO %sempty, expected queue size %zu",
             error_id.c_str(), static_cast<unsigned int>(reason.get_domain()),
             static_cast<unsigned int>(reason.get_code()),
             is_urlfifo_empty ? "" : "not ", queued_streams_.size());

    auto crawler_lock(crawler_->lock());
    bool skipping = false;
    PlayNewMode replay_mode = PlayNewMode::KEEP;
    const auto intention = stop_regardless_of_intention
        ? UserIntention::STOPPING
        : player_->get_intention();

    switch(intention)
    {
      case UserIntention::NOTHING:
      case UserIntention::STOPPING:
        crawler_->configure_and_restart(crawler_->get_recursive_mode(),
                                        crawler_->get_shuffle_mode());
        return StopReaction::STOPPED;

      case UserIntention::PAUSING:
        replay_mode = PlayNewMode::SEND_PAUSE_COMMAND_IF_IDLE;
        break;

      case UserIntention::LISTENING:
        replay_mode = PlayNewMode::SEND_PLAY_COMMAND_IF_IDLE;
        break;

      case UserIntention::SKIPPING_PAUSED:
        replay_mode = PlayNewMode::SEND_PAUSE_COMMAND_IF_IDLE;
        skipping = true;
        break;

      case UserIntention::SKIPPING_LIVE:
        replay_mode = PlayNewMode::SEND_PLAY_COMMAND_IF_IDLE;
        skipping = true;
        break;
    }

    /* at this point we know for sure that it was indeed our stream that has
     * failed */
    bool should_retry = false;

    switch(reason.get_code())
    {
      case StoppedReason::Code::UNKNOWN:
      case StoppedReason::Code::FLOW_REPORTED_UNKNOWN:
      case StoppedReason::Code::FLOW_EMPTY_URLFIFO:
      case StoppedReason::Code::IO_MEDIA_FAILURE:
      case StoppedReason::Code::IO_AUTHENTICATION_FAILURE:
      case StoppedReason::Code::IO_STREAM_UNAVAILABLE:
      case StoppedReason::Code::IO_STREAM_TYPE_NOT_SUPPORTED:
      case StoppedReason::Code::IO_ACCESS_DENIED:
      case StoppedReason::Code::DATA_CODEC_MISSING:
      case StoppedReason::Code::DATA_WRONG_FORMAT:
      case StoppedReason::Code::DATA_ENCRYPTED:
      case StoppedReason::Code::DATA_ENCRYPTION_SCHEME_NOT_SUPPORTED:
        break;

      case StoppedReason::Code::FLOW_ALREADY_STOPPED:
        return Player::Control::StopReaction::QUEUED;

      case StoppedReason::Code::IO_NETWORK_FAILURE:
      case StoppedReason::Code::IO_URL_MISSING:
      case StoppedReason::Code::IO_PROTOCOL_VIOLATION:
        should_retry = true;
        break;

      case StoppedReason::Code::DATA_BROKEN_STREAM:
        should_retry = permissions_->retry_if_stream_broken();
        break;
    }

    if(should_retry)
    {
        bool may_prefetch_more;
        const auto replay_result =
            replay(ID::OurStream::make_from_generic_id(stream_id), true,
                   replay_mode, may_prefetch_more);

        switch(replay_result)
        {
          case ReplayResult::OK:
            if(may_prefetch_more)
                need_next_item_hint(false);

            return StopReaction::RETRY;

          case ReplayResult::RETRY_FAILED_HARD:
            return StopReaction::STOPPED;

          case ReplayResult::GAVE_UP:
          case ReplayResult::EMPTY_QUEUE:
            break;
        }
    }
    else if(stream_expected_result == StreamExpected::OURS_QUEUED)
    {
        player_->forget_stream(queued_streams_.front().get());
        queued_streams_.pop_front();
    }

    player_->forget_stream(stream_id);
    retry_data_.reset();

    if(!permissions_->can_skip_on_error())
        return StopReaction::STOPPED;

    /* so we should skip to the next stream---what *is* the "next" stream? */
    if(is_urlfifo_empty)
    {
        if(!queued_streams_.empty())
        {
            /* looks slightly out of sync because of a race between the stream
             * player and this process (stream player has sent the stop
             * notification before it could process our last push), but
             * everything is fine */
            return StopReaction::QUEUED;
        }
        else if(crawler_->is_busy())
        {
            /* empty URL FIFO, and we haven't anything either---crawler is
             * already busy fetching the next stream, so everything is fine,
             * just a bit too slow for the user's actions */
            return StopReaction::QUEUED;
        }

        /* nothing queued anywhere or gave up---maybe we can find some other
         * stream to play below */
    }
    else
    {
        if(queued_streams_.empty())
        {
            BUG("Out of sync: stream player stopped our stream with error "
                "and has streams queued, but we don't known which");
            return StopReaction::STOPPED;
        }

        switch(replay_mode)
        {
          case PlayNewMode::KEEP:
            break;

          case PlayNewMode::SEND_PLAY_COMMAND_IF_IDLE:
            /* play next in queue */
            if(send_play_command(audio_source_))
                return StopReaction::TAKE_NEXT;

            break;

          case PlayNewMode::SEND_PAUSE_COMMAND_IF_IDLE:
            /* pause next in queue */
            if(send_pause_command(audio_source_))
                return StopReaction::TAKE_NEXT;

            break;
        }
    }

    if(!skipping)
        crawler_->set_direction_from_marked_position();

    switch(crawler_->find_next(std::bind(&Player::Control::async_list_entry_for_playing,
                                         this,
                                         std::placeholders::_1,
                                         std::placeholders::_2)))
    {
      case Playlist::CrawlerIface::FindNextFnResult::SEARCHING:
        return StopReaction::QUEUED;

      case Playlist::CrawlerIface::FindNextFnResult::STOPPED_AT_START_OF_LIST:
      case Playlist::CrawlerIface::FindNextFnResult::STOPPED_AT_END_OF_LIST:
      case Playlist::CrawlerIface::FindNextFnResult::FAILED:
        break;
    }

    return StopReaction::STOPPED;
}

void Player::Control::pause_notification(ID::Stream stream_id)
{
    retry_data_.playing(stream_id);

    if(is_active_controller())
        enforce_intention(player_->get_intention(), Player::StreamState::PAUSED,
                          audio_source_);
}


void Player::Control::need_next_item_hint(bool queue_is_full)
{
    if(!is_active_controller())
        return;

    if(queue_is_full)
        BUG("Streamplayer reports full queue");

    if(!permissions_->can_prefetch_for_gapless())
        return;

    if(crawler_ == nullptr)
        return;

    auto crawler_lock(crawler_->lock());

    switch(prefetch_state_)
    {
      case PrefetchState::NOT_PREFETCHING:
        break;

      case PrefetchState::PREFETCHING_NEXT_LIST_ITEM:
      case PrefetchState::PREFETCHING_NEXT_LIST_ITEM_AND_PLAY_IT:
      case PrefetchState::HAVE_NEXT_LIST_ITEM:
      case PrefetchState::PREFETCHING_LIST_ITEM_INFORMATION:
      case PrefetchState::HAVE_LIST_ITEM_INFORMATION:
        return;
    }

    if(queued_streams_.size() >= permissions_->maximum_number_of_prefetched_streams())
        return;

    /* we always prefetch the next item even for sources that do not support
     * gapless playback to mask possible network latencies caused by that
     * operation; we do not, however, fetch the next item's detailed data
     * because this may cause problems on non-gapless sources */
    prefetch_state_ = PrefetchState::PREFETCHING_NEXT_LIST_ITEM;

    switch(crawler_->find_next(std::bind(&Player::Control::async_list_entry_prefetched,
                                         this,
                                         std::placeholders::_1,
                                         std::placeholders::_2)))
    {
      case Playlist::CrawlerIface::FindNextFnResult::SEARCHING:
        break;

      case Playlist::CrawlerIface::FindNextFnResult::STOPPED_AT_START_OF_LIST:
      case Playlist::CrawlerIface::FindNextFnResult::STOPPED_AT_END_OF_LIST:
      case Playlist::CrawlerIface::FindNextFnResult::FAILED:
        prefetch_state_ = PrefetchState::NOT_PREFETCHING;
        break;
    }
}

static void not_attached_bug(const char *what, bool and_crawler = false)
{
    BUG("%s, but not attached to player%s anymore",
        what, and_crawler ? " and/or crawler" : "");
}

void Player::Control::async_list_entry_for_playing(Playlist::CrawlerIface &crawler,
                                                   Playlist::CrawlerIface::FindNextItemResult result)
{
    auto locks(lock());

    if(player_ == nullptr)
    {
        not_attached_bug("Found list item for playing");
        return;
    }

    switch(result)
    {
      case Playlist::CrawlerIface::FindNextItemResult::FOUND:
        crawler.mark_current_position();
        prefetch_state_ = PrefetchState::PREFETCHING_LIST_ITEM_INFORMATION;

        if(crawler.retrieve_item_information(std::bind(&Player::Control::async_stream_details_for_playing,
                                                       this,
                                                       std::placeholders::_1,
                                                       std::placeholders::_2)))
            return;

        break;

      case Playlist::CrawlerIface::FindNextItemResult::FAILED:
      case Playlist::CrawlerIface::FindNextItemResult::CANCELED:
      case Playlist::CrawlerIface::FindNextItemResult::START_OF_LIST:
      case Playlist::CrawlerIface::FindNextItemResult::END_OF_LIST:
        break;
    }

    prefetch_state_ = PrefetchState::NOT_PREFETCHING;
    skip_requests_.stop_skipping(*player_, *crawler_);

    if(stop_playing_notification_ != nullptr)
        stop_playing_notification_();
}

void Player::Control::async_list_entry_to_skip(Playlist::CrawlerIface &crawler,
                                               Playlist::CrawlerIface::FindNextItemResult result)
{
    auto locks(lock());

    if(player_ == nullptr)
    {
        not_attached_bug("Found list item to skip");
        return;
    }

    switch(result)
    {
      case Playlist::CrawlerIface::FindNextItemResult::FOUND:
        prefetch_state_ = PrefetchState::HAVE_NEXT_LIST_ITEM;

        switch(skip_requests_.skipped(*player_, *crawler_, true))
        {
          case Player::Skipper::SkippedResult::SKIPPING_FORWARD:
          case Player::Skipper::SkippedResult::SKIPPING_BACKWARD:
            switch(crawler.find_next(std::bind(&Player::Control::async_list_entry_to_skip,
                                               this,
                                               std::placeholders::_1,
                                               std::placeholders::_2)))
            {
              case Playlist::CrawlerIface::FindNextFnResult::SEARCHING:
                break;

              case Playlist::CrawlerIface::FindNextFnResult::STOPPED_AT_START_OF_LIST:
              case Playlist::CrawlerIface::FindNextFnResult::STOPPED_AT_END_OF_LIST:
              case Playlist::CrawlerIface::FindNextFnResult::FAILED:
                skip_requests_.stop_skipping(*player_, *crawler_);
                break;
            }

            return;

          case Player::Skipper::SkippedResult::DONE_FORWARD:
          case Player::Skipper::SkippedResult::DONE_BACKWARD:
            break;
        }

        prefetch_state_ = PrefetchState::PREFETCHING_LIST_ITEM_INFORMATION;

        if(crawler.retrieve_item_information(std::bind(&Player::Control::async_stream_details_for_playing,
                                                       this,
                                                       std::placeholders::_1,
                                                       std::placeholders::_2)))
            return;

        break;

      case Playlist::CrawlerIface::FindNextItemResult::START_OF_LIST:
        if(!crawler_->resume_crawler())
            break;

        skip_requests_.stop_skipping(*player_, *crawler_);
        prefetch_state_ = PrefetchState::PREFETCHING_LIST_ITEM_INFORMATION;

        if(crawler.retrieve_item_information(std::bind(&Player::Control::async_stream_details_for_playing,
                                                       this,
                                                       std::placeholders::_1,
                                                       std::placeholders::_2)))
            return;

        break;

      case Playlist::CrawlerIface::FindNextItemResult::FAILED:
      case Playlist::CrawlerIface::FindNextItemResult::CANCELED:
      case Playlist::CrawlerIface::FindNextItemResult::END_OF_LIST:
        break;
    }

    prefetch_state_ = PrefetchState::NOT_PREFETCHING;
    skip_requests_.stop_skipping(*player_, *crawler_);
}

void Player::Control::async_list_entry_prefetched(Playlist::CrawlerIface &crawler,
                                                  Playlist::CrawlerIface::FindNextItemResult result)
{
    auto locks(lock());

    if(player_ == nullptr)
    {
        not_attached_bug("Found list item for prefetching");
        return;
    }

    switch(result)
    {
      case Playlist::CrawlerIface::FindNextItemResult::FOUND:
        prefetch_state_ = PrefetchState::HAVE_NEXT_LIST_ITEM;

        if(!permissions_->can_prefetch_for_gapless())
        {
            /* gapless playback is actually not supported in this list,
             * retrieval of item information must be deferred to a later
             * point if and when needed */
            break;
        }

        prefetch_state_ = PrefetchState::PREFETCHING_LIST_ITEM_INFORMATION;

        if(crawler.retrieve_item_information(std::bind(&Player::Control::async_stream_details_prefetched,
                                                       this,
                                                       std::placeholders::_1,
                                                       std::placeholders::_2)))
            return;

        break;

      case Playlist::CrawlerIface::FindNextItemResult::FAILED:
      case Playlist::CrawlerIface::FindNextItemResult::CANCELED:
      case Playlist::CrawlerIface::FindNextItemResult::START_OF_LIST:
      case Playlist::CrawlerIface::FindNextItemResult::END_OF_LIST:
        break;
    }

    prefetch_state_ = PrefetchState::NOT_PREFETCHING;
}

void Player::Control::async_stream_details_for_playing(Playlist::CrawlerIface &crawler,
                                                       Playlist::CrawlerIface::RetrieveItemInfoResult result)
{
    auto locks(lock());

    if(player_ == nullptr)
    {
        not_attached_bug("Found item information for playing");
        return;
    }

    switch(result)
    {
      case Playlist::CrawlerIface::RetrieveItemInfoResult::DROPPED:
        break;

      case Playlist::CrawlerIface::RetrieveItemInfoResult::FOUND:
        switch(player_->get_intention())
        {
          case UserIntention::NOTHING:
          case UserIntention::STOPPING:
            prefetch_state_ = PrefetchState::HAVE_LIST_ITEM_INFORMATION;
            break;

          case UserIntention::SKIPPING_PAUSED:
            if(skip_requests_.has_pending_skip_requests())
            {
                switch(crawler.find_next(std::bind(&Player::Control::async_list_entry_to_skip,
                                                   this,
                                                   std::placeholders::_1,
                                                   std::placeholders::_2)))
                {
                  case Playlist::CrawlerIface::FindNextFnResult::SEARCHING:
                    return;

                  case Playlist::CrawlerIface::FindNextFnResult::STOPPED_AT_START_OF_LIST:
                  case Playlist::CrawlerIface::FindNextFnResult::STOPPED_AT_END_OF_LIST:
                  case Playlist::CrawlerIface::FindNextFnResult::FAILED:
                    break;
                }
            }

            skip_requests_.stop_skipping(*player_, *crawler_);

            /* fall-through */

          case UserIntention::PAUSING:
            switch(process_crawler_item(&Player::Control::async_redirect_resolved_for_playing,
                                        QueueMode::REPLACE_ALL,
                                        PlayNewMode::SEND_PAUSE_COMMAND_IF_IDLE))
            {
              case StreamPreplayInfo::OpResult::SUCCEEDED:
                prefetch_state_ = PrefetchState::NOT_PREFETCHING;
                crawler.set_direction_forward();
                need_next_item_hint(false);
                break;

              case StreamPreplayInfo::OpResult::STARTED:
              case StreamPreplayInfo::OpResult::FAILED:
              case StreamPreplayInfo::OpResult::CANCELED:
                prefetch_state_ = PrefetchState::NOT_PREFETCHING;
                break;
            }

            break;

          case UserIntention::SKIPPING_LIVE:
            if(skip_requests_.has_pending_skip_requests())
            {
                switch(crawler.find_next(std::bind(&Player::Control::async_list_entry_to_skip,
                                                   this,
                                                   std::placeholders::_1,
                                                   std::placeholders::_2)))
                {
                  case Playlist::CrawlerIface::FindNextFnResult::SEARCHING:
                    return;

                  case Playlist::CrawlerIface::FindNextFnResult::STOPPED_AT_START_OF_LIST:
                  case Playlist::CrawlerIface::FindNextFnResult::STOPPED_AT_END_OF_LIST:
                  case Playlist::CrawlerIface::FindNextFnResult::FAILED:
                    break;
                }
            }

            skip_requests_.stop_skipping(*player_, *crawler_);

            /* fall-through */

          case UserIntention::LISTENING:
            switch(process_crawler_item(&Player::Control::async_redirect_resolved_for_playing,
                                        QueueMode::REPLACE_ALL,
                                        PlayNewMode::SEND_PLAY_COMMAND_IF_IDLE))
            {
              case StreamPreplayInfo::OpResult::SUCCEEDED:
                send_play_command(audio_source_);
                break;

              case StreamPreplayInfo::OpResult::STARTED:
              case StreamPreplayInfo::OpResult::FAILED:
              case StreamPreplayInfo::OpResult::CANCELED:
                break;
            }

            prefetch_state_ = PrefetchState::NOT_PREFETCHING;

            break;
        }

        break;

      case Playlist::CrawlerIface::RetrieveItemInfoResult::FOUND__NO_URL:
      case Playlist::CrawlerIface::RetrieveItemInfoResult::FAILED:
        /* skip this one, maybe the next one will work */
        prefetch_state_ = PrefetchState::NOT_PREFETCHING;
        crawler.set_direction_forward();
        crawler.find_next(std::bind(&Player::Control::async_list_entry_for_playing,
                                    this,
                                    std::placeholders::_1,
                                    std::placeholders::_2));
        break;

      case Playlist::CrawlerIface::RetrieveItemInfoResult::CANCELED:
        break;
    }
}

void Player::Control::async_stream_details_prefetched(Playlist::CrawlerIface &crawler,
                                                      Playlist::CrawlerIface::RetrieveItemInfoResult result)
{
    auto locks(lock());

    if(player_ == nullptr)
    {
        not_attached_bug("Found item information for prefetching");
        return;
    }

    switch(result)
    {
      case Playlist::CrawlerIface::RetrieveItemInfoResult::DROPPED:
        break;

      case Playlist::CrawlerIface::RetrieveItemInfoResult::FOUND:
        switch(player_->get_intention())
        {
          case UserIntention::NOTHING:
          case UserIntention::STOPPING:
            prefetch_state_ = PrefetchState::HAVE_LIST_ITEM_INFORMATION;
            break;

          case UserIntention::SKIPPING_PAUSED:
          case UserIntention::SKIPPING_LIVE:
            skip_requests_.stop_skipping(*player_, *crawler_);

            /* fall-through */

          case UserIntention::PAUSING:
          case UserIntention::LISTENING:
            switch(process_crawler_item(&Player::Control::async_redirect_resolved_prefetched,
                                        QueueMode::APPEND,
                                        PlayNewMode::KEEP))
            {
              case StreamPreplayInfo::OpResult::SUCCEEDED:
                prefetch_state_ = PrefetchState::NOT_PREFETCHING;
                crawler.set_direction_forward();
                need_next_item_hint(false);
                break;

              case StreamPreplayInfo::OpResult::STARTED:
              case StreamPreplayInfo::OpResult::FAILED:
              case StreamPreplayInfo::OpResult::CANCELED:
                prefetch_state_ = PrefetchState::NOT_PREFETCHING;
                break;
            }

            break;
        }

        break;

      case Playlist::CrawlerIface::RetrieveItemInfoResult::FOUND__NO_URL:
      case Playlist::CrawlerIface::RetrieveItemInfoResult::FAILED:
        prefetch_state_ = PrefetchState::NOT_PREFETCHING;
        crawler.set_direction_forward();
        crawler.find_next(std::bind(&Player::Control::async_list_entry_prefetched,
                                    this,
                                    std::placeholders::_1,
                                    std::placeholders::_2));
        break;

      case Playlist::CrawlerIface::RetrieveItemInfoResult::CANCELED:
        break;
    }
}

static bool async_redirect_check_preconditions(Player::Data *player,
                                               Playlist::CrawlerIface *crawler,
                                               Player::StreamPreplayInfo::ResolvedRedirectResult result,
                                               size_t idx, const char *what)
{
    if(player == nullptr || crawler == nullptr)
    {
        not_attached_bug(what, true);
        return false;
    }

    switch(result)
    {
      case Player::StreamPreplayInfo::ResolvedRedirectResult::FOUND:
        break;

      case Player::StreamPreplayInfo::ResolvedRedirectResult::FAILED:
        BUG("%s: canceled at %zu, but case not handled", what, idx);
        return false;

      case Player::StreamPreplayInfo::ResolvedRedirectResult::CANCELED:
        BUG("%s: failed at %zu, but case not handled", what, idx);
        return false;
    }

    return true;
}

void Player::Control::async_redirect_resolved_for_playing(size_t idx,
                                                          StreamPreplayInfo::ResolvedRedirectResult result,
                                                          ID::OurStream for_stream,
                                                          QueueMode queue_mode,
                                                          PlayNewMode play_new_mode)
{
    auto locks(lock());

    if(!async_redirect_check_preconditions(player_, crawler_, result, idx,
                                           "Resolved redirect for playing"))
        return;

    switch(player_->get_intention())
    {
      case UserIntention::NOTHING:
      case UserIntention::STOPPING:
        break;

      case UserIntention::SKIPPING_PAUSED:
      case UserIntention::PAUSING:
        switch(process_crawler_item_tail(for_stream,
                                         queue_mode, play_new_mode,
                                         std::bind(&Player::Control::unexpected_resolve_error, this,
                                                   std::placeholders::_1, std::placeholders::_2)))
        {
          case StreamPreplayInfo::OpResult::SUCCEEDED:
            crawler_->set_direction_forward();
            need_next_item_hint(false);
            break;

          case StreamPreplayInfo::OpResult::STARTED:
          case StreamPreplayInfo::OpResult::FAILED:
          case StreamPreplayInfo::OpResult::CANCELED:
            break;
        }

        break;

      case UserIntention::SKIPPING_LIVE:
      case UserIntention::LISTENING:
        switch(process_crawler_item_tail(for_stream,
                                         queue_mode, play_new_mode,
                                         std::bind(&Player::Control::unexpected_resolve_error, this,
                                                   std::placeholders::_1, std::placeholders::_2)))
        {
          case StreamPreplayInfo::OpResult::SUCCEEDED:
            send_play_command(audio_source_);
            break;

          case StreamPreplayInfo::OpResult::STARTED:
          case StreamPreplayInfo::OpResult::FAILED:
          case StreamPreplayInfo::OpResult::CANCELED:
            break;
        }

        break;
    }
}

void Player::Control::async_redirect_resolved_prefetched(size_t idx,
                                                         StreamPreplayInfo::ResolvedRedirectResult result,
                                                         ID::OurStream for_stream,
                                                         QueueMode queue_mode,
                                                         PlayNewMode play_new_mode)
{
    auto locks(lock());

    if(!async_redirect_check_preconditions(player_, crawler_, result, idx,
                                           "Resolved redirect for prefetching"))
        return;

    switch(player_->get_intention())
    {
      case UserIntention::NOTHING:
      case UserIntention::STOPPING:
        break;

      case UserIntention::SKIPPING_PAUSED:
      case UserIntention::SKIPPING_LIVE:
      case UserIntention::PAUSING:
      case UserIntention::LISTENING:
        switch(process_crawler_item_tail(for_stream,
                                         queue_mode, play_new_mode,
                                         std::bind(&Player::Control::unexpected_resolve_error, this,
                                                   std::placeholders::_1, std::placeholders::_2)))
        {
          case StreamPreplayInfo::OpResult::SUCCEEDED:
            crawler_->set_direction_forward();
            need_next_item_hint(false);
            break;

          case StreamPreplayInfo::OpResult::STARTED:
          case StreamPreplayInfo::OpResult::FAILED:
          case StreamPreplayInfo::OpResult::CANCELED:
            break;
        }

        break;
    }
}

void Player::Control::unexpected_resolve_error(size_t idx,
                                               Player::StreamPreplayInfo::ResolvedRedirectResult result)
{
    BUG("Asynchronous resolution of Airable redirect failed unexpectedly "
        "for URL at index %zu, result %u", idx, static_cast<unsigned int>(result));
    unplug(false);
}

static MetaData::Set
mk_meta_data_from_preloaded_information(const Playlist::DirectoryCrawler::ItemInfo &item_info)
{
    const auto &preloaded(item_info.file_item_meta_data_);
    MetaData::Set meta_data;

    meta_data.add(MetaData::Set::ARTIST, preloaded.artist_.c_str(), ViewPlay::meta_data_reformatters);
    meta_data.add(MetaData::Set::ALBUM,  preloaded.album_.c_str(),  ViewPlay::meta_data_reformatters);
    meta_data.add(MetaData::Set::TITLE,  preloaded.title_.c_str(),  ViewPlay::meta_data_reformatters);
    meta_data.add(MetaData::Set::INTERNAL_DRCPD_TITLE, item_info.file_item_text_.c_str(),
                  ViewPlay::meta_data_reformatters);

    return meta_data;
}

Player::StreamPreplayInfo::OpResult
Player::Control::process_crawler_item(ProcessCrawlerItemAsyncRedirectResolved callback,
                                      QueueMode queue_mode, PlayNewMode play_new_mode)
{
    if(player_ == nullptr || crawler_ == nullptr)
        return StreamPreplayInfo::OpResult::CANCELED;

    auto *crawler = dynamic_cast<Playlist::DirectoryCrawler *>(crawler_);
    log_assert(crawler != nullptr);

    auto &item_info(crawler->get_current_list_item_info_non_const());
    log_assert(item_info.position_.get_list_id().is_valid());
    log_assert(item_info.is_item_info_valid_);

    item_info.airable_links_.finalize(bitrate_limiter_);

    /* we'll steal some data from the item info for efficiency */
    const ID::OurStream stream_id(player_->store_stream_preplay_information(
                                        std::move(item_info.stream_key_),
                                        std::move(item_info.stream_uris_),
                                        std::move(item_info.airable_links_),
                                        item_info.position_.get_list_id(),
                                        item_info.position_.get_line(),
                                        item_info.position_.get_directory_depth(),
                                        crawler_->get_active_direction() == Playlist::CrawlerIface::Direction::BACKWARD));

    if(!stream_id.get().is_valid())
        return StreamPreplayInfo::OpResult::FAILED;

    switch(queue_mode)
    {
      case QueueMode::APPEND:
        break;

      case QueueMode::REPLACE_QUEUE:
        log_assert(play_new_mode == PlayNewMode::KEEP);
        forget_queued_and_playing(false);
        break;

      case QueueMode::REPLACE_ALL:
        log_assert(play_new_mode != PlayNewMode::KEEP);
        forget_queued_and_playing(true);
        break;
    }

    player_->put_meta_data(stream_id.get(),
                           std::move(mk_meta_data_from_preloaded_information(item_info)));

    return process_crawler_item_tail(stream_id, queue_mode, play_new_mode,
                                     std::bind(callback, this,
                                               std::placeholders::_1, std::placeholders::_2,
                                               stream_id, queue_mode, play_new_mode));
}

Player::StreamPreplayInfo::OpResult
Player::Control::process_crawler_item_tail(ID::OurStream stream_id,
                                           QueueMode queue_mode,
                                           PlayNewMode play_new_mode,
                                           const StreamPreplayInfo::ResolvedRedirectCallback &callback)
{
    const auto result(queue_stream_or_forget(*player_, stream_id, queue_mode,
                                             play_new_mode, callback,
                                             audio_source_));

    switch(result)
    {
      case StreamPreplayInfo::OpResult::SUCCEEDED:
        queued_streams_.push_back(stream_id);
        break;

      case StreamPreplayInfo::OpResult::STARTED:
      case StreamPreplayInfo::OpResult::FAILED:
      case StreamPreplayInfo::OpResult::CANCELED:
        break;
    }

    return result;
}

Player::Control::ReplayResult
Player::Control::replay(ID::OurStream stream_id, bool is_retry,
                        PlayNewMode play_new_mode, bool &took_from_queue)
{
    log_assert(stream_id.get().is_valid());

    if(!is_retry && !queued_streams_.empty() && queued_streams_.front() == stream_id)
    {
        took_from_queue = true;
        queued_streams_.pop_front();
    }
    else
        took_from_queue = false;

    if(!retry_data_.retry(stream_id))
    {
        if(is_retry)
            msg_info("Giving up on stream %u", stream_id.get().get_raw_id());

        return ReplayResult::GAVE_UP;
    }

    if(is_retry)
        msg_info("Retry stream %u", stream_id.get().get_raw_id());

    bool is_queued = false;

    switch(queue_stream_or_forget(*player_, stream_id,
                                 Player::Control::QueueMode::REPLACE_ALL,
                                 play_new_mode,
                                 std::bind(&Player::Control::unexpected_resolve_error,
                                           this,
                                           std::placeholders::_1, std::placeholders::_2),
                                 audio_source_))
    {
      case StreamPreplayInfo::OpResult::STARTED:
        BUG("Unexpected async redirect resolution while replaying");
        return ReplayResult::RETRY_FAILED_HARD;

      case StreamPreplayInfo::OpResult::SUCCEEDED:
        is_queued = true;
        break;

      case StreamPreplayInfo::OpResult::FAILED:
      case StreamPreplayInfo::OpResult::CANCELED:
        break;
    }

    if(!is_queued && is_retry)
        return ReplayResult::RETRY_FAILED_HARD;

    decltype(queued_streams_) temp;

    if(is_queued)
        temp.push_back(stream_id);

    for(const auto id : queued_streams_)
    {
        switch(queue_stream_or_forget(*player_, id,
                                      QueueMode::APPEND,
                                      PlayNewMode::KEEP,
                                      std::bind(&Player::Control::unexpected_resolve_error, this,
                                                std::placeholders::_1, std::placeholders::_2),
                                      audio_source_))
        {
          case Player::StreamPreplayInfo::OpResult::STARTED:
            BUG("Unexpected queuing result while replaying");
            break;

          case Player::StreamPreplayInfo::OpResult::SUCCEEDED:
            temp.push_back(id);
            break;

          case Player::StreamPreplayInfo::OpResult::FAILED:
          case Player::StreamPreplayInfo::OpResult::CANCELED:
            break;
        }
    }

    queued_streams_.clear();

    if(!temp.empty())
        queued_streams_.swap(temp);

    msg_info("Queued %zu streams once again", queued_streams_.size());

    return queued_streams_.empty()
        ? ReplayResult::EMPTY_QUEUE
        : ReplayResult::OK;
}
