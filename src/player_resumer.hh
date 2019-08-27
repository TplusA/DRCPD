/*
 * Copyright (C) 2017, 2019  T+A elektroakustik GmbH & Co. KG
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

#ifndef PLAYER_RESUMER_HH
#define PLAYER_RESUMER_HH

#include <string>

#include "messages.h"

namespace Player
{

class Resumer
{
  public:
    enum class RequestState
    {
        INITIALIZED,
        WAITING_FOR_AUDIO_SOURCE,
        HAVE_AUDIO_SOURCE,
        WAITING_FOR_LIST_BROKER,
    };

  private:
    RequestState state_;
    std::string url_;
    uint32_t cookie_;

  public:
    Resumer(const Resumer &) = delete;
    Resumer &operator=(const Resumer &) = delete;

    explicit Resumer():
        state_(RequestState::INITIALIZED),
        cookie_(0)
    {}

    ~Resumer()
    {
        switch(state_)
        {
          case RequestState::INITIALIZED:
            break;

          case RequestState::WAITING_FOR_AUDIO_SOURCE:
          case RequestState::HAVE_AUDIO_SOURCE:
          case RequestState::WAITING_FOR_LIST_BROKER:
            Busy::clear(Busy::Source::RESUMING_PLAYBACK);
            break;
        }
    }

    RequestState get_state() const { return state_; }

    bool set_url(const char *url)
    {
        log_assert(state_ == RequestState::INITIALIZED);
        return url != nullptr ? set_url(std::move(std::string(url))) : false;
    }

    bool set_url(std::string &&url)
    {
        log_assert(state_ == RequestState::INITIALIZED);

        if(url.empty())
            return false;

        Busy::set(Busy::Source::RESUMING_PLAYBACK);
        url_ = std::move(url);
        state_ = RequestState::WAITING_FOR_AUDIO_SOURCE;

        return true;
    }

    void audio_source_available_notification()
    {
        log_assert(state_ == RequestState::WAITING_FOR_AUDIO_SOURCE);
        state_ = RequestState::HAVE_AUDIO_SOURCE;
    }

    bool set_cookie(uint32_t cookie)
    {
        log_assert(state_ == RequestState::HAVE_AUDIO_SOURCE);

        if(cookie == 0)
            return false;

        cookie_ = cookie;
        state_ = RequestState::WAITING_FOR_LIST_BROKER;

        return true;
    }

    const std::string &get_url() const { return url_; }
    uint32_t get_cookie() const { return cookie_; }
};

}

#endif /* !PLAYER_RESUMER_HH */
