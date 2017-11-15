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

    RequestState get_state() const { return state_; }

    bool set_url(const char *url)
    {
        log_assert(state_ == RequestState::INITIALIZED);

        if(url == nullptr || url[0] == '\0')
            return false;

        url_ = url;
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
