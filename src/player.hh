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

#include "streaminfo.hh"
#include "playinfo.hh"

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
     * Most function members have no effect if this function has not be called.
     *
     * \returns
     *     True on success, false on error (start playing failed for some
     *     reason).
     *
     * \see
     *     #Playback::PlayerIface::release()
     */
    virtual bool take(State &playback_state, const List::DBusList &file_list, int line,
                      std::function<void(bool)> buffering_callback) = 0;

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
     */
    virtual void release(bool active_stop_command) = 0;

    /*!
     * To be called when the stream player notifies that is has started
     * playing a new stream.
     */
    virtual void start_notification(uint16_t stream_id, bool try_enqueue) = 0;

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
    virtual const PlayInfo::MetaData &get_track_meta_data() const = 0;

    /*!
     * Return current (assumed) stream playback state.
     */
    virtual PlayInfo::Data::StreamState get_assumed_stream_state() const = 0;

    /*!
     * Return true if start playing notification is pending.
     */
    virtual bool is_buffering() const = 0;

    /*!
     * Return current track's position and total duration (in this order).
     */
    virtual std::pair<std::chrono::milliseconds, std::chrono::milliseconds> get_times() const = 0;

    /*!
     * Return name of stream with given ID as it appeared in the list.
     *
     * Used as fallback in case no other meta information are available.
     */
    virtual const std::string *get_original_stream_name(uint16_t id) const = 0;

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

class Player: public PlayerIface, public MetaDataStoreIface
{
  private:
    /* active vs passive mode */
    State *current_state_;
    bool waiting_for_start_notification_;

    /* information about currently playing stream */
    uint16_t current_stream_id_;
    StreamInfo stream_info_;
    PlayInfo::MetaData incoming_meta_data_;
    PlayInfo::Data track_info_;

    const PlayInfo::Reformatters &meta_data_reformatters_;

  public:
    Player(const Player &) = delete;
    Player &operator=(const Player &) = delete;

    explicit Player(const PlayInfo::Reformatters &meta_data_reformatters):
        current_state_(nullptr),
        waiting_for_start_notification_(false),
        current_stream_id_(0),
        meta_data_reformatters_(meta_data_reformatters)
    {}

    bool take(State &playback_state, const List::DBusList &file_list, int line,
              std::function<void(bool)> buffering_callback) override;
    void release(bool active_stop_command) override;

    void start_notification(uint16_t stream_id, bool try_enqueue) override;
    void stop_notification() override;
    void pause_notification() override;
    bool track_times_notification(const std::chrono::milliseconds &position,
                                  const std::chrono::milliseconds &duration) override;

    const PlayInfo::MetaData &get_track_meta_data() const override;
    PlayInfo::Data::StreamState get_assumed_stream_state() const override;
    bool is_buffering() const override { return waiting_for_start_notification_; }
    std::pair<std::chrono::milliseconds, std::chrono::milliseconds> get_times() const override;
    const std::string *get_original_stream_name(uint16_t id) const override;

    void skip_to_previous(std::chrono::milliseconds rewind_threshold) override;
    void skip_to_next() override;

    void meta_data_add_begin(bool is_update) override;
    void meta_data_add(const char *key, const char *value) override;
    bool meta_data_add_end() override;

  private:
    bool is_active_mode(const Playback::State *new_state = nullptr) const;
    void set_assumed_stream_state(PlayInfo::Data::StreamState state);
};

}

#endif /* !PLAYER_HH */
