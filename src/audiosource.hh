/*
 * Copyright (C) 2017, 2019, 2021, 2022  T+A elektroakustik GmbH & Co. KG
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
    struct _tdbussplayPlayback *playback_proxy_for_forced_commands_;
    bool reject_proxies_;

  public:
    AudioSource(const AudioSource &) = delete;
    AudioSource(AudioSource &&) = default;
    AudioSource &operator=(const AudioSource &) = delete;

    explicit AudioSource(std::string &&id, StateChangedFn &&state_changed_fn):
        id_(std::move(id)),
        state_(AudioSourceState::DESELECTED),
        state_changed_callback_(state_changed_fn),
        urlfifo_proxy_(nullptr),
        playback_proxy_(nullptr),
        playback_proxy_for_forced_commands_(nullptr),
        reject_proxies_(false)
    {}

    AudioSourceState get_state() const { return state_; }

    void block_player_commands() { reject_proxies_ = true; }

    void set_proxies(struct _tdbussplayURLFIFO *urlfifo_proxy,
                     struct _tdbussplayPlayback *playback_proxy)
    {
        playback_proxy_for_forced_commands_ = playback_proxy;

        if(reject_proxies_)
            return;

        switch(state_)
        {
          case AudioSourceState::DESELECTED:
          case AudioSourceState::REQUESTED:
            MSG_BUG("Set D-Bus proxies for not selected audio source %s", id_.c_str());
            break;

          case AudioSourceState::SELECTED:
            break;
        }

        urlfifo_proxy_ = urlfifo_proxy;
        playback_proxy_ = playback_proxy;
    }

    struct _tdbussplayURLFIFO *get_urlfifo_proxy() const { return urlfifo_proxy_; }
    struct _tdbussplayPlayback *get_playback_proxy(bool force = false) const
    { return force ? playback_proxy_for_forced_commands_ : playback_proxy_; }

    void deselected_notification()
    {
        set_state(AudioSourceState::DESELECTED);
        urlfifo_proxy_ = nullptr;
        playback_proxy_ = nullptr;
        playback_proxy_for_forced_commands_ = nullptr;
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
            MSG_BUG("Cannot switch to selected state directly");
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
            MSG_BUG("Bogus direct switch to selected state from requested state");

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

    void resume_data_update(PlainURLResumeData &&data)
    {
        resume_data_.plain_url_data_ = std::move(data);
    }

  private:
    void set_state(AudioSourceState new_state)
    {
        if(new_state == state_)
            return;

        // cppcheck-suppress variableScope
        const auto prev_state = state_;
        state_ = new_state;

        if(state_changed_callback_ != nullptr)
            state_changed_callback_(*this, prev_state);
    }
};

}

#endif /* !AUDIOSOURCE_HH */
