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

#ifndef PLAYER_HH
#define PLAYER_HH

#include <string>
#include <chrono>
#include <deque>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

#include "streaminfo.hh"
#include "playinfo.hh"
#include "playback_abort_enqueue.hh"
#include "messages.h"

namespace List { class DBusList; }

namespace Playback
{

class State;

class MetaDataStoreIface
{
  protected:
    explicit MetaDataStoreIface() {}

  public:
    MetaDataStoreIface(const MetaDataStoreIface &) = delete;
    MetaDataStoreIface &operator=(const MetaDataStoreIface &) = delete;

    virtual ~MetaDataStoreIface() {}

    virtual void meta_data_add_begin(bool is_update) = 0;
    virtual void meta_data_add(const char *key, const char *value) = 0;
    virtual bool meta_data_add_end() = 0;
};

/*!
 * Interface class for interfacing with the external stream player.
 *
 * There are two basic modes of operation, namely \e active mode and \e passive
 * mode. Active mode corresponds to actions initiated by the user through some
 * view, usually initialted via remote control. Passive mode corresponds to
 * actions initiated by other means such as starting playback by other devices
 * or other daemons (app, TCP connection, timer, etc.).
 *
 * The major difference is that active mode is a result of conscious user
 * actions with an explicit "plan" about what is supposed to happen (such as
 * playing a playlist, traversing through directory structure and playing it,
 * shuffled playback, etc.), and passive mode is all about monitoring what's
 * going on and displaying these information. In active mode, the player is
 * "owned" by some view and there is always some view-specific state that
 * represents planned playback actions and its progress; in passive mode there
 * is nothing.
 *
 * Playing streams (or not) and displaying stream information are things that
 * are independent of active and passive modes. It is possible to have a stream
 * playing in both modes, and it is possible to have no stream playing in both
 * modes as well.
 */
class PlayerIface
{
  protected:
    explicit PlayerIface() {}

  public:
    PlayerIface(const PlayerIface &) = delete;
    PlayerIface &operator=(const PlayerIface &) = delete;

    virtual ~PlayerIface() {}

    using IsBufferingCallback = std::function<void(bool is_buffering)>;

    virtual void start() = 0;
    virtual void shutdown() = 0;

    /*!
     * Take over the player using the given playback state and start position.
     *
     * This function enters active mode.
     *
     * The player will configure the given state to start playing at the given
     * line in given list. The playback mode is embedded in the state object
     * and advancing back and forth through the list is implemented there as
     * well, so the mode is taken care of.
     *
     * If the player is already taken by another view when this function is
     * called, then that view's state is reverted and the new state is used.
     *
     * Most function members of the #Playback::PlayerIface class have no effect
     * if this function has not be called.
     *
     * \param playback_state
     *     A view-specific playback state. It is possible for multiple views to
     *     have different, independently existing states, but only at most one
     *     of them can be tied to a player.
     *
     * \param file_list, line
     *     The list the playback state shall use and the line to begin with.
     *     The state object will typically iterate over items in and below the
     *     given list. How exactly it will iterate depends on the configuration
     *     of the \p playback_state object.
     *
     * \param buffering_callback
     *     Function to call when buffering of the stream starts or stops. The
     *     function is only called the first time the stream is buffered and
     *     can be used to activate or hide the play view Note that the function
     *     will be called from a worker thread context, so it may have to be
     *     thread-safe to some degree.
     *
     * \see
     *     #Playback::PlayerIface::release()
     */
    virtual void take(State &playback_state, const List::DBusList &file_list, int line,
                      IsBufferingCallback buffering_callback) = 0;

    /*!
     * Explicitly stop and release the player.
     *
     * This function leaves active mode and enters passive mode. If the player
     * is in passive mode already, then the function has no effect.
     *
     * For clean end of playing, the player should be released when playback is
     * supposed to end. This avoids accidental restarting of playback by
     * spurious calls of other functions.
     *
     * \param active_stop_command
     *     If true, send a stop command to the stream player.
     *
     * \param stop_playbackmode_state_if_active
     *     If true and the player is in active mode, then put the associated
     *     playback mode state to stop mode.
     */
    virtual void release(bool active_stop_command,
                         bool stop_playbackmode_state_if_active) = 0;

    /*!
     * To be called when the stream player notifies that is has started
     * playing a new stream.
     */
    virtual void start_notification(ID::Stream stream_id, bool try_enqueue) = 0;

    /*!
     * To be called when the stream player notifies that it has stopped playing
     * at all.
     */
    virtual void stop_notification() = 0;

    /*!
     * To be called when the stream player notifies that is has paused
     * playback.
     */
    virtual void pause_notification() = 0;

    /*!
     * To be called when the stream player sends new track times.
     */
    virtual bool track_times_notification(const std::chrono::milliseconds &position,
                                          const std::chrono::milliseconds &duration) = 0;

    /*!
     * Return meta data for currently playing stream.
     *
     * \returns
     *     Track meta data, or \c nullptr in case there is not track playing at
     *     the moment.
     */
    virtual std::pair<const PlayInfo::MetaData *, std::unique_lock<std::mutex>>
    get_track_meta_data() const = 0;

    /*!
     * Return current (assumed) stream playback state.
     */
    virtual PlayInfo::Data::StreamState get_assumed_stream_state() const = 0;

    /*!
     * Return current track's position and total duration (in this order).
     */
    virtual std::pair<std::chrono::milliseconds, std::chrono::milliseconds> get_times() const = 0;

    /*!
     * Return name of stream with given ID as it appeared in the list.
     *
     * Used as fallback in case no other meta information are available.
     */
    virtual const std::string *get_original_stream_name(ID::Stream id) const = 0;

    /*!
     * Force skipping to previous track, if any.
     *
     * If there is no previous track and \p rewind_threshold is 0, then this
     * function has no effect.
     *
     * \param rewind_threshold
     *     Must be 0 or positive. If this parameter is positive, then skipping
     *     behaviour is modified as follows. If the currently playing track has
     *     advanced at least the given amount of milliseconds, then the track
     *     is restarted from the beginning. Otherwise, the player skips to the
     *     previous track if there is any, or it skips to the beginning of the
     *     currently playing track if there is no previous track.
     */
    virtual void skip_to_previous(std::chrono::milliseconds rewind_threshold) = 0;

    /*!
     * Force skipping to next track, if any.
     *
     * If there is no next track then this function has no effect.
     */
    virtual void skip_to_next() = 0;
};

/*!
 * The audio player.
 *
 * FIXME: This is a mess now. The step to asynchronous D-Bus communication
 *        induced by bug #290 has turned this simple thing into a nightmare of
 *        mutexes and state flags. The whole enqueuing mechanism should be
 *        redesigned so that it becomes easier to control and introspect from
 *        the player. There should be an enqueuing thread that can be asked to
 *        enqueue the next N streams, stop enqueuing, etc., and that also
 *        manages a #StreamInfo object.
 *        NOTE: This is not easy and must be properly designed on paper first!
 */
class Player: public PlayerIface, public MetaDataStoreIface
{
  private:
    /*!
     * Thread that walks through the current #Playback::State, if any.
     *
     * This thread is heavily I/O-bound. It communicates with any list broker
     * tied to the playback state and with the stream player over D-Bus. In a
     * loop, it tries to pull as many URIs from the list broker as possible
     * while pushing them to the stream player. There can be heavy delays when
     * pulling URIs from the list brokers, especially in case the lists are
     * originating from the Internet (read: Airable). Therefore, doing it
     * asynchronously is mandatory to keep the UI responsive.
     */
    std::thread stream_enqueuer_;

    class Controller
    {
      private:
        std::mutex lock_;
        std::condition_variable wait_for_last_unref_;

        /* avoid pulling the rug from under users of the #State object */
        std::atomic_uint refcount_;

        /* active vs passive mode */
        State *current_state_;

      public:
        class RefCountWrapper
        {
          private:
            Controller &controller_;

          public:
            explicit RefCountWrapper(Controller &controller):
                controller_(controller)
            {
                std::lock_guard<std::mutex> lock(controller_.lock_);

                if(controller_.current_state_ != nullptr)
                    ++controller_.refcount_;
            }

            explicit RefCountWrapper(Controller &controller,
                                     bool unlocked_dummy):
                controller_(controller)
            {
                if(controller_.current_state_ != nullptr)
                    ++controller_.refcount_;
            }

            ~RefCountWrapper()
            {
                std::lock_guard<std::mutex> lock(controller_.lock_);

                if(controller_.current_state_ == nullptr)
                    return;

                log_assert(controller_.refcount_ > 0);
                --controller_.refcount_;

                if(controller_.refcount_ == 0)
                    controller_.wait_for_last_unref_.notify_all();
            }

            State *get() { return controller_.current_state_; }
            const State *get() const { return controller_.current_state_; }
        };

        explicit Controller():
            refcount_(0),
            current_state_(nullptr)
        {}

        RefCountWrapper ref()
        {
            return RefCountWrapper(*this);
        }

        RefCountWrapper update_and_ref(State *new_state)
        {
            std::unique_lock<std::mutex> lock(lock_);

            wait_for_last_unref_.wait(lock,
                                      [this] () { return refcount_ == 0; });

            current_state_ = new_state;

            return ref_unlocked();
        }

      private:
        RefCountWrapper ref_unlocked()
        {
            return RefCountWrapper(*this, true);
        }
    };

    struct CurrentStreamData
    {
        std::mutex lock_;

        bool waiting_for_start_notification_;

        ID::OurStream stream_id_;
        StreamInfo stream_info_;
        PlayInfo::Data track_info_;

        explicit CurrentStreamData():
            waiting_for_start_notification_(false),
            stream_id_(ID::OurStream::make_invalid()),
            track_info_(PlayInfo::Data::STREAM_STOPPED)
        {}
    };

    class SynchronizedRequest
    {
      private:
        std::mutex lock_;
        std::condition_variable done_;
        std::atomic_bool requested_;

      public:
        SynchronizedRequest(const SynchronizedRequest &) = delete;
        SynchronizedRequest &operator=(const SynchronizedRequest &) = delete;

        explicit SynchronizedRequest(): requested_(false) {}

        bool is_requested()
        {
            return requested_.load();
        }

        std::unique_lock<std::mutex> lock()
        {
            return std::unique_lock<std::mutex>(lock_);
        }

        bool request()
        {
            return requested_.exchange(true);
        }

        void wait(std::unique_lock<std::mutex> &lock_req)
        {
            done_.wait(lock_req, [this] () { return !requested_.load(); });
        }

        /*
        std::unique_lock<std::mutex> synchronize(bool set_request)
        {
            if(set_request)
                request();

            auto lock_req(lock());
            wait(lock_req);

            return lock_req;
        }
        */

        void ack()
        {
            std::lock_guard<std::mutex> lock_req(lock_);

            if(requested_.exchange(false))
                done_.notify_all();
        }
    };

    /* take influence on the worker thread's behavior */
    struct Requests
    {
      public:
        SynchronizedRequest release_player_;
        std::atomic_bool stop_enqueuing_;
        std::atomic_bool shutdown_request_;

        explicit Requests():
            stop_enqueuing_(false),
            shutdown_request_(false)
        {}
    };

    struct LockWithStopRequest: public AbortEnqueueIface
    {
      private:
        std::unique_lock<std::mutex> lock_csd_;
        Requests &requests_;
        std::atomic_bool &is_enqueuing_flag_;
        bool is_unlocked_;

      public:
        LockWithStopRequest(const LockWithStopRequest &) = delete;
        LockWithStopRequest &operator=(const LockWithStopRequest &) = delete;

        explicit LockWithStopRequest(CurrentStreamData &current_stream_data,
                                     Requests &requests,
                                     std::atomic_bool &is_enqueuing_flag):
            lock_csd_(current_stream_data.lock_, std::defer_lock),
            requests_(requests),
            is_enqueuing_flag_(is_enqueuing_flag),
            is_unlocked_(true)
        {}

        bool may_continue() const override
        {
            return !requests_.stop_enqueuing_.load();
        }

        void unlock() override
        {
            log_assert(!is_unlocked_);

            is_unlocked_ = true;
            lock_csd_.unlock();

            sched_yield();

        }

        void lock() override
        {
            log_assert(is_unlocked_);

            lock_csd_.lock();
            is_unlocked_ = false;
        }

        bool enqueue_start() override { return is_enqueuing_flag_.exchange(true); }
        bool enqueue_stop() override { return is_enqueuing_flag_.exchange(false); }
    };

    /*!
     * Functions as messages.
     *
     * Each message is executed with the #Playback::Player::controller_,
     * #Playback::Player::current_stream_data_, and
     * #Playback::Player::requests_ structures locked.
     */
    using Message = std::function<void(LockWithStopRequest &lockstop)>;

    class MessageQueue
    {
      private:
        std::mutex lock_;
        std::condition_variable have_messages_;
        std::condition_variable is_idle_;
        bool processing_message_;

      public:
        std::deque<Message> messages_;

        MessageQueue(const MessageQueue &) = delete;
        MessageQueue &operator=(const MessageQueue &) = delete;

        explicit MessageQueue():
            processing_message_(false)
        {}

        std::unique_lock<std::mutex> lock()
        {
            return std::unique_lock<std::mutex>(lock_);
        }

        std::unique_lock<std::mutex> drain(std::atomic_bool &waiting_for_idle,
                                           std::atomic_bool &shutdown_request)
        {
            waiting_for_idle.exchange(true);

            auto queue_lock(lock());

            is_idle_.wait(queue_lock,
                [this, &shutdown_request] () -> bool
                {
                    return
                        shutdown_request.load() ||
                        (!processing_message_ && messages_.empty());
                });

            waiting_for_idle.exchange(false);

            return queue_lock;
        }

        void wake_up()
        {
            have_messages_.notify_one();
        }

        void wait(std::unique_lock<std::mutex> &queue_lock,
                  std::atomic_bool &shutdown_request)
        {
            have_messages_.wait(queue_lock,
                [this, &shutdown_request] () -> bool
                {
                    return shutdown_request.load() || !messages_.empty();
                });

            processing_message_ = !shutdown_request.load();
        }

        void message_processed()
        {
            auto queue_lock(lock());

            processing_message_ = false;

            if(messages_.empty())
                is_idle_.notify_all();
        }
    };

    Controller controller_;
    CurrentStreamData current_stream_data_;
    MessageQueue message_queue_;
    Requests requests_;
    std::atomic_bool enqueuing_in_progress_;

    PlayInfo::MetaData incoming_meta_data_;
    const PlayInfo::Reformatters &meta_data_reformatters_;

    void do_take(LockWithStopRequest &lockstop,
                 const State *expected_playback_state,
                 const List::DBusList &file_list, int line,
                 IsBufferingCallback buffering_callback);
    void do_release(LockWithStopRequest &lockstop, bool active_stop_command,
                    bool stop_playbackmode_state_if_active);
    void do_start_notification(LockWithStopRequest &lockstop, ID::Stream stream_id, bool try_enqueue);
    void do_skip_to_previous(LockWithStopRequest &lockstop, bool allow_restart_stream);
    void do_skip_to_next(LockWithStopRequest &lockstop);

    bool send_message(Message &&message);

  public:
    Player(const Player &) = delete;
    Player &operator=(const Player &) = delete;

    explicit Player(const PlayInfo::Reformatters &meta_data_reformatters):
        meta_data_reformatters_(meta_data_reformatters)
    {}

    void start() override;
    void shutdown() override;

    void take(State &playback_state, const List::DBusList &file_list, int line,
              IsBufferingCallback buffering_callback) override;
    void release(bool active_stop_command,
                 bool stop_playbackmode_state_if_active = true) override;

    void start_notification(ID::Stream stream_id, bool try_enqueue) override;
    void stop_notification() override;
    void pause_notification() override;
    bool track_times_notification(const std::chrono::milliseconds &position,
                                  const std::chrono::milliseconds &duration) override;

    std::pair<const PlayInfo::MetaData *, std::unique_lock<std::mutex>>
    get_track_meta_data() const override;

    PlayInfo::Data::StreamState get_assumed_stream_state() const override;
    PlayInfo::Data::StreamState get_assumed_stream_state__unlocked() const;
    std::pair<std::chrono::milliseconds, std::chrono::milliseconds> get_times() const override;
    std::pair<std::chrono::milliseconds, std::chrono::milliseconds> get_times__unlocked() const;
    const std::string *get_original_stream_name(ID::Stream id) const override;

    void skip_to_previous(std::chrono::milliseconds rewind_threshold) override;
    void skip_to_next() override;

    void meta_data_add_begin(bool is_update) override;
    void meta_data_add(const char *key, const char *value) override;
    bool meta_data_add_end() override;

  private:
    bool is_active_mode(const Playback::State *new_state = nullptr);
    bool is_different_active_mode(const Playback::State *new_state);
    void set_assumed_stream_state(PlayInfo::Data::StreamState state);
    const std::string *get_original_stream_name(ID::OurStream id) const;

    bool try_take(State &playback_state, const List::DBusList &file_list,
                  int line, IsBufferingCallback buffering_callback);
    bool try_fast_skip();

    static bool get_next_message(MessageQueue &queue,
                                 std::atomic_bool &shutdown_request,
                                 Message &message);
    void worker_main();
};

}

#endif /* !PLAYER_HH */
