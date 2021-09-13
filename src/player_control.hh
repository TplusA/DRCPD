/*
 * Copyright (C) 2016--2021  T+A elektroakustik GmbH & Co. KG
 *
 * This file is part of DRCPD.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301, USA.
 */

#ifndef PLAYER_CONTROL_HH
#define PLAYER_CONTROL_HH

#include "player_control_skipper.hh"
#include "player_permissions.hh"

namespace Player
{

class AudioSource;

class Control
{
  public:
    enum class StopReaction
    {
        /*! Player data or crawler not attached to player control, ignore */
        NOT_ATTACHED,

        /*! Unexpected stream, notification is to be ignored */
        STREAM_IGNORED,

        /*! Streamplayer has stopped, and we should keep it this way */
        STOPPED,

        /*! Streamplayer has stopped, its queue is empty, but more streams are
         * currently being fetched by the crawler or have already been
         * submitted to Streamplayer */
        QUEUED,

        /*! Streamplayer failed, stream has been restarted */
        RETRY,

        /*! Told Streamplayer to play the next queue from its queue, and we
         * need to find the next entry in our list */
        TAKE_NEXT,
    };

    enum class PlayNewMode
    {
        KEEP,
        SEND_PLAY_COMMAND_IF_IDLE,
        SEND_PAUSE_COMMAND_IF_IDLE,
    };

    enum class InsertMode
    {
        APPEND,
        REPLACE_QUEUE,
        REPLACE_ALL,
    };

    enum class FinishedWith
    {
        PREFETCHING,
        PLAYING,
    };

  private:
    LoggedLock::RecMutex lock_;

    AudioSource *audio_source_;
    bool with_enforced_intentions_;

    /*!
     * Where to start playing when the audio source has been selected.
     *
     * This is just a temporary space to hold the find operation for the first
     * entry to play. It is set when a play request is being made while audio
     * source selection hasn't finished yet. In this case, the play request is
     * automatically repeated when the audio source selection is done, using
     * the operation stored here (moving it to a different place).
     */
    std::shared_ptr<Playlist::Crawler::FindNextOpBase> audio_source_selected_find_op_;

    Data *player_data_;
    LoggedLock::RecMutex player_dummy_lock_;

    Playlist::Crawler::Handle crawler_handle_;

    const LocalPermissionsIface *permissions_;

    Skipper skip_requests_;

    /*!
     * The direction to move the cursor while finding the next playable item
     * after running into a playback error.
     */
    Playlist::Crawler::Direction prefetch_direction_after_failure_;

    /*!
     * Current operation for finding the next item, if any.
     *
     * The sole purpose of having this pointer around is to have something we
     * can cancel when needed. Most code will operate on shared pointers passed
     * around by the crawler.
     *
     * This is, by the way, critical to avoid race conditions and overwritten
     * state. It is possible to cancel this operation, and then allocate and
     * schedule some other operation and assign it here.
     */
    std::shared_ptr<Playlist::Crawler::FindNextOpBase> prefetch_next_item_op_;

    /*!
     * Current operation for getting item details.
     *
     * This operation logically follows a #Playlist::Crawler::FindNextOpBase
     * operation.
     */
    std::shared_ptr<Playlist::Crawler::GetURIsOpBase> prefetch_uris_op_;

    /* simple function which tells us whether or not we can play a stream at
     * given bit rate */
    const std::function<bool(uint32_t)> bitrate_limiter_;

    /* called when there is no next item to play */
    std::function<void(FinishedWith)> finished_notification_;

    /*!
     * Manage retrying of playing streams.
     *
     * This is basically a retry counter coupled with a stream ID. The counter
     * keeps track of how many times the stream player has reported failure of
     * the stream ID. Client code can use it to decide whether or not yet
     * another retry should be attempted.
     */
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

  public:
    Control(const Control &) = delete;
    Control &operator=(const Control &) = delete;

    explicit Control(std::function<bool(uint32_t)> &&bitrate_limiter):
        audio_source_(nullptr),
        with_enforced_intentions_(false),
        player_data_(nullptr),
        permissions_(nullptr),
        prefetch_direction_after_failure_(Playlist::Crawler::Direction::FORWARD),
        bitrate_limiter_(std::move(bitrate_limiter))
    {
        LoggedLock::configure(lock_, "Player::Control", MESSAGE_LEVEL_DEBUG);
        LoggedLock::configure(player_dummy_lock_, "Player::Data dummy", MESSAGE_LEVEL_DEBUG);
    }

    /*!
     * Lock this player control.
     *
     * Before calling \e any function member, the lock must be acquired using
     * this function.
     */
    std::tuple<LoggedLock::UniqueLock<LoggedLock::RecMutex>,
               LoggedLock::UniqueLock<LoggedLock::RecMutex>>
    lock() const
    {
        Control &ncthis(*const_cast<Control *>(this));

        LOGGED_LOCK_CONTEXT_HINT;
        return std::make_tuple(LoggedLock::UniqueLock<LoggedLock::RecMutex>(ncthis.lock_),
                               player_data_ != nullptr
                               ? player_data_->lock()
                               : LoggedLock::UniqueLock<LoggedLock::RecMutex>(ncthis.player_dummy_lock_));
    }

    bool is_any_audio_source_plugged() const
    {
        return audio_source_ != nullptr;
    };

    bool is_audio_source_plugged(const AudioSource &audio_source) const
    {
        return audio_source_ == &audio_source;
    };

    bool is_active_controller() const
    {
        return player_data_ != nullptr && is_any_audio_source_plugged();
    }

    bool is_active_controller_for_audio_source(const AudioSource &audio_source) const
    {
        return player_data_ != nullptr && is_audio_source_plugged(audio_source);
    }

    bool is_active_controller_for_audio_source(const std::string &audio_source_id) const;

    const AudioSource *get_plugged_audio_source() const { return audio_source_; }

    void plug(std::shared_ptr<List::ListViewportBase> skipper_viewport,
              const List::ListIface *list);
    void plug(AudioSource &audio_source, bool with_enforced_intentions,
              const std::function<void(FinishedWith)> &finished_notification,
              const std::string *external_player_id = nullptr);
    void plug(Data &player_data);
    void plug(const std::function<Playlist::Crawler::Handle()> &get_crawler_handle,
              const LocalPermissionsIface &permissions);
    void plug(const LocalPermissionsIface &permissions);
    void unplug(bool is_complete_unplug);

    bool source_selected_notification(const std::string &audio_source_id, bool is_on_hold);
    bool source_deselected_notification(const std::string *audio_source_id);

    void repeat_mode_toggle_request() const;
    void shuffle_mode_toggle_request() const;

    /* functions below are called as a result of user actions that are supposed
     * to take direct, immediate influence on playback, so they impose requests
     * to the system */
    void play_request(std::shared_ptr<Playlist::Crawler::FindNextOpBase> find_op);
    void stop_request(const char *reason);
    void pause_request();
    void skip_forward_request();
    void skip_backward_request();
    void rewind_request();
    void fast_wind_set_speed_request(double speed_factor);
    void seek_stream_request(int64_t value, const std::string &units);

    /* functions below are called as a result of status updates from the
     * system, so they may be direct reactions to preceding user actions */
    void play_notification(ID::Stream stream_id, bool is_new_stream);
    StopReaction stop_notification_ok(ID::Stream stream_id);
    StopReaction stop_notification_with_error(ID::Stream stream_id,
                                              const std::string &error_id,
                                              bool is_urlfifo_empty);
    void pause_notification(ID::Stream stream_id);

    enum class Execution
    {
        NOW,
        DELAYED,
    };

    void start_prefetch_next_item(const char *const reason,
                                  Playlist::Crawler::Bookmark from_where,
                                  Playlist::Crawler::Direction direction,
                                  bool force_play_uri_when_available,
                                  Execution delayed_execution);

    // cppcheck-suppress functionStatic
    void bring_forward_delayed_prefetch();

  private:
    /* skip request handling */
    bool skip_request_prepare(UserIntention previous_intention,
                              Skipper::RunNewFindNextOp &run_new_find_next_fn,
                              Skipper::SkipperDoneCallback &done_fn);

    /* play request handling (immediate playback) */
    bool found_item_for_playing(std::shared_ptr<Playlist::Crawler::FindNextOpBase> op);
    bool found_item_uris_for_playing(Playlist::Crawler::GetURIsOpBase &op,
                                     Playlist::Crawler::Direction from_direction);

    /* prefetch handling (play when possible) */
    bool found_prefetched_item(Playlist::Crawler::FindNextOpBase &op,
                               bool force_play_uri_when_available);
    bool found_prefetched_item_uris(Playlist::Crawler::GetURIsOpBase &op,
                                    Playlist::Crawler::Direction from_direction,
                                    bool force_play_uri_when_available);

    void async_redirect_resolved_for_playing(
            size_t idx, QueuedStream::ResolvedRedirectResult result,
            ID::OurStream for_stream, InsertMode insert_mode,
            PlayNewMode play_new_mode);
    void async_redirect_resolved_prefetched(
            size_t idx, QueuedStream::ResolvedRedirectResult result,
            ID::OurStream for_stream, InsertMode insert_mode,
            PlayNewMode play_new_mode);

    void unexpected_resolve_error(size_t idx,
                                  QueuedStream::ResolvedRedirectResult result);

    using QueueItemRedirectResolved =
        void (Control::*)(size_t, QueuedStream::ResolvedRedirectResult,
                          ID::OurStream, InsertMode, PlayNewMode);

    QueuedStream::OpResult
    queue_item_from_op(Playlist::Crawler::GetURIsOpBase &op,
                       Playlist::Crawler::Direction direction,
                       const QueueItemRedirectResolved &callback,
                       InsertMode insert_mode, PlayNewMode play_new_mode);
    QueuedStream::OpResult
    queue_item_from_op_tail(ID::OurStream stream_id, InsertMode insert_mode,
                            PlayNewMode play_new_mode,
                            QueuedStream::ResolvedRedirectCallback &&callback);

    enum class ReplayResult
    {
        OK,
        GAVE_UP,
        RETRY_FAILED_HARD,
        EMPTY_QUEUE,
    };

    ReplayResult replay(ID::OurStream stream_id, bool is_retry,
                        PlayNewMode play_new_mode);

    void forget_queued_and_playing();
};

}

#endif /* !PLAYER_CONTROL_HH */
