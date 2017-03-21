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

struct _tdbussplayURLFIFO;
struct _tdbussplayPlayback;

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

    struct _tdbussplayURLFIFO *urlfifo_proxy_;
    struct _tdbussplayPlayback *playback_proxy_;

  public:
    AudioSource(const AudioSource &) = delete;
    AudioSource &operator=(const AudioSource &) = delete;

    explicit AudioSource(const char *id):
        id_(id),
        state_(AudioSourceState::DESELECTED),
        urlfifo_proxy_(nullptr),
        playback_proxy_(nullptr)
    {}

    bool is_deselected() const { return state_ == AudioSourceState::DESELECTED; }
    bool is_requested() const { return state_ == AudioSourceState::REQUESTED; }
    bool is_selected() const { return state_ == AudioSourceState::SELECTED; }

    void set_proxies(struct _tdbussplayURLFIFO *urlfifo_proxy,
                     struct _tdbussplayPlayback *playback_proxy)
    {
        if(!is_selected())
            BUG("Set D-Bus proxies for not selected audio source %s", id_);

        urlfifo_proxy_ = urlfifo_proxy;
        playback_proxy_ = playback_proxy;
    }

    struct _tdbussplayURLFIFO *get_urlfifo_proxy() const { return urlfifo_proxy_; }
    struct _tdbussplayPlayback *get_playback_proxy() const { return playback_proxy_; }

    void deselected_notification()
    {
        state_ = AudioSourceState::DESELECTED;
        urlfifo_proxy_ = nullptr;
        playback_proxy_ = nullptr;
    }

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
