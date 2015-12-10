/*
 * Copyright (C) 2015  T+A elektroakustik GmbH & Co. KG
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
     * The player will configure the given state to start playing at the given
     * line in given list. The playback mode is embedded in the state object
     * and advancing back and forth through the list is implemented there as
     * well, so the mode is taken care of.
     *
     * If the player is currently taken by another view, then that view's state
     * is reverted and the new state is used.
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
    virtual bool take(State &playback_state, const List::DBusList &file_list, int line) = 0;

    /*!
     * Explicitly stop and release the player.
     *
     * For clean end of playing, the player should be released when playback is
     * supposed to end. This avoids accidental restarting of playback by
     * spurious calls of other functions.
     */
    virtual void release() = 0;

    /*!
     * To be called when the stream player notifies that is has started
     * playing.
     */
    virtual void start_notification() = 0;

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
     * To be called when the stream player notifies end of stream.
     */
    virtual void enqueue_next() = 0;

    /*!
     * Return meta data for currently playing stream.
     *
     * \returns
     *     Track meta data, or \c nullptr in case there is not track playing at
     *     the moment.
     */
    virtual const PlayInfo::MetaData *const get_track_meta_data() const = 0;

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
    virtual const std::string *get_original_stream_name(uint16_t id) = 0;

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
    State *current_state_;
    StreamInfo stream_info_;
    PlayInfo::MetaData incoming_meta_data_;
    PlayInfo::Data track_info_;

    const PlayInfo::Reformatters &meta_data_reformatters_;

  public:
    Player(const Player &) = delete;
    Player &operator=(const Player &) = delete;

    explicit Player(const PlayInfo::Reformatters &meta_data_reformatters):
        current_state_(nullptr),
        meta_data_reformatters_(meta_data_reformatters)
    {}

    bool take(State &playback_state, const List::DBusList &file_list, int line) override;
    void release() override;

    void start_notification() override;
    void stop_notification() override;
    void pause_notification() override;
    bool track_times_notification(const std::chrono::milliseconds &position,
                                  const std::chrono::milliseconds &duration) override;
    void enqueue_next() override;

    const PlayInfo::MetaData *const get_track_meta_data() const override;
    PlayInfo::Data::StreamState get_assumed_stream_state() const override;
    std::pair<std::chrono::milliseconds, std::chrono::milliseconds> get_times() const override;
    const std::string *get_original_stream_name(uint16_t id) override;

    void skip_to_next() override;

    void meta_data_add_begin(bool is_update) override;
    void meta_data_add(const char *key, const char *value) override;
    bool meta_data_add_end() override;

  private:
    void set_assumed_stream_state(PlayInfo::Data::StreamState state);
    void clear();
};

}

#endif /* !PLAYER_HH */
