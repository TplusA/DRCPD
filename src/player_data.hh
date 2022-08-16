/*
 * Copyright (C) 2016, 2017, 2019--2022  T+A elektroakustik GmbH & Co. KG
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

#ifndef PLAYER_DATA_HH
#define PLAYER_DATA_HH

#include "metadata.hh"
#include "dbus_async.hh"
#include "playback_modes.hh"
#include "playlist_cursor.hh"
#include "de_tahifi_airable.h"
#include "airable_links.hh"
#include "logged_lock.hh"
#include "dbus_iface_proxies.hh"
#include "gvariantwrapper.hh"

#include <map>
#include <deque>
#include <unordered_set>

namespace Player
{

/*!
 * User's intention.
 *
 * What the user had in mind according to the last command that we have
 * received. The stream state should follow the user's intention.
 */
enum class UserIntention
{
    NOTHING,
    STOPPING,
    PAUSING,
    LISTENING,
    SKIPPING_PAUSED,
    SKIPPING_LIVE,
};

/*!
 * Stream player state, technically.
 *
 * Which state the currently playing stream is in, if any.
 */
enum class PlayerState
{
    STOPPED,
    BUFFERING,
    PLAYING,
    PAUSED,

    STREAM_STATE_LAST = PAUSED,
};

/*!
 * Stream state as shown to the user.
 *
 * Do not use this enum for anything except displaying purposes.
 */
enum class VisibleStreamState
{
    STOPPED,
    BUFFERING,
    PLAYING,
    PAUSED,
    FAST_FORWARD,
    FAST_REWIND,

    LAST = FAST_REWIND,
};

using AppStreamID = ID::SourcedStream<STREAM_ID_SOURCE_APP>;

using AsyncResolveRedirect =
    DBus::AsyncCall<tdbusAirable, std::tuple<guchar, gchar *>,
                    Busy::Source::RESOLVING_AIRABLE_REDIRECT>;

/*!
 * Information about the currently playing stream, if any.
 *
 * These are pure information about the currently playing stream, not about
 * controller states, planned actions, actions in transition. We store observed
 * information as reported by the player in an object of this type.
 */
class NowPlayingInfo
{
  private:
    /*!
     * The ID of the currently playing stream.
     *
     * This variable is set based on player reports, i.e., it reflects the
     * player's reality. It is set for any kind of stream.
     */
    ID::Stream stream_id_;

    std::chrono::milliseconds stream_position_;
    std::chrono::milliseconds stream_duration_;

    const std::function<void(ID::Stream)> on_remove_cb_;

    /*!
     * Meta data as received from connected stream player.
     */
    std::unique_ptr<MetaData::Set> meta_data_;

    /*!
     * Just for dcpd...
     */
    std::string stream_url_;

  public:
    NowPlayingInfo(const NowPlayingInfo &) = delete;
    NowPlayingInfo(NowPlayingInfo &&) = default;
    NowPlayingInfo &operator=(const NowPlayingInfo &) = delete;

    explicit NowPlayingInfo(std::function<void(ID::Stream)> &&on_remove_cb):
        stream_id_(ID::Stream::make_invalid()),
        stream_position_(-1),
        stream_duration_(-1),
        on_remove_cb_(std::move(on_remove_cb))
    {}

    /*!
     * There is a stream playing now, as per report from some player.
     */
    void now_playing(ID::Stream stream_id, std::string &&stream_url);

    /*!
     * There is no stream playing now.
     */
    void nothing();

    ID::Stream get_stream_id() const { return stream_id_; }

    bool put_meta_data(ID::Stream stream_id, std::unique_ptr<MetaData::Set> meta_data);
    const MetaData::Set &get_meta_data(ID::Stream stream_id = ID::Stream::make_invalid()) const;

    bool is_stream(const ID::Stream &id) const
    {
        log_assert(id.is_valid());
        return stream_id_ == id;
    }

    bool update_times(const std::chrono::milliseconds &position,
                      const std::chrono::milliseconds &duration)
    {
        if(stream_position_ == position && stream_duration_ == duration)
            return false;

        stream_position_ = position;
        stream_duration_ = duration;
        return true;
    }

    /*!
     * Return current stream's position and total duration (in this order).
     */
    std::pair<std::chrono::milliseconds, std::chrono::milliseconds> get_times() const
    {
        return std::pair<std::chrono::milliseconds, std::chrono::milliseconds>(
                    stream_position_, stream_duration_);
    }
};

class QueueError: public std::logic_error
{
  public:
    explicit QueueError(const std::string &what_arg): logic_error(what_arg) {}
    explicit QueueError(const char *what_arg): logic_error(what_arg) {}
};

/*!
 * Information about a queued stream.
 */
class QueuedStream
{
  public:
    const ID::OurStream stream_id_;

    /* for list reference counting */
    const ID::List list_id_;

    /* for jumping back to this stream, for recovering the list crawler state,
     * for diagnostics */
    enum class State
    {
        /*! This object has just been constructed, no actions going on */
        FLOATING,

        /*! Waiting for indirect URI to be resolved (resolving link to link) */
        RESOLVING_INDIRECT_URI,

        /*! Stream URI definitely available or definitely empty */
        MAY_HAVE_DIRECT_URI,

        /*! Stream URI has been sent to stream player */
        QUEUED,

        /*! This object describes the currently playing stream */
        CURRENT,

        /*! Object is waiting for its destruction */
        ABOUT_TO_DIE,
    };

    enum class OpResult
    {
        STARTED,
        SUCCEEDED,
        FAILED,
        CANCELED,
    };

    enum class ResolvedRedirectResult
    {
        FOUND,
        FAILED,
        CANCELED,
    };

    using ResolvedRedirectCallback =
        std::function<void(size_t idx, ResolvedRedirectResult result)>;

  private:
    State state_;

    GVariantWrapper stream_key_;
    std::unique_ptr<MetaData::Set> meta_data_;
    std::vector<std::string> uris_;
    Airable::SortedLinks airable_links_;

    size_t next_uri_to_try_;

    std::shared_ptr<AsyncResolveRedirect> async_resolve_redirect_call_;

    const std::unique_ptr<Playlist::Crawler::CursorBase> originating_cursor_;

  public:
    QueuedStream(const QueuedStream &) = delete;
    QueuedStream &operator=(const QueuedStream &) = delete;

    explicit QueuedStream(const ID::OurStream &stream_id,
                          const GVariantWrapper &stream_key,
                          std::unique_ptr<MetaData::Set> meta_data,
                          std::vector<std::string> &&uris,
                          Airable::SortedLinks &&airable_links,
                          ID::List list_id,
                          std::unique_ptr<Playlist::Crawler::CursorBase> originating_cursor):
        stream_id_(stream_id),
        list_id_(list_id),
        state_(State::FLOATING),
        stream_key_(stream_key),
        meta_data_(std::move(meta_data)),
        uris_(std::move(uris)),
        airable_links_(std::move(airable_links)),
        next_uri_to_try_(0),
        originating_cursor_(std::move(originating_cursor))
    {
        log_assert(meta_data_ != nullptr);
    }

    ~QueuedStream()
    {
        AsyncResolveRedirect::cancel_and_delete(async_resolve_redirect_call_);
    }

    const GVariantWrapper &get_stream_key() const { return stream_key_; }
    const Playlist::Crawler::CursorBase &get_originating_cursor() const { return *originating_cursor_; }
    const MetaData::Set &get_meta_data() const { return *meta_data_; }

    void iter_reset()
    {
        BUG_IF(state_ == State::RESOLVING_INDIRECT_URI &&
               async_resolve_redirect_call_ == nullptr,
               "No active resolve op in state %d, stream %u",
               int(state_), stream_id_.get().get_raw_id());
        BUG_IF(state_ != State::RESOLVING_INDIRECT_URI &&
               async_resolve_redirect_call_ != nullptr,
               "Active resolve op in state %d, stream %u",
               int(state_), stream_id_.get().get_raw_id());
        AsyncResolveRedirect::cancel_and_delete(async_resolve_redirect_call_);
        next_uri_to_try_ = 0;
    }

    OpResult iter_next(tdbusAirable *proxy, const std::string *&uri,
                       ResolvedRedirectCallback &&callback);

    bool set_state(State new_state, const char *reason)
    {
        if(state_ == State::ABOUT_TO_DIE)
            return false;

        switch(new_state)
        {
          case State::FLOATING:
          case State::RESOLVING_INDIRECT_URI:
            log_assert(state_ == State::FLOATING);
            break;

          case State::MAY_HAVE_DIRECT_URI:
            log_assert(state_ != State::ABOUT_TO_DIE);
            break;

          case State::QUEUED:
            log_assert(state_ == State::MAY_HAVE_DIRECT_URI);
            break;

          case State::CURRENT:
            log_assert(state_ == State::QUEUED);
            break;

          case State::ABOUT_TO_DIE:
            break;
        }

        if(new_state == state_)
            return false;

        state_ = new_state;
        return true;
    }

    bool is_state(State state) const { return state == state_; }

    void prepare_for_recovery()
    {
        switch(state_)
        {
          case QueuedStream::State::QUEUED:
          case QueuedStream::State::CURRENT:
            state_ = State::MAY_HAVE_DIRECT_URI;
            break;

          case State::FLOATING:
          case State::RESOLVING_INDIRECT_URI:
          case State::MAY_HAVE_DIRECT_URI:
          case State::ABOUT_TO_DIE:
            break;
        }
    }

  private:
    bool iter_next_resolved(const std::string *&uri)
    {
        if(next_uri_to_try_ < uris_.size())
        {
            uri = &uris_[next_uri_to_try_++];
            return true;
        }
        else
        {
            uri = nullptr;
            return false;
        }
    }

    void process_resolved_redirect(DBus::AsyncCall_ &async_call, size_t idx,
                                   ResolvedRedirectCallback &&callback);
};

/*!
 * Keep track of queued streams.
 *
 * This class performs bookkeeping of streams that we push to the stream
 * player. It also assigns stream IDs to streams and keeps track of the
 * currently playing stream's ID. When an ID is removed from the queue, a
 * callback is called to notify about this event.
 *
 * The purpose of duplicating the queue content of the stream player is to be
 * able to re-submit the whole queue as part of failure recovery. In case the
 * player stops dead (or crashes and gets restarted), it forgets all queued
 * streams. We can fill it again from information maintained by this class.
 *
 * This class is strictly about the streams pushed into the player queue by our
 * own crawler. It does \e not know anything about app streams or any other
 * kind of foreign streams.
 */
class QueuedStreams
{
  private:
    static constexpr const size_t MAX_ENTRIES = 20;

    /*!
     * Information about all our streams.
     *
     * For each stream ID stored in #Player::QueuedStreams::queue_ or
     * #Player::QueuedStreams::stream_in_flight_, there is an entry in this
     * container storing the details about the stream.
     */
    std::map<ID::OurStream, std::unique_ptr<QueuedStream>> streams_;

    /*!
     * Play queue as submitted to streamplayer.
     */
    std::deque<ID::OurStream> queue_;

    /*!
     * The "hot" stream supposed to be played by streamplayer.
     *
     * It is possible for a stream which is supposed to be played next to be
     * killed off before it had a chance to play. This may happen if there is a
     * skip request sent to the player before it could hand over the stream to
     * the GStreamer backend. In this case, the stream is never played, and we
     * never receive a NowPlaying signal (instead, we will encounter it in some
     * drop list).
     *
     * Since such a stream is not really in queue anymore, we keep it around in
     * this special place. It tells us that we have sent the stream to
     * streamplayer and that streamplayer should know that this is the stream
     * it needs to play.
     */
    ID::OurStream stream_in_flight_;

    /*!
     * A fresh, unused, valid stream ID for any stream to come.
     *
     * This ID is increased whenever it is used for some new stream. To be
     * absolutely sure that collisions are avoided, the increased value is
     * checked against #Player::QueuedStreams::streams_. In case of a
     * collision, the ID is increased and checked again until a vacant ID is
     * found.
     */
    ID::OurStream next_free_stream_id_;

    /*!
     * This function is called whenever a stream is removed from the queue.
     */
    const std::function<void(const QueuedStream &)> on_remove_cb_;

  public:
    QueuedStreams(const QueuedStreams &) = delete;
    QueuedStreams &operator=(const QueuedStreams &) = delete;

    explicit QueuedStreams(std::function<void(const QueuedStream &)> &&on_remove_cb):
        stream_in_flight_(ID::OurStream::make_invalid()),
        next_free_stream_id_(ID::OurStream::make()),
        on_remove_cb_(std::move(on_remove_cb))
    {}

    bool is_full(size_t max_length = MAX_ENTRIES) const
    {
        log_assert(max_length <= MAX_ENTRIES);
        return queue_.size() >= max_length;
    }

    ID::OurStream append(const GVariantWrapper &stream_key,
                         std::unique_ptr<MetaData::Set> meta_data,
                         std::vector<std::string> &&uris,
                         Airable::SortedLinks &&airable_links, ID::List list_id,
                         std::unique_ptr<Playlist::Crawler::CursorBase> originating_cursor);

    /*
     * Remove front item from queue if exists in a given set of IDs.
     *
     * \param ids
     *     A set of stream IDs allowed/expected as front element of the queue.
     *     If the ID of the queue front element is found in this set, then the
     *     front element is removed from the queue and the ID is removed from
     *     the \p ids set.
     *
     * \returns
     *     The removed stream, or \c nullptr if the queue is empty or if
     *     the stream does not occur in \c ids.
     *
     * \throws
     *     #Player::QueueError Given stream ID invalid, not found, or unexpected.
     */
    std::unique_ptr<QueuedStream> remove_front(std::unordered_set<ID::OurStream> &ids);

    /*
     * Remove some item from queue if exists.
     *
     * \param id
     *     The stream ID to remove from the queue.
     *
     * \returns
     *     The removed stream, or \c nullptr if the queue is empty or if
     *     the \c id does not occur in the queue.
     */
    std::unique_ptr<QueuedStream> remove_anywhere(ID::OurStream id);

    /*!
     * Move stream from queue to currently playing, remove currently playing.
     *
     * This function checks if the queue is populated with the given expected
     * value. It will throw a #Player::QueueError in case of a mismatch. This
     * enables us to detect situations where we went out of sync with the
     * stream player.
     *
     * \throws
     *     #Player::QueueError Given stream ID invalid, not found, or unexpected.
     */
    std::unique_ptr<QueuedStream> shift(ID::OurStream expected_next_id);

    /*!
     * Move stream from queue to currently playing, remove currently playing.
     *
     * This function works unchecked.
     *
     * \throws
     *     #Player::QueueError Given stream ID invalid or not found.
     */
    std::unique_ptr<QueuedStream> shift_if_not_flying(const ID::OurStream &id);

    /*!
     * Move stream from queue to currently playing slot if there is no
     * currently playing entry.
     */
    bool shift_if_not_flying();

    std::vector<ID::OurStream> copy_all_stream_ids() const;

    /*!
     * The stream from the head of our queue (the "stream in flight").
     *
     * This is not necessarily the currently playing stream, but might be work
     * in progress; that is, the stream returned by this function might have
     * been sent to streamplayer, but streamplayer may not have received it and
     * thus may not play it yet. Therefore, the stream state needs to be
     * considered when looking at the stream returned by this function.
     *
     * Take a look at #Player::NowPlayingInfo for the currently playing stream
     * as reported by the player.
     */
    ID::OurStream get_head_stream_id() const { return stream_in_flight_; };

    /*!
     * The stream we are trying to play next from the queue.
     */
    ID::OurStream get_next_stream_id() const { return queue_.empty() ? ID::OurStream::make_invalid() : queue_.front(); };

    const QueuedStream *get_stream_by_id(ID::OurStream stream_id) const;

    template <typename T>
    auto with_stream(const ID::OurStream &stream_id,
                     const std::function<T(QueuedStream *qs)> &apply)
    {
        return apply(get_stream_by_id(stream_id));
    }

    template <typename T>
    auto &with_stream(const ID::OurStream &stream_id,
                      const std::function<T &(const QueuedStream *qs)> &apply) const
    {
        return apply(get_stream_by_id(stream_id));
    }

    template <typename T>
    auto with_stream(const ID::OurStream &stream_id,
                     const std::function<T(QueuedStream &qs)> &apply)
    {
        auto *const s = get_stream_by_id(stream_id);

        if(s != nullptr)
            return apply(*s);
        else
            return T();
    }

    size_t clear();
    size_t clear_if(const std::function<bool(const QueuedStream &)> &pred);

    bool is_next(ID::OurStream stream_id) const
    {
        if(!stream_id.get().is_valid())
            return false;

        if(queue_.empty())
            return false;

        return queue_.front() == stream_id;
    }

    bool is_player_queue_filled() const { return !queue_.empty(); }

    void log(const char *prefix = nullptr,
             MessageVerboseLevel level = MESSAGE_LEVEL_DIAG) const;

  private:
    QueuedStream *get_stream_by_id(ID::OurStream stream_id)
    {
        return const_cast<QueuedStream *>(
                    static_cast<const QueuedStreams &>(*this)
                    .get_stream_by_id(stream_id));
    }
};

class ReportedPlaybackState
{
  private:
    DBus::ReportedRepeatMode repeat_mode_;
    DBus::ReportedShuffleMode shuffle_mode_;

  public:
    ReportedPlaybackState(const ReportedPlaybackState &) = delete;
    ReportedPlaybackState &operator=(const ReportedPlaybackState &) = delete;

    explicit ReportedPlaybackState():
        repeat_mode_(DBus::ReportedRepeatMode::UNKNOWN),
        shuffle_mode_(DBus::ReportedShuffleMode::UNKNOWN)
    {}

    void reset()
    {
        repeat_mode_ = DBus::ReportedRepeatMode::UNKNOWN;
        shuffle_mode_ = DBus::ReportedShuffleMode::UNKNOWN;
    }

    bool set(DBus::ReportedRepeatMode repeat_mode,
             DBus::ReportedShuffleMode shuffle_mode)
    {
        const bool changed = (repeat_mode != repeat_mode_ ||
                              shuffle_mode != shuffle_mode_);

        repeat_mode_ = repeat_mode;
        shuffle_mode_ = shuffle_mode;

        return changed;
    }

    DBus::ReportedRepeatMode get_repeat_mode() const
    {
        return repeat_mode_;
    }

    DBus::ReportedShuffleMode get_shuffle_mode() const
    {
        return shuffle_mode_;
    }
};

class Data
{
  private:
    LoggedLock::RecMutex lock_;

    bool is_attached_;

    /*!
     * Streams we have sent to streamplayer, but are not playing yet.
     *
     * We need to know the exact IDs and their order to recover from playback
     * errors. There are situations in which we need restart playback from
     * scratch, and this queue enables us to restore the whole player queue to
     * the previous state.
     *
     * Note that this is actually an optimization for long queues and/or
     * situations where iterating over playlists items is expensive.
     */
    QueuedStreams queued_streams_;

    std::map<ID::List, size_t> referenced_lists_;

    UserIntention intention_;
    PlayerState player_state_;

    /*!
     * Information about the currently playing stream, if any.
     *
     * These are pure information about the stream, not about player or
     * controller states.
     */
    NowPlayingInfo now_playing_;

    ReportedPlaybackState reported_playback_state_;

    double playback_speed_;

    tdbusAirable *airable_proxy_;

  public:
    Data(const Data &) = delete;
    Data &operator=(const Data &) = delete;

    explicit Data():
        is_attached_(false),
        queued_streams_(
            [this] (const auto &qs)
            { remove_data_for_stream(qs, referenced_lists_); }
        ),
        intention_(UserIntention::NOTHING),
        player_state_(PlayerState::STOPPED),
        now_playing_([] (auto id) { msg_info("Finished playing stream %u", id.get_raw_id()); }),
        playback_speed_(1.0),
        airable_proxy_(DBus::get_airable_sec_iface())
    {
        LoggedLock::configure(lock_, "Player::Data", MESSAGE_LEVEL_DEBUG);
    }

    /*!
     * Lock this player data.
     *
     * Before calling \e any function member, the lock must be acquired using
     * this function.
     */
    LoggedLock::UniqueLock<LoggedLock::RecMutex> lock() const
    {
        LOGGED_LOCK_CONTEXT_HINT;
        return LoggedLock::UniqueLock<LoggedLock::RecMutex>(const_cast<Data *>(this)->lock_);
    }

    void attached_to_player_notification()
    {
        reported_playback_state_.reset();
        is_attached_ = true;
    }

    void detached_from_player_notification(bool is_complete_unplug)
    {
        is_attached_ = false;
        reported_playback_state_.reset();
        set_intention(UserIntention::NOTHING);
    }

    void set_intention(UserIntention intention)
    {
        intention_ = intention;
    }

    UserIntention get_intention() const { return intention_; }

    PlayerState get_player_state() const { return player_state_; }
    VisibleStreamState get_current_visible_stream_state() const;

    const NowPlayingInfo &get_now_playing() const { return now_playing_; }
    NowPlayingInfo &get_now_playing() { return now_playing_; }

  private:
    bool set_player_state(PlayerState state);
    bool set_player_state(ID::Stream new_current_stream, PlayerState state);

  public:
    bool set_reported_playback_state(DBus::ReportedRepeatMode repeat_mode,
                                     DBus::ReportedShuffleMode shuffle_mode)
    {
        return is_attached_
            ? reported_playback_state_.set(repeat_mode, shuffle_mode)
            : false;
    }

    DBus::ReportedRepeatMode get_repeat_mode() const
    {
        return reported_playback_state_.get_repeat_mode();
    }

    DBus::ReportedShuffleMode get_shuffle_mode() const
    {
        return reported_playback_state_.get_shuffle_mode();
    }

    const QueuedStreams &queued_streams_get() const { return queued_streams_; }

    ID::OurStream queued_stream_append(const GVariantWrapper &stream_key,
                                       std::unique_ptr<MetaData::Set> meta_data,
                                       std::vector<std::string> &&uris,
                                       Airable::SortedLinks &&airable_links,
                                       ID::List list_id,
                                       std::unique_ptr<Playlist::Crawler::CursorBase> originating_cursor);

    void queued_stream_sent_to_player(ID::OurStream stream_id);

    void queued_stream_playing_next();

    std::vector<ID::OurStream> copy_all_queued_streams_for_recovery();

    void queued_stream_remove(ID::OurStream stream_id);

    void revert_all_queued_streams_to_unqueued();

    void remove_all_queued_streams(bool also_remove_playing_stream);

  private:
    /*!
     * Called when a mismatch between our state and player state is detected.
     *
     * Clear our internal mirror of the player's queue, remove all associated
     * meta data, pretend we are not playing anything.
     */
    void player_failed();

    /*!
     * Update queue: streamplayer has just gave us a new currently playing
     * stream.
     *
     * The parameter is the ID of the stream the player has just switched to.
     * Thus, we forget the currently playing stream and replace it by the next
     * stream in queue (which is expected to match the ID passed in \p next_stream_id).
     */
    bool stream_has_changed(ID::Stream next_stream_id);

  public:
    /*!
     * Update queue: streamplayer has informed us about dropped streams.
     *
     * These streams must be removed from our mirror queue as well, and this is
     * what this function does.
     */
    bool player_dropped_from_queue(const std::vector<ID::Stream> &dropped);

    /*!
     * Update queue: streamplayer has dropped a stream from the queue without
     * actually having played it.
     *
     * The stream can be any of our queued streams, and it may not be the
     * stream at the front. There is one key property about the dropped stream,
     * though: it never hit the player's playback engine, so it never had to
     * chance to alter the playback state. All we need to do here is to find
     * and remove the ID from our mirror queue.
     */
    void player_rejected_unplayed_stream(ID::Stream dropped);

    /*!
     * Player has told us that it is now playing a particular stream.
     *
     * This function only sets the player state to playing if the stream has
     * not been switched, i.e., if \p switched_stream is false.
     */
    bool player_now_playing_stream(ID::Stream stream_id, std::string &&stream_url,
                                   bool switched_stream)
    {
        if(!switched_stream)
            return set_player_state(Player::PlayerState::PLAYING);

        if(ID::OurStream::compatible_with(stream_id))
            stream_has_changed(stream_id);

        now_playing_.now_playing(stream_id, std::move(stream_url));
        return set_player_state(stream_id, Player::PlayerState::PLAYING);
    }

    /*!
     * Player has resumed playback.
     */
    void player_has_resumed()
    {
        set_player_state(Player::PlayerState::PLAYING);
    }

    /*!
     * Player has paused playback.
     *
     * A call of #Player::Data::player_has_resumed() communicates end of
     * paused state.
     */
    void player_has_paused()
    {
        set_player_state(Player::PlayerState::PAUSED);
    }

    /*!
     * Player has stopped playing.
     *
     * A call of this function communicates just one simple observation: the
     * player has stopped. Valid reasons include regular end of stream, I/O
     * error, or external stop signals. Context not available to this function
     * determines whether or not further actions are required.
     */
    void player_has_stopped()
    {
        set_player_state(Player::PlayerState::STOPPED);
        now_playing_.nothing();
    }

    /*!
     * Final stop: player is completely idle.
     */
    void player_finished_and_idle();

    QueuedStream::OpResult
    get_first_stream_uri(const ID::OurStream &stream_id,
                         const GVariantWrapper *&stream_key,
                         const std::string *&uri,
                         QueuedStream::ResolvedRedirectCallback &&callback);
    QueuedStream::OpResult
    get_next_stream_uri(const ID::OurStream &stream_id,
                        const GVariantWrapper *&stream_key,
                        const std::string *&uri,
                        QueuedStream::ResolvedRedirectCallback &&callback);

    const MetaData::Set &
    get_queued_meta_data(const ID::OurStream &stream_id) const;

    bool update_track_times(const ID::Stream &stream_id,
                            const std::chrono::milliseconds &position,
                            const std::chrono::milliseconds &duration);
    bool update_playback_speed(const ID::Stream &stream_id, double speed);

    void append_referenced_lists(std::vector<ID::List> &list_ids) const;

    // cppcheck-suppress functionStatic
    void list_replaced_notification(ID::List old_id, ID::List new_id) const;

  private:
    static void remove_data_for_stream(const QueuedStream &qs,
                                       std::map<ID::List, size_t> &referenced_lists);
};

}

#endif /* !PLAYER_DATA_HH */
