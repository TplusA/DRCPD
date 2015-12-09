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

#include "streaminfo.hh"

namespace List { class DBusList; }

namespace Playback
{

class State;

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
     * To be called when the stream player notifies that it has stopped playing
     * at all.
     */
    virtual void stop_notification() = 0;

    /*!
     * To be called when the stream player notifies end of stream.
     */
    virtual void enqueue_next() = 0;

    /*!
     * Return name of currently playing stream as it appeared in the list.
     *
     * Used as fallback in case no other meta information are available.
     */
    virtual const std::string *get_original_stream_name(uint16_t id) = 0;
};

class Player: public PlayerIface
{
  private:
    State *current_state_;
    StreamInfo stream_info_;

  public:
    Player(const Player &) = delete;
    Player &operator=(const Player &) = delete;

    explicit Player():
        current_state_(nullptr)
    {}

    bool take(State &playback_state, const List::DBusList &file_list, int line) override;
    void release() override;

    void stop_notification() override;
    void enqueue_next() override;

    const std::string *get_original_stream_name(uint16_t id) override;
};

}

#endif /* !PLAYER_HH */
