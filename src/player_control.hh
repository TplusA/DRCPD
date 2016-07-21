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

#ifndef PLAYER_CONTROL_HH
#define PLAYER_CONTROL_HH

#include "player_data.hh"
#include "player_permissions.hh"
#include "playlist_crawler.hh"
#include "view.hh"
#include "logged_lock.hh"

namespace Player
{

/*!
 * Keep track of fast skip requests and user intention.
 */
class Skipper
{
  public:
    enum SkipState
    {
        REJECTED,
        FIRST_SKIP_REQUEST,
        SKIPPING,
        BACK_TO_NORMAL,
    };

  private:
    static constexpr const char MAX_PENDING_SKIP_REQUESTS = 5;

    bool is_skipping_;

    /*!
     * Cumulated effect of fast skip requests.
     */
    char pending_skip_requests_;

  public:
    Skipper(const Skipper &) = delete;
    Skipper &operator=(const Skipper &) = delete;

    constexpr explicit Skipper():
        is_skipping_(false),
        pending_skip_requests_(0)
    {}

    void reset()
    {
        is_skipping_ = false;
        pending_skip_requests_ = 0;
    }

    SkipState forward_request(Data &data, Playlist::CrawlerIface &crawler,
                              UserIntention &previous_intention);
    SkipState backward_request(Data &data, Playlist::CrawlerIface &crawler,
                               UserIntention &previous_intention);
    bool skipped(Data &data, Playlist::CrawlerIface &crawler);
    void stop_skipping(Data &data, Playlist::CrawlerIface &crawler);

  private:
    static bool set_intention_for_skipping(Data &data);
    static void set_intention_from_skipping(Data &data);
};

class Control
{
  public:
    enum class RepeatMode
    {
        NONE,
        SINGLE,
        ALL,
    };

  private:
    LoggedLock::RecMutex lock_;

    const ViewIface *owning_view_;
    Data *player_;
    Playlist::CrawlerIface *crawler_;
    LoggedLock::RecMutex crawler_dummy_lock_;
    const LocalPermissionsIface *permissions_;

    Skipper skip_requests_;

    ID::OurStream next_stream_in_queue_;
    bool is_prefetching_;

  public:
    Control(const Control &) = delete;
    Control &operator=(const Control &) = delete;

    explicit Control():
        owning_view_(nullptr),
        player_(nullptr),
        crawler_(nullptr),
        permissions_(nullptr),
        next_stream_in_queue_(ID::OurStream::make_invalid()),
        is_prefetching_(false)
    {
        LoggedLock::set_name(lock_, "Player::Control");
        LoggedLock::set_name(crawler_dummy_lock_, "Player::Control dummy");
    }

    /*!
     * Lock this player control.
     *
     * Before calling \e any function member, the lock must be acquired using
     * this function.
     */
    std::pair<LoggedLock::UniqueLock<LoggedLock::RecMutex>,
              LoggedLock::UniqueLock<LoggedLock::RecMutex>>
    lock() const
    {
        if(crawler_ != nullptr)
        {
            auto crawler_lock(crawler_->lock());
            return std::make_pair(LoggedLock::UniqueLock<LoggedLock::RecMutex>(const_cast<Control *>(this)->lock_),
                                  std::move(crawler_lock));
        }
        else
        {
            auto crawler_lock(LoggedLock::UniqueLock<LoggedLock::RecMutex>(const_cast<Control *>(this)->crawler_dummy_lock_));
            return std::make_pair(LoggedLock::UniqueLock<LoggedLock::RecMutex>(const_cast<Control *>(this)->lock_),
                                  std::move(crawler_lock));
        }

    }

    bool is_active_controller() const { return owning_view_ != nullptr; };
    bool is_active_controller_for_view(const ViewIface &view) const { return owning_view_ == &view; }
    void plug(const ViewIface &view);
    void plug(Data &player_data);
    void plug(Playlist::CrawlerIface &crawler, const LocalPermissionsIface &permissions);
    void unplug();

    void set_repeat_mode(RepeatMode repeat_mode);

    /* functions below are called as a result of user actions that are supposed
     * to take direct, immediate influence on playback, so they impose requests
     * to the system */
    void play_request();
    void stop_request();
    void pause_request();
    void skip_forward_request();
    void skip_backward_request();
    void rewind_request();
    void fast_wind_set_speed_request(double speed_factor);
    void fast_wind_set_direction_request(bool is_forward);
    void fast_wind_start_request();
    void fast_wind_stop_request();

    /* functions below are called as a result of status updates from the
     * system, so they may be direct reactions to preceding user actions */
    void play_notification(ID::Stream stream_id);
    bool stop_notification(ID::Stream stream_id);
    void pause_notification(ID::Stream stream_id);
    void need_next_item_hint(bool queue_is_full);

  private:
    enum class CrawlerContext
    {
        IMMEDIATE_PLAY,
        PREFETCH,
        SKIP,
    };

    void found_list_item(Playlist::CrawlerIface &crawler,
                         Playlist::CrawlerIface::FindNext result,
                         CrawlerContext ctx);
    void found_item_information(Playlist::CrawlerIface &crawler,
                                Playlist::CrawlerIface::RetrieveItemInfo result,
                                CrawlerContext ctx);

    bool store_current_item_info_and_play(bool play_immediately);
};

}

#endif /* !PLAYER_CONTROL_HH */
