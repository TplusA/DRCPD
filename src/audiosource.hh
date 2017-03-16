/*
 * Copyright (C) 2017  T+A elektroakustik GmbH & Co. KG
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

#ifndef AUDIOSOURCE_HH
#define AUDIOSOURCE_HH

#include "messages.h"

namespace Player
{

enum class AudioSourceState
{
    DESELECTED,
    REQUESTED,
    SELECTED,
};

class AudioSource
{
  public:
    const char *const id_;

  private:
    AudioSourceState state_;

  public:
    AudioSource(const AudioSource &) = delete;
    AudioSource &operator=(const AudioSource &) = delete;

    explicit AudioSource(const char *id):
        id_(id),
        state_(AudioSourceState::DESELECTED)
    {}

    bool is_selected() const { return state_ == AudioSourceState::SELECTED; }

    void deselected_notification() { state_ = AudioSourceState::DESELECTED; }

    void request()
    {
        switch(state_)
        {
          case AudioSourceState::DESELECTED:
            state_ = AudioSourceState::REQUESTED;
            break;

          case AudioSourceState::REQUESTED:
          case AudioSourceState::SELECTED:
            break;
        }
    }

    void selected_notification()
    {
        switch(state_)
        {
          case AudioSourceState::DESELECTED:
            BUG("Cannot switch to selected state directly");
            break;

          case AudioSourceState::REQUESTED:
            state_ = AudioSourceState::SELECTED;
            break;

          case AudioSourceState::SELECTED:
            break;
        }
    }

    void select_now()
    {
        switch(state_)
        {
          case AudioSourceState::REQUESTED:
            BUG("Bogus direct switch to selected state from requested state");

            /* fall-through */

          case AudioSourceState::DESELECTED:
            state_ = AudioSourceState::SELECTED;
            break;

          case AudioSourceState::SELECTED:
            break;
        }
    }
};

}

#endif /* !AUDIOSOURCE_HH */
