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

#include "player_control.hh"
#include "player_stopped_reason.hh"
#include "view_filebrowser_fileitem.hh"
#include "directory_crawler.hh"
#include "dbus_iface_deep.h"
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

bool Player::Skipper::stop_skipping(Player::Data &data, Playlist::CrawlerIface &crawler,
                                    Player::Skipper::StopSkipBehavior how)
{
    switch(how)
    {
      case StopSkipBehavior::STOP:
        reset();
        set_intention_from_skipping(data);
        break;

      case StopSkipBehavior::KEEP_SKIPPING_FORWARD:
      case StopSkipBehavior::KEEP_SKIPPING_IN_CURRENT_DIRECTION:
        log_assert(is_skipping_);
        pending_skip_requests_ = 0;

        if(how == StopSkipBehavior::KEEP_SKIPPING_IN_CURRENT_DIRECTION)
            return !crawler.is_crawling_forward();

        break;
    }

    return crawler.set_direction_forward();
}

Player::Skipper::SkipState
Player::Skipper::forward_request(Player::Data &data, Playlist::CrawlerIface &crawler,
                                 Player::UserIntention &previous_intention)
{
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
                         Player::Skipper::StopSkipBehavior how)
{
    if(pending_skip_requests_ == 0)
    {
        if(is_skipping_)
            crawler.mark_current_position();
        else
        {
            BUG("Got skipped notification, but not skipping");
            how = StopSkipBehavior::STOP;
        }

        return stop_skipping(data, crawler, how)
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

void Player::Control::plug(const ViewIface &view)
{
    log_assert(owning_view_ == nullptr);
    log_assert(crawler_ == nullptr);
    log_assert(permissions_ == nullptr);

    owning_view_ = &view;
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
    crawler_ = &crawler;
    permissions_ = &permissions;
    skip_requests_.reset();
    queued_streams_.clear();
    prefetch_state_ = PrefetchState::NOT_PREFETCHING;
    retry_data_.reset();

    auto crawler_lock(crawler_->lock());
    crawler_->attached_to_player_notification();
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

void Player::Control::unplug()
{
    auto locks(lock());

    owning_view_ = nullptr;

    forget_queued_and_playing(true);

    if(player_ != nullptr)
    {
        player_->detached_from_player_notification();
        player_ = nullptr;
    }

    if(crawler_ != nullptr)
    {
        auto crawler_lock(crawler_->lock());
        crawler_->detached_from_player_notification();
        crawler_ = nullptr;
    }

    permissions_ = nullptr;
}

static bool send_play_command()
{
    if(!tdbus_splay_playback_call_start_sync(dbus_get_streamplayer_playback_iface(),
                                             NULL, NULL))
    {
        msg_error(0, LOG_NOTICE, "Failed sending start playback message");
        return false;
    }
    else
        return true;
}

static bool send_stop_command()
{
    if(!tdbus_splay_playback_call_stop_sync(dbus_get_streamplayer_playback_iface(),
                                            NULL, NULL))
    {
        msg_error(0, LOG_NOTICE, "Failed sending stop playback message");
        return false;
    }
    else
        return true;
}

static bool send_pause_command()
{
    if(!tdbus_splay_playback_call_pause_sync(dbus_get_streamplayer_playback_iface(),
                                             NULL, NULL))
    {
        msg_error(0, LOG_NOTICE, "Failed sending pause playback message");
        return false;
    }
    else
        return true;
}

static bool send_skip_to_next_command(ID::Stream &removed_stream_from_queue,
                                      Player::StreamState &play_status)
{
    guint skipped_id;
    guint next_id;
    guchar raw_play_status;

    if(!tdbus_splay_urlfifo_call_next_sync(dbus_get_streamplayer_urlfifo_iface(),
                                           &skipped_id, &next_id,
                                           &raw_play_status, NULL, NULL))
    {
        msg_error(0, LOG_NOTICE, "Failed sending skip track message");
        return false;
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

static void resume_paused_stream(Player::Data *player)
{
    if(player != nullptr &&
       player->get_current_stream_state() == Player::StreamState::PAUSED)
        send_play_command();
}

void Player::Control::play_request()
{
    if(!is_active_controller())
    {
        /* foreign stream, but maybe we can resume it if paused */
        send_play_command();
        return;
    }

    if(permissions_ != nullptr && !permissions_->can_play())
    {
        msg_error(EPERM, LOG_INFO, "Ignoring play request");
        return;
    }

    player_->set_intention(UserIntention::LISTENING);

    switch(player_->get_current_stream_state())
    {
      case Player::StreamState::BUFFERING:
      case Player::StreamState::PLAYING:
        break;

      case Player::StreamState::STOPPED:
        {
            auto crawler_lock(crawler_->lock());
            crawler_->set_direction_forward();
            crawler_->find_next(std::bind(&Player::Control::found_list_item,
                                          this,
                                          std::placeholders::_1,
                                          std::placeholders::_2,
                                          CrawlerContext::IMMEDIATE_PLAY));
        }

        break;

      case Player::StreamState::PAUSED:
        resume_paused_stream(player_);
        break;
    }
}

void Player::Control::jump_to_crawler_location()
{
    if(!is_active_controller())
        return;

    auto crawler_lock(crawler_->lock());

    crawler_->set_direction_forward();
    crawler_->find_next(std::bind(&Player::Control::found_list_item,
                                  this,
                                  std::placeholders::_1,
                                  std::placeholders::_2,
                                  CrawlerContext::IMMEDIATE_PLAY));
}

void Player::Control::stop_request()
{
    if(is_active_controller())
        player_->set_intention(UserIntention::STOPPING);

    send_stop_command();
}

void Player::Control::pause_request()
{
    if(permissions_ != nullptr && !permissions_->can_pause())
    {
        msg_error(EPERM, LOG_INFO, "Ignoring pause request");
        return;
    }

    if(is_active_controller())
        player_->set_intention(UserIntention::PAUSING);

    send_pause_command();
}

static void enforce_intention(Player::UserIntention intention,
                              Player::StreamState known_stream_state)
{
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
            send_stop_command();
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
            send_pause_command();
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
            send_play_command();
            break;

          case Player::StreamState::BUFFERING:
          case Player::StreamState::PLAYING:
            break;
        }

        break;
    }
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
    bool peek_at_queue = false;

    switch(mode)
    {
      case ExpectedPlayingCheckMode::STOPPED:
        mode_name = "Stopped";
        peek_at_queue = false;
        break;

      case ExpectedPlayingCheckMode::STOPPED_WITH_ERROR:
        mode_name = "StoppedWithError";
        peek_at_queue = true;
        break;

      case ExpectedPlayingCheckMode::SKIPPED:
        mode_name = "Skipped";
        peek_at_queue = true;
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
        if(peek_at_queue && !queued.empty() && queued.front().get() == stream_id)
        {
            result = StreamExpected::OURS_QUEUED;
            break;
        }

        if(result == StreamExpected::UNEXPECTEDLY_OURS)
            BUG("Out of sync: %s our stream %u that we don't know about",
                mode_name, stream_id.get_raw_id());
        else
            BUG("Out of sync: %s stream ID should be %u, but streamplayer says it's %u",
                mode_name,
                current_stream_id.get().get_raw_id(), stream_id.get_raw_id());

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
        return false;

    if(permissions_ != nullptr && !permissions_->can_skip_forward())
    {
        msg_error(EPERM, LOG_INFO, "Ignoring skip forward request");
        return false;
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
                                          streamplayer_status))
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

            skip_requests_.skipped(*player_, *crawler_,
                                   Player::Skipper::StopSkipBehavior::STOP);
            enforce_intention(player_->get_intention(), streamplayer_status);

            retval = true;
        }
        else
            should_find_next = true;

        break;
    }

    if(should_find_next)
    {
        switch(crawler_->find_next(std::bind(&Player::Control::found_list_item, this,
                                             std::placeholders::_1,
                                             std::placeholders::_2,
                                             CrawlerContext::SKIP)))
        {
          case Playlist::CrawlerIface::FindNextFnResult::SEARCHING:
          case Playlist::CrawlerIface::FindNextFnResult::STOPPED:
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
        return;

    if(permissions_ != nullptr && !permissions_->can_skip_backward())
    {
        msg_error(EPERM, LOG_INFO, "Ignoring skip backward request");
        return;
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

        switch(crawler_->find_next(std::bind(&Player::Control::found_list_item,
                                             this,
                                             std::placeholders::_1,
                                             std::placeholders::_2,
                                             CrawlerContext::SKIP)))
        {
          case Playlist::CrawlerIface::FindNextFnResult::SEARCHING:
          case Playlist::CrawlerIface::FindNextFnResult::STOPPED:
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

    if(!tdbus_splay_playback_call_seek_sync(dbus_get_streamplayer_playback_iface(),
                                            0, "ms", NULL, NULL))
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
    if(!is_fast_winding_allowed(permissions_))
    {
        msg_error(EPERM, LOG_INFO,
                  "Ignoring fast wind set factor request");
        return;
    }

    BUG("%s(): not implemented", __func__);
    fast_wind_data_.speed_factor_ = speed_factor;
}

void Player::Control::fast_wind_set_direction_request(bool is_forward)
{
    if(!is_fast_winding_allowed(permissions_, is_forward))
    {
        msg_error(EPERM, LOG_INFO,
                  "Ignoring fast wind set direction %u request", is_forward);
        return;
    }

    BUG("%s(): not implemented", __func__);
    fast_wind_data_.is_forward_mode_ = is_forward;
}

void Player::Control::fast_wind_start_request() const
{
    if(!is_fast_winding_allowed(permissions_, fast_wind_data_.is_forward_mode_))
    {
        msg_error(EPERM, LOG_INFO,
                  "Ignoring fast wind start request in direction %u",
                  fast_wind_data_.is_forward_mode_);
        return;
    }

    BUG("%s(): not implemented", __func__);
}

void Player::Control::fast_wind_stop_request() const
{
    BUG("%s(): not implemented", __func__);
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
          case StreamExpected::UNEXPECTEDLY_OURS:
          case StreamExpected::OURS_WRONG_ID:
            break;

          case StreamExpected::UNEXPECTEDLY_NOT_OURS:
            unplug();
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
                crawler->mark_position(info->list_id_, info->line_, info->directory_depth_);
            else
                BUG("No list position for stream %u", stream_id.get_raw_id());
        }

        enforce_intention(player_->get_intention(), Player::StreamState::PLAYING);
    }
}

Player::Control::StopReaction
Player::Control::stop_notification(ID::Stream stream_id)
{

    if(player_ == nullptr || crawler_ == nullptr)
        return StopReaction::NOT_ATTACHED;

    bool stop_regardless_of_intention = false;

    switch(is_stream_expected_playing(retry_data_.get_stream_id(),
                                      queued_streams_, stream_id,
                                      ExpectedPlayingCheckMode::STOPPED))
    {
      case StreamExpected::OURS_AS_EXPECTED:
        break;

      case StreamExpected::OURS_QUEUED:
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

    auto crawler_lock(crawler_->lock());
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

        if(crawler_->retrieve_item_information(std::bind(&Player::Control::found_item_information,
                                                         this,
                                                         std::placeholders::_1,
                                                         std::placeholders::_2,
                                                         CrawlerContext::IMMEDIATE_PLAY)))
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

    switch(crawler_->find_next(std::bind(&Player::Control::found_list_item,
                                         this,
                                         std::placeholders::_1,
                                         std::placeholders::_2,
                                         CrawlerContext::IMMEDIATE_PLAY)))
    {
      case Playlist::CrawlerIface::FindNextFnResult::SEARCHING:
        return StopReaction::QUEUED;

      case Playlist::CrawlerIface::FindNextFnResult::STOPPED:
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
 * \param[out] queued_url
 *     Which URL was chosen for this stream.
 *
 * \returns
 *     True in case of success, false otherwise.
 */
static bool send_selected_file_uri_to_streamplayer(ID::OurStream stream_id,
                                                   const GVariantWrapper &stream_key,
                                                   Player::Control::QueueMode queue_mode,
                                                   Player::Control::PlayNewMode play_new_mode,
                                                   const std::string &queued_url)
{
    if(queued_url.empty())
        return false;

    msg_info("Passing URI for stream %u to player: \"%s\"",
             stream_id.get().get_raw_id(), queued_url.c_str());

    gboolean fifo_overflow;
    gboolean is_playing;

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

    if(!tdbus_splay_urlfifo_call_push_sync(dbus_get_streamplayer_urlfifo_iface(),
                                           stream_id.get().get_raw_id(),
                                           queued_url.c_str(), GVariantWrapper::get(stream_key),
                                           0, "ms", 0, "ms", keep_first_n,
                                           &fifo_overflow, &is_playing,
                                           NULL, NULL))
    {
        msg_error(0, LOG_NOTICE, "Failed queuing URI to streamplayer");
        return false;
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
        if(!send_play_command())
        {
            msg_error(0, LOG_NOTICE, "Failed sending start playback message");
            return false;
        }

        break;

      case Player::Control::PlayNewMode::SEND_PAUSE_COMMAND_IF_IDLE:
        if(!send_pause_command())
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
                       const Player::StreamPreplayInfo::ResolvedRedirectCallback &callback)
{
    const GVariantWrapper *stream_key;
    const std::string *uri = nullptr;
    const auto result(player.get_first_stream_uri(stream_id, stream_key, uri, callback));

    if(uri == nullptr)
        return result;

    if(!send_selected_file_uri_to_streamplayer(stream_id, *stream_key,
                                               queue_mode, play_new_mode,
                                               *uri))
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
            if(send_play_command())
                return StopReaction::TAKE_NEXT;

            break;

          case PlayNewMode::SEND_PAUSE_COMMAND_IF_IDLE:
            /* pause next in queue */
            if(send_pause_command())
                return StopReaction::TAKE_NEXT;

            break;
        }
    }

    if(!skipping)
        crawler_->set_direction_forward();

    switch(crawler_->find_next(std::bind(&Player::Control::found_list_item,
                                         this,
                                         std::placeholders::_1,
                                         std::placeholders::_2,
                                         CrawlerContext::IMMEDIATE_PLAY)))
    {
      case Playlist::CrawlerIface::FindNextFnResult::SEARCHING:
        return StopReaction::QUEUED;

      case Playlist::CrawlerIface::FindNextFnResult::STOPPED:
      case Playlist::CrawlerIface::FindNextFnResult::FAILED:
        break;
    }

    return StopReaction::STOPPED;
}

void Player::Control::pause_notification(ID::Stream stream_id)
{
    retry_data_.playing(stream_id);

    if(is_active_controller())
        enforce_intention(player_->get_intention(), Player::StreamState::PAUSED);
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

    switch(crawler_->find_next(std::bind(&Player::Control::found_list_item,
                                         this,
                                         std::placeholders::_1,
                                         std::placeholders::_2,
                                         CrawlerContext::PREFETCH)))
    {
      case Playlist::CrawlerIface::FindNextFnResult::SEARCHING:
        break;

      case Playlist::CrawlerIface::FindNextFnResult::STOPPED:
      case Playlist::CrawlerIface::FindNextFnResult::FAILED:
        prefetch_state_ = PrefetchState::NOT_PREFETCHING;
        break;
    }
}

void Player::Control::found_list_item(Playlist::CrawlerIface &crawler,
                                      Playlist::CrawlerIface::FindNextItemResult result,
                                      CrawlerContext ctx)
{
    auto locks(lock());

    if(player_ == nullptr)
    {
        BUG("Found list item, but not attached to player anymore");
        return;
    }

    bool need_item_information_now = false;

    switch(result)
    {
      case Playlist::CrawlerIface::FindNextItemResult::FOUND:
        if(prefetch_state_ == PrefetchState::PREFETCHING_NEXT_LIST_ITEM_AND_PLAY_IT)
            need_item_information_now = true;

        prefetch_state_ = PrefetchState::HAVE_NEXT_LIST_ITEM;

        switch(ctx)
        {
          case CrawlerContext::SKIP:
            prefetch_state_ = PrefetchState::NOT_PREFETCHING;

            switch(skip_requests_.skipped(*player_, *crawler_,
                                          Player::Skipper::StopSkipBehavior::KEEP_SKIPPING_IN_CURRENT_DIRECTION))
            {
              case Player::Skipper::SkippedResult::SKIPPING_FORWARD:
              case Player::Skipper::SkippedResult::SKIPPING_BACKWARD:
                switch(crawler.find_next(std::bind(&Player::Control::found_list_item,
                                                   this,
                                                   std::placeholders::_1,
                                                   std::placeholders::_2,
                                                   ctx)))
                {
                  case Playlist::CrawlerIface::FindNextFnResult::SEARCHING:
                    break;

                  case Playlist::CrawlerIface::FindNextFnResult::STOPPED:
                  case Playlist::CrawlerIface::FindNextFnResult::FAILED:
                    skip_requests_.stop_skipping(*player_, *crawler_);
                    break;
                }

                return;

              case Player::Skipper::SkippedResult::DONE_FORWARD:
              case Player::Skipper::SkippedResult::DONE_BACKWARD:
                break;
            }

            /* fall-through */

          case CrawlerContext::IMMEDIATE_PLAY:
            crawler.mark_current_position();
            need_item_information_now = true;
            break;

          case CrawlerContext::PREFETCH:
            if(!permissions_->can_prefetch_for_gapless())
            {
                /* gapless playback is actually not supported in this list,
                 * retrieval of item information must be deferred to a later
                 * point if and when needed */
                return;
            }

            need_item_information_now = true;
            break;
        }

        break;

      case Playlist::CrawlerIface::FindNextItemResult::CANCELED:
        break;

      case Playlist::CrawlerIface::FindNextItemResult::FAILED:
      case Playlist::CrawlerIface::FindNextItemResult::START_OF_LIST:
      case Playlist::CrawlerIface::FindNextItemResult::END_OF_LIST:
        prefetch_state_ = PrefetchState::NOT_PREFETCHING;
        skip_requests_.stop_skipping(*player_, *crawler_);
        break;
    }

    if(need_item_information_now)
    {
        prefetch_state_ = PrefetchState::PREFETCHING_LIST_ITEM_INFORMATION;

        if(!crawler.retrieve_item_information(std::bind(&Player::Control::found_item_information,
                                                        this,
                                                        std::placeholders::_1,
                                                        std::placeholders::_2,
                                                        ctx)))
            prefetch_state_ = PrefetchState::NOT_PREFETCHING;
    }
}

void Player::Control::found_item_information(Playlist::CrawlerIface &crawler,
                                             Playlist::CrawlerIface::RetrieveItemInfoResult result,
                                             CrawlerContext ctx)
{
    auto locks(lock());

    if(player_ == nullptr)
    {
        BUG("Found item information, but not attached to player anymore");
        return;
    }

    bool prefetch_more = false;
    StreamPreplayInfo::OpResult queuing_result = StreamPreplayInfo::OpResult::SUCCEEDED;

    switch(result)
    {
      case Playlist::CrawlerIface::RetrieveItemInfoResult::FOUND:
        prefetch_state_ = PrefetchState::HAVE_LIST_ITEM_INFORMATION;

        switch(ctx)
        {
          case CrawlerContext::IMMEDIATE_PLAY:
            switch(player_->get_intention())
            {
              case UserIntention::NOTHING:
              case UserIntention::STOPPING:
                break;

              case UserIntention::SKIPPING_PAUSED:
                skip_requests_.stop_skipping(*player_, *crawler_);

                /* fall-through */

              case UserIntention::PAUSING:
                queuing_result =
                    process_crawler_item(ctx, QueueMode::REPLACE_ALL,
                                         PlayNewMode::SEND_PAUSE_COMMAND_IF_IDLE);
                prefetch_state_ = PrefetchState::NOT_PREFETCHING;
                prefetch_more = true;
                break;

              case UserIntention::SKIPPING_LIVE:
                skip_requests_.stop_skipping(*player_, *crawler_);

                /* fall-through */

              case UserIntention::LISTENING:
                queuing_result =
                    process_crawler_item(ctx, QueueMode::REPLACE_ALL,
                                         PlayNewMode::SEND_PLAY_COMMAND_IF_IDLE);
                prefetch_state_ = PrefetchState::NOT_PREFETCHING;

                if(queuing_result == StreamPreplayInfo::OpResult::SUCCEEDED)
                    send_play_command();

                break;
            }

            break;

          case CrawlerContext::PREFETCH:
            switch(player_->get_intention())
            {
              case UserIntention::NOTHING:
              case UserIntention::STOPPING:
                break;

              case UserIntention::SKIPPING_PAUSED:
              case UserIntention::SKIPPING_LIVE:
                skip_requests_.stop_skipping(*player_, *crawler_);

                /* fall-through */

              case UserIntention::PAUSING:
              case UserIntention::LISTENING:
                queuing_result =
                    process_crawler_item(ctx, QueueMode::APPEND, PlayNewMode::KEEP);
                prefetch_state_ = PrefetchState::NOT_PREFETCHING;
                prefetch_more = true;
                break;
            }

            break;

          case CrawlerContext::SKIP:
            {
                auto play_new_mode(PlayNewMode::KEEP);

                switch(player_->get_intention())
                {
                  case UserIntention::NOTHING:
                  case UserIntention::STOPPING:
                  case UserIntention::PAUSING:
                  case UserIntention::LISTENING:
                    break;

                  case UserIntention::SKIPPING_PAUSED:
                    play_new_mode = PlayNewMode::SEND_PAUSE_COMMAND_IF_IDLE;
                    break;

                  case UserIntention::SKIPPING_LIVE:
                    play_new_mode = PlayNewMode::SEND_PLAY_COMMAND_IF_IDLE;
                    break;
                }

                if(play_new_mode != PlayNewMode::KEEP)
                {
                    skip_requests_.stop_skipping(*player_, *crawler_);
                    queuing_result =
                        process_crawler_item(ctx, QueueMode::REPLACE_ALL, play_new_mode);
                    prefetch_state_ = PrefetchState::NOT_PREFETCHING;
                    prefetch_more = true;
                }
            }

            break;
        }

        if(queuing_result != StreamPreplayInfo::OpResult::SUCCEEDED)
            prefetch_more = false;

        break;

      case Playlist::CrawlerIface::RetrieveItemInfoResult::FAILED:
        /* skip this one, maybe the next one will work */
        prefetch_state_ = PrefetchState::NOT_PREFETCHING;

        switch(ctx)
        {
          case CrawlerContext::IMMEDIATE_PLAY:
          case CrawlerContext::PREFETCH:
            crawler.set_direction_forward();

            /* fall-through */

          case CrawlerContext::SKIP:
            crawler.find_next(std::bind(&Player::Control::found_list_item,
                                        this,
                                        std::placeholders::_1,
                                        std::placeholders::_2,
                                        ctx));

            break;
        }

        break;

      case Playlist::CrawlerIface::RetrieveItemInfoResult::CANCELED:
        prefetch_state_ = PrefetchState::NOT_PREFETCHING;
        break;
    }

    if(prefetch_more)
    {
        crawler.set_direction_forward();
        need_next_item_hint(false);
    }
}

void Player::Control::resolved_redirect_for_found_item(size_t idx,
                                                       StreamPreplayInfo::ResolvedRedirectResult result,
                                                       CrawlerContext ctx,
                                                       ID::OurStream for_stream,
                                                       QueueMode queue_mode,
                                                       PlayNewMode play_new_mode)
{
    auto locks(lock());

    if(player_ == nullptr || crawler_ == nullptr)
    {
        BUG("Resolved redirect, but not attached to player and/or crawler anymore");
        return;
    }

    switch(result)
    {
      case StreamPreplayInfo::ResolvedRedirectResult::FOUND:
        break;

      case StreamPreplayInfo::ResolvedRedirectResult::FAILED:
        BUG("Resolving link at %zu canceled, but case not handled", idx);
        return;

      case StreamPreplayInfo::ResolvedRedirectResult::CANCELED:
        BUG("Resolving link at %zu failed, but case not handled", idx);
        return;
    }

    bool prefetch_more = false;
    StreamPreplayInfo::OpResult queuing_result = StreamPreplayInfo::OpResult::SUCCEEDED;

    switch(ctx)
    {
      case CrawlerContext::IMMEDIATE_PLAY:
        switch(player_->get_intention())
        {
          case UserIntention::NOTHING:
          case UserIntention::STOPPING:
            break;

          case UserIntention::SKIPPING_PAUSED:
          case UserIntention::PAUSING:
            queuing_result =
                process_crawler_item_tail(ctx, for_stream,
                                          queue_mode, play_new_mode,
                                          std::bind(&Player::Control::unexpected_resolve_error, this,
                                                    std::placeholders::_1, std::placeholders::_2));
            prefetch_more = true;
            break;

          case UserIntention::SKIPPING_LIVE:
          case UserIntention::LISTENING:
            queuing_result =
                process_crawler_item_tail(ctx, for_stream,
                                          queue_mode, play_new_mode,
                                          std::bind(&Player::Control::unexpected_resolve_error, this,
                                                    std::placeholders::_1, std::placeholders::_2));

            if(queuing_result == StreamPreplayInfo::OpResult::SUCCEEDED)
                send_play_command();

            break;
        }

        break;

      case CrawlerContext::PREFETCH:
        switch(player_->get_intention())
        {
          case UserIntention::NOTHING:
          case UserIntention::STOPPING:
            break;

          case UserIntention::SKIPPING_PAUSED:
          case UserIntention::SKIPPING_LIVE:
          case UserIntention::PAUSING:
          case UserIntention::LISTENING:
            queuing_result =
                process_crawler_item_tail(ctx, for_stream,
                                          queue_mode, play_new_mode,
                                          std::bind(&Player::Control::unexpected_resolve_error, this,
                                                    std::placeholders::_1, std::placeholders::_2));
            prefetch_more = true;
            break;
        }

        break;

      case CrawlerContext::SKIP:
        if(play_new_mode != PlayNewMode::KEEP)
        {
            queuing_result =
                process_crawler_item_tail(ctx, for_stream,
                                          queue_mode, play_new_mode,
                                          std::bind(&Player::Control::unexpected_resolve_error, this,
                                                    std::placeholders::_1, std::placeholders::_2));
            prefetch_more = true;
        }

        break;
    }

    if(queuing_result != StreamPreplayInfo::OpResult::SUCCEEDED)
        prefetch_more = false;

    if(prefetch_more)
    {
        crawler_->set_direction_forward();
        need_next_item_hint(false);
    }
}

void Player::Control::unexpected_resolve_error(size_t idx,
                                               Player::StreamPreplayInfo::ResolvedRedirectResult result)
{
    BUG("Asynchronous resolution of Airable redirect failed unexpectedly "
        "for URL at index %zu, result %u", idx, static_cast<unsigned int>(result));
    unplug();
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
Player::Control::process_crawler_item(CrawlerContext ctx, QueueMode queue_mode,
                                      PlayNewMode play_new_mode)
{
    if(player_ == nullptr || crawler_ == nullptr)
        return StreamPreplayInfo::OpResult::CANCELED;

    auto *crawler = dynamic_cast<Playlist::DirectoryCrawler *>(crawler_);
    log_assert(crawler != nullptr);

    auto &item_info(crawler->get_current_list_item_info_non_const());
    log_assert(item_info.position_.list_id_.is_valid());
    log_assert(item_info.is_item_info_valid_);

    item_info.airable_links_.finalize(bitrate_limiter_);

    /* we'll steal some data from the item info for efficiency */
    const ID::OurStream stream_id(player_->store_stream_preplay_information(
                                        std::move(item_info.stream_key_),
                                        std::move(item_info.stream_uris_),
                                        std::move(item_info.airable_links_),
                                        item_info.position_.list_id_, item_info.position_.line_,
                                        item_info.position_.directory_depth_));

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

    return process_crawler_item_tail(ctx, stream_id, queue_mode, play_new_mode,
                                     std::bind(&Player::Control::resolved_redirect_for_found_item,
                                               this,
                                               std::placeholders::_1, std::placeholders::_2,
                                               ctx, stream_id, queue_mode, play_new_mode));
}

Player::StreamPreplayInfo::OpResult
Player::Control::process_crawler_item_tail(CrawlerContext ctx, ID::OurStream stream_id,
                                           QueueMode queue_mode,
                                           PlayNewMode play_new_mode,
                                           const StreamPreplayInfo::ResolvedRedirectCallback &callback)
{
    const auto result(queue_stream_or_forget(*player_, stream_id, queue_mode,
                                             play_new_mode, callback));

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
                                           std::placeholders::_1, std::placeholders::_2)))
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
                                                std::placeholders::_1, std::placeholders::_2)))
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
