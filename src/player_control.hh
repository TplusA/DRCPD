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

#ifndef PLAYER_CONTROL_HH
#define PLAYER_CONTROL_HH

#include <deque>

#include "player_data.hh"
#include "player_permissions.hh"
#include "playlist_crawler.hh"
#include "logged_lock.hh"

namespace Player
{

class AudioSource;

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

    enum class SkippedResult
    {
        DONE_FORWARD,
        DONE_BACKWARD,
        SKIPPING_FORWARD,
        SKIPPING_BACKWARD,
    };

    enum class StopSkipBehavior
    {
        STOP,
        KEEP_SKIPPING_FORWARD,
        KEEP_SKIPPING_IN_CURRENT_DIRECTION,
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
    SkippedResult skipped(Data &data, Playlist::CrawlerIface &crawler,
                          StopSkipBehavior how);

    bool stop_skipping(Data &data, Playlist::CrawlerIface &crawler)
    {
        return stop_skipping(data, crawler, StopSkipBehavior::STOP);
    }

  private:
    static bool set_intention_for_skipping(Data &data);
    static void set_intention_from_skipping(Data &data);

    bool stop_skipping(Data &data, Playlist::CrawlerIface &crawler,
                       StopSkipBehavior how);
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

    enum class PrefetchState
    {
        NOT_PREFETCHING,
        PREFETCHING_NEXT_LIST_ITEM,
        PREFETCHING_NEXT_LIST_ITEM_AND_PLAY_IT,
        HAVE_NEXT_LIST_ITEM,
        PREFETCHING_LIST_ITEM_INFORMATION,
        HAVE_LIST_ITEM_INFORMATION,
    };

    enum class StopReaction
    {
        NOT_ATTACHED,
        STREAM_IGNORED,
        STOPPED,
        QUEUED,
        RETRIEVE_QUEUED,
        NOP,
        RETRY,
        REPLAY_QUEUE,
        TAKE_NEXT,
    };

    enum class PlayNewMode
    {
        KEEP,
        SEND_PLAY_COMMAND_IF_IDLE,
        SEND_PAUSE_COMMAND_IF_IDLE,
    };

    enum class QueueMode
    {
        APPEND,
        REPLACE_QUEUE,
        REPLACE_ALL,
    };

    enum class ReplayResult
    {
        OK,
        GAVE_UP,
        RETRY_FAILED_HARD,
        EMPTY_QUEUE,
    };

  private:
    LoggedLock::RecMutex lock_;

    AudioSource *audio_source_;
    Data *player_;
    LoggedLock::RecMutex player_dummy_lock_;
    Playlist::CrawlerIface *crawler_;
    LoggedLock::RecMutex crawler_dummy_lock_;
    const LocalPermissionsIface *permissions_;

    Skipper skip_requests_;

    /* streams queued in streamplayer, but not playing */
    std::deque<ID::OurStream> queued_streams_;
    PrefetchState prefetch_state_;

    const std::function<bool(uint32_t)> bitrate_limiter_;

    class Retry
    {
      private:
        static constexpr unsigned int MAX_RETRIES = 2;

        unsigned int count_;
        ID::OurStream stream_id_;

      public:
        explicit Retry():
            count_(0),
            stream_id_(ID::OurStream::make_invalid())
        {}

        void reset()
        {
            playing(ID::Stream::make_invalid());
        }

        void playing(ID::Stream stream_id)
        {
            count_ = 0;
            stream_id_ = ID::OurStream::make_from_generic_id(stream_id);
        }

        bool retry(ID::OurStream stream_id)
        {
            log_assert(stream_id.get().is_valid());

            if(stream_id_ != stream_id)
                playing(stream_id.get());

            if(count_ >= MAX_RETRIES)
                return false;

            ++count_;

            return true;
        }

        ID::OurStream get_stream_id() const { return stream_id_; }
    };

    Retry retry_data_;

    struct FastWindData
    {
        bool is_fast_winding_;
        bool is_forward_mode_;
        double speed_factor_;

        FastWindData(const FastWindData &) = delete;
        FastWindData &operator=(const FastWindData &) = delete;

        explicit FastWindData():
            is_fast_winding_(false),
            is_forward_mode_(true),
            speed_factor_(1.0L)
        {}
    };

    FastWindData fast_wind_data_;

  public:
    Control(const Control &) = delete;
    Control &operator=(const Control &) = delete;

    explicit Control(std::function<bool(uint32_t)> &&bitrate_limiter):
        audio_source_(nullptr),
        player_(nullptr),
        crawler_(nullptr),
        permissions_(nullptr),
        prefetch_state_(PrefetchState::NOT_PREFETCHING),
        bitrate_limiter_(bitrate_limiter)
    {
        LoggedLock::set_name(lock_, "Player::Control");
        LoggedLock::set_name(player_dummy_lock_, "Player::Data dummy");
        LoggedLock::set_name(crawler_dummy_lock_, "Player::Control dummy");
    }

    /*!
     * Lock this player control.
     *
     * Before calling \e any function member, the lock must be acquired using
     * this function.
     */
    std::tuple<LoggedLock::UniqueLock<LoggedLock::RecMutex>,
               LoggedLock::UniqueLock<LoggedLock::RecMutex>,
               LoggedLock::UniqueLock<LoggedLock::RecMutex>>
    lock() const
    {
        Control &ncthis(*const_cast<Control *>(this));

        return std::make_tuple(LoggedLock::UniqueLock<LoggedLock::RecMutex>(ncthis.lock_),
                               crawler_ != nullptr
                               ? crawler_->lock()
                               : LoggedLock::UniqueLock<LoggedLock::RecMutex>(ncthis.crawler_dummy_lock_),
                               player_ != nullptr
                               ? player_->lock()
                               : LoggedLock::UniqueLock<LoggedLock::RecMutex>(ncthis.player_dummy_lock_));
    }

    bool is_active_controller() const { return audio_source_ != nullptr; };
    bool is_active_controller_for_audio_source(const AudioSource &audio_source) const { return audio_source_ == &audio_source; }
    void plug(AudioSource &audio_source);
    void plug(Data &player_data);
    void plug(Playlist::CrawlerIface &crawler, const LocalPermissionsIface &permissions);
    void unplug();

    bool source_selected_notification(const std::string &audio_source_id);
    bool source_deselected_notification(const std::string &audio_source_id);

    void set_repeat_mode(RepeatMode repeat_mode);

    /* functions below are called as a result of user actions that are supposed
     * to take direct, immediate influence on playback, so they impose requests
     * to the system */
    void play_request();
    void stop_request();
    void pause_request();
    void jump_to_crawler_location();
    bool skip_forward_request();
    void skip_backward_request();
    void rewind_request();
    void fast_wind_set_speed_request(double speed_factor);
    void fast_wind_set_direction_request(bool is_forward);
    void fast_wind_start_request() const;
    void fast_wind_stop_request() const;

    /* functions below are called as a result of status updates from the
     * system, so they may be direct reactions to preceding user actions */
    void play_notification(ID::Stream stream_id, bool is_new_stream);
    StopReaction stop_notification(ID::Stream stream_id);
    StopReaction stop_notification(ID::Stream stream_id,
                                   const std::string &error_id,
                                   bool is_urlfifo_empty);
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
                         Playlist::CrawlerIface::FindNextItemResult result,
                         CrawlerContext ctx);
    void found_item_information(Playlist::CrawlerIface &crawler,
                                Playlist::CrawlerIface::RetrieveItemInfoResult result,
                                CrawlerContext ctx);
    void resolved_redirect_for_found_item(size_t idx,
                                          StreamPreplayInfo::ResolvedRedirectResult result,
                                          CrawlerContext ctx, ID::OurStream for_stream,
                                          QueueMode queue_mode, PlayNewMode play_new_mode);
    void unexpected_resolve_error(size_t idx,
                                  Player::StreamPreplayInfo::ResolvedRedirectResult result);;

    StreamPreplayInfo::OpResult
    process_crawler_item(CrawlerContext ctx, QueueMode queue_mode, PlayNewMode play_new_mode);
    StreamPreplayInfo::OpResult
    process_crawler_item_tail(CrawlerContext ctx, ID::OurStream stream_id,
                              QueueMode queue_mode, PlayNewMode play_new_mode,
                              const StreamPreplayInfo::ResolvedRedirectCallback &callback);

    ReplayResult replay(ID::OurStream stream_id, bool is_retry,
                        PlayNewMode mode, bool &tool_from_queue);
    void forget_queued_and_playing(bool also_forget_playing);
};

}

#endif /* !PLAYER_CONTROL_HH */
