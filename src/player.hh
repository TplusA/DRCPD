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

    virtual bool take(State &playback_state, const List::DBusList &file_list, int line) = 0;
    virtual void release() = 0;

    virtual void stop_notification() = 0;
    virtual void enqueue_next() = 0;

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
