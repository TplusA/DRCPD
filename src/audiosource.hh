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

#include <string>
#include <functional>

#include "player_resume_data.hh"
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
    using StateChangedFn =
        std::function<void(const AudioSource &src, AudioSourceState prev_state)>;

    const std::string id_;

  private:
    AudioSourceState state_;
    const StateChangedFn state_changed_callback_;

    ResumeData resume_data_;

    struct _tdbussplayURLFIFO *urlfifo_proxy_;
    struct _tdbussplayPlayback *playback_proxy_;

  public:
    AudioSource(const AudioSource &) = delete;
    AudioSource(AudioSource &&) = default;
    AudioSource &operator=(const AudioSource &) = delete;

    explicit AudioSource(std::string &&id, StateChangedFn &&state_changed_fn):
        id_(std::move(id)),
        state_(AudioSourceState::DESELECTED),
        state_changed_callback_(state_changed_fn),
        urlfifo_proxy_(nullptr),
        playback_proxy_(nullptr)
    {}

    AudioSourceState get_state() const { return state_; }

    void set_proxies(struct _tdbussplayURLFIFO *urlfifo_proxy,
                     struct _tdbussplayPlayback *playback_proxy)
    {
        switch(state_)
        {
          case AudioSourceState::DESELECTED:
          case AudioSourceState::REQUESTED:
            BUG("Set D-Bus proxies for not selected audio source %s", id_.c_str());
            break;

          case AudioSourceState::SELECTED:
            break;
        }

        urlfifo_proxy_ = urlfifo_proxy;
        playback_proxy_ = playback_proxy;
    }

    struct _tdbussplayURLFIFO *get_urlfifo_proxy() const { return urlfifo_proxy_; }
    struct _tdbussplayPlayback *get_playback_proxy() const { return playback_proxy_; }

    void deselected_notification()
    {
        set_state(AudioSourceState::DESELECTED);
        urlfifo_proxy_ = nullptr;
        playback_proxy_ = nullptr;
    }

    void request()
    {
        switch(state_)
        {
          case AudioSourceState::DESELECTED:
            set_state(AudioSourceState::REQUESTED);
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
            set_state(AudioSourceState::SELECTED);
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
            set_state(AudioSourceState::SELECTED);
            break;

          case AudioSourceState::SELECTED:
            break;
        }
    }

    void resume_data_reset() { resume_data_.reset(); }
    const ResumeData &get_resume_data() const { return resume_data_; }

    void resume_data_update(CrawlerResumeData &&data)
    {
        resume_data_.crawler_data_ = std::move(data);
    }

  private:
    void set_state(AudioSourceState new_state)
    {
        if(new_state == state_)
            return;

        const auto prev_state = state_;
        state_ = new_state;

        if(state_changed_callback_ != nullptr)
            state_changed_callback_(*this, prev_state);
    }
};

}

#endif /* !AUDIOSOURCE_HH */
