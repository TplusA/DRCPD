/*
 * Copyright (C) 2016  T+A elektroakustik GmbH & Co. KG
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
#include "directory_crawler.hh"
#include "dbus_iface_deep.h"
#include "view_play.hh"
#include "messages.h"

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

void Player::Skipper::stop_skipping(Player::Data &data, Playlist::CrawlerIface &crawler,
                                    bool keep_skipping_flag)
{
    if(keep_skipping_flag)
    {
        log_assert(is_skipping_);
        pending_skip_requests_ = 0;
    }
    else
    {
        reset();
        set_intention_from_skipping(data);
    }

    crawler.set_direction_forward();
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

bool Player::Skipper::skipped(Data &data, Playlist::CrawlerIface &crawler,
                              bool keep_skipping_flag_if_done)
{
    if(pending_skip_requests_ == 0)
    {
        if(is_skipping_)
            crawler.mark_current_position();
        else
        {
            BUG("Got skipped notification, but not skipping");
            keep_skipping_flag_if_done = false;
        }

        stop_skipping(data, crawler, keep_skipping_flag_if_done);

        return false;
    }

    log_assert(is_skipping_);

    crawler.mark_current_position();

    if(pending_skip_requests_ > 0)
    {
        --pending_skip_requests_;
        crawler.set_direction_forward();
    }
    else
    {
        ++pending_skip_requests_;
        crawler.set_direction_backward();
    }

    return true;
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
    next_stream_in_queue_ = ID::OurStream::make_invalid();

    auto crawler_lock(crawler_->lock());
    crawler_->attached_to_player_notification();
}

void Player::Control::unplug()
{
    owning_view_ = nullptr;

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

static bool send_skip_to_next_command(ID::Stream &next_stream_in_queue,
                                      bool &is_playing)
{
    guint next_id;
    gboolean is_playing_flag;

    if(!tdbus_splay_urlfifo_call_next_sync(dbus_get_streamplayer_urlfifo_iface(),
                                           &next_id, &is_playing_flag,
                                           NULL, NULL))
    {
        msg_error(0, LOG_NOTICE, "Failed sending skip track message");
        return false;
    }

    next_stream_in_queue = (next_id != UINT32_MAX
                            ? ID::Stream::make_from_raw_id(next_id)
                            : ID::Stream::make_invalid());
    is_playing = is_playing_flag;

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
        resume_paused_stream(player_);
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

    UserIntention previous_intention;
    switch(skip_requests_.forward_request(*player_, *crawler_, previous_intention))
    {
      case Skipper::REJECTED:
      case Skipper::BACK_TO_NORMAL:
      case Skipper::SKIPPING:
        break;

      case Skipper::FIRST_SKIP_REQUEST:
        if(next_stream_in_queue_.get().is_valid())
        {
            auto next_stream_id(ID::Stream::make_invalid());
            bool is_playing;

            if(!send_skip_to_next_command(next_stream_id, is_playing))
            {
                player_->set_intention(previous_intention);
                break;
            }

            if(!next_stream_in_queue_.compatible_with(next_stream_id))
                BUG("Stream in streamplayer queue is not ours");
            else if(next_stream_in_queue_.get() != next_stream_id)
                BUG("Next stream ID should be %u, but streamplayer says it's %u",
                    next_stream_in_queue_.get().get_raw_id(),
                    next_stream_id.get_raw_id());

            next_stream_in_queue_ = ID::OurStream::make_invalid();
            skip_requests_.skipped(*player_, *crawler_, false);

            /* FIXME: The "known" player state is probably too inaccurate here */
            enforce_intention(player_->get_intention(),
                              is_playing ? Player::StreamState::PLAYING : Player::StreamState::STOPPED);

            return true;
        }
        else if(!crawler_->find_next(std::bind(&Player::Control::found_list_item,
                                               this,
                                               std::placeholders::_1,
                                               std::placeholders::_2,
                                               CrawlerContext::SKIP)))
            player_->set_intention(previous_intention);

        break;
    }

    return false;
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
        if(!crawler_->find_next(std::bind(&Player::Control::found_list_item,
                                          this,
                                          std::placeholders::_1,
                                          std::placeholders::_2,
                                          CrawlerContext::SKIP)))
            player_->set_intention(previous_intention);

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


void Player::Control::play_notification(ID::Stream stream_id)
{
    if(stream_id == next_stream_in_queue_.get())
        next_stream_in_queue_ = ID::OurStream::make_invalid();

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

bool Player::Control::stop_notification(ID::Stream stream_id)
{
    if(player_ == nullptr || crawler_ == nullptr)
        return true;

    auto crawler_lock(crawler_->lock());

    switch(player_->get_intention())
    {
      case UserIntention::NOTHING:
      case UserIntention::STOPPING:
        crawler_->configure_and_restart(crawler_->get_recursive_mode(),
                                        crawler_->get_shuffle_mode());
        return true;

      case UserIntention::PAUSING:
      case UserIntention::LISTENING:
        crawler_->set_direction_forward();

        /* fall-through */

      case UserIntention::SKIPPING_PAUSED:
      case UserIntention::SKIPPING_LIVE:
        crawler_->find_next(std::bind(&Player::Control::found_list_item,
                                      this,
                                      std::placeholders::_1,
                                      std::placeholders::_2,
                                      CrawlerContext::IMMEDIATE_PLAY));
        break;
    }

    return false;
}

void Player::Control::pause_notification(ID::Stream stream_id)
{
    if(is_active_controller())
        enforce_intention(player_->get_intention(), Player::StreamState::PAUSED);
}


void Player::Control::need_next_item_hint(bool queue_is_full)
{
    if(queue_is_full)
        BUG("Streamplayer reports full queue");

    if(crawler_ == nullptr)
        return;

    auto crawler_lock(crawler_->lock());

    if(!permissions_->can_prefetch_for_gapless())
        return;

    if(is_prefetching_)
        return;

    if(next_stream_in_queue_.get().is_valid())
       return;

    is_prefetching_ = true;

    if(!crawler_->find_next(std::bind(&Player::Control::found_list_item,
                                      this,
                                      std::placeholders::_1,
                                      std::placeholders::_2,
                                      CrawlerContext::PREFETCH)))
    {
        is_prefetching_ = false;
    }
}

void Player::Control::found_list_item(Playlist::CrawlerIface &crawler,
                                      Playlist::CrawlerIface::FindNext result,
                                      CrawlerContext ctx)
{
    auto locks(lock());

    switch(result)
    {
      case Playlist::CrawlerIface::FindNext::FOUND:
        switch(ctx)
        {
          case CrawlerContext::SKIP:
            if(skip_requests_.skipped(*player_, *crawler_, true))
            {
                if(!crawler.find_next(std::bind(&Player::Control::found_list_item,
                                                this,
                                                std::placeholders::_1,
                                                std::placeholders::_2,
                                                ctx)))
                    skip_requests_.stop_skipping(*player_, *crawler_);

                return;
            }

            /* fall-through */

          case CrawlerContext::IMMEDIATE_PLAY:
            crawler.mark_current_position();

            /* fall-through */

          case CrawlerContext::PREFETCH:
            crawler.retrieve_item_information(std::bind(&Player::Control::found_item_information,
                                                        this,
                                                        std::placeholders::_1,
                                                        std::placeholders::_2,
                                                        ctx));
            break;
        }

        break;

      case Playlist::CrawlerIface::FindNext::FAILED:
      case Playlist::CrawlerIface::FindNext::CANCELED:
      case Playlist::CrawlerIface::FindNext::START_OF_LIST:
      case Playlist::CrawlerIface::FindNext::END_OF_LIST:
        if(ctx == CrawlerContext::PREFETCH)
            is_prefetching_ = false;

        skip_requests_.stop_skipping(*player_, *crawler_);

        break;
    }
}

void Player::Control::found_item_information(Playlist::CrawlerIface &crawler,
                                             Playlist::CrawlerIface::RetrieveItemInfo result,
                                             CrawlerContext ctx)
{
    auto locks(lock());

    bool prefetch_more = false;
    bool queuing_failed = false;

    if(ctx == CrawlerContext::PREFETCH)
        is_prefetching_ = false;

    switch(result)
    {
      case Playlist::CrawlerIface::RetrieveItemInfo::FOUND:
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
                queuing_failed = !store_current_item_info_and_play(true);
                prefetch_more = true;
                break;

              case UserIntention::SKIPPING_LIVE:
                skip_requests_.stop_skipping(*player_, *crawler_);

                /* fall-through */

              case UserIntention::LISTENING:
                queuing_failed = !store_current_item_info_and_play(true);
                prefetch_more = true;
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
                queuing_failed = !store_current_item_info_and_play(false);
                break;
            }

            break;

          case CrawlerContext::SKIP:
            switch(player_->get_intention())
            {
              case UserIntention::NOTHING:
              case UserIntention::STOPPING:
              case UserIntention::PAUSING:
              case UserIntention::LISTENING:
                break;

              case UserIntention::SKIPPING_PAUSED:
              case UserIntention::SKIPPING_LIVE:
                skip_requests_.stop_skipping(*player_, *crawler_);
                queuing_failed = !store_current_item_info_and_play(true);
                prefetch_more = true;
                break;
            }

            break;
        }

        if(queuing_failed)
        {
            result = Playlist::CrawlerIface::RetrieveItemInfo::FAILED;

            /* fall-through */
        }
        else
            break;

      case Playlist::CrawlerIface::RetrieveItemInfo::FAILED:
        /* skip this one, maybe the next one will work */
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

      case Playlist::CrawlerIface::RetrieveItemInfo::CANCELED:
        break;
    }

    if(prefetch_more)
    {
        crawler.set_direction_forward();
        need_next_item_hint(false);
    }
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
 *     True in case of success, false otherwise.
 */
static bool send_selected_file_uri_to_streamplayer(ID::OurStream stream_id,
                                                   bool play_immediately,
                                                   const std::string &queued_url)
{
    if(queued_url.empty())
        return false;

    msg_info("Passing URI to player: \"%s\"", queued_url.c_str());

    gboolean fifo_overflow;
    gboolean is_playing;

    if(!tdbus_splay_urlfifo_call_push_sync(dbus_get_streamplayer_urlfifo_iface(),
                                           stream_id.get().get_raw_id(),
                                           queued_url.c_str(),
                                           0, "ms", 0, "ms",
                                           play_immediately ? -2 : -1,
                                           &fifo_overflow, &is_playing,
                                           NULL, NULL))
    {
        msg_error(0, LOG_NOTICE, "Failed queuing URI to streamplayer");
        return false;
    }

    if(fifo_overflow)
    {
        msg_error(0, LOG_INFO, "URL FIFO overflow");
        return false;
    }

    if(!is_playing && !send_play_command())
    {
        msg_error(0, LOG_NOTICE, "Failed sending start playback message");
        return false;
    }

    return true;
}

static bool queue_stream_or_forget(Player::Data &player,
                                   ID::OurStream stream_id, bool play_immediately)
{
    if(!send_selected_file_uri_to_streamplayer(stream_id, play_immediately,
                                               player.get_first_stream_uri(stream_id)))
    {
        player.forget_stream(stream_id.get());
        return false;
    }

    return true;
}

static MetaData::Set mk_meta_data_from_preloaded_information(const ViewFileBrowser::FileItem &file_item)
{
    const auto &preloaded(file_item.get_preloaded_meta_data());
    MetaData::Set meta_data;

    meta_data.add(MetaData::Set::ARTIST, preloaded.artist_.c_str(), ViewPlay::meta_data_reformatters);
    meta_data.add(MetaData::Set::ALBUM,  preloaded.album_.c_str(),  ViewPlay::meta_data_reformatters);
    meta_data.add(MetaData::Set::TITLE,  preloaded.title_.c_str(),  ViewPlay::meta_data_reformatters);
    meta_data.add(MetaData::Set::INTERNAL_DRCPD_TITLE, file_item.get_text(),
                  ViewPlay::meta_data_reformatters);

    return meta_data;
}

bool Player::Control::store_current_item_info_and_play(bool play_immediately)
{
    if(player_ == nullptr || crawler_ == nullptr)
        return false;

    auto crawler_lock(crawler_->lock());

    auto *crawler = dynamic_cast<Playlist::DirectoryCrawler *>(crawler_);
    log_assert(crawler != nullptr);

    auto &item_info(crawler->get_current_list_item_info_non_const());
    log_assert(item_info.position_.list_id_.is_valid());
    log_assert(item_info.file_item_ != nullptr);

    /* we'll steal the URI list from the item info for efficiency */
    const ID::OurStream stream_id(player_->store_stream_preplay_information(
                                        std::move(item_info.stream_uris_),
                                        item_info.position_.list_id_, item_info.position_.line_,
                                        item_info.position_.directory_depth_));

    if(!stream_id.get().is_valid())
        return false;

    player_->put_meta_data(stream_id.get(),
                           std::move(mk_meta_data_from_preloaded_information(*item_info.file_item_)));

    if(!play_immediately && next_stream_in_queue_.get().is_valid())
        BUG("Losing information about our next stream ID %u",
            next_stream_in_queue_.get().get_raw_id());

    next_stream_in_queue_ = ID::OurStream::make_invalid();

    if(!queue_stream_or_forget(*player_, stream_id, play_immediately))
        return false;

    if(!play_immediately)
        next_stream_in_queue_ = stream_id;

    return true;
}
