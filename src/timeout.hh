/*
 * Copyright (C) 2016, 2019  T+A elektroakustik GmbH & Co. KG
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

#ifndef TIMEOUT_HH
#define TIMEOUT_HH

#include <stdint.h>
#include <chrono>
#include <functional>

namespace Timeout
{

class Timer
{
  public:
    /*!
     * Type of the callback function to be invoked when the timer expires.
     *
     * \retval std::chrono::milliseconds::min()
     *         Causes the timer to be disabled.
     * \retval std::chrono::milliseconds::zero()
     *         Causes the timer to continue using the current configuration.
     * \retval t  A new timer value used to restart the timer.
     */
    using TimeoutCallback = std::function<std::chrono::milliseconds()>;

  private:
    uint32_t timeout_event_source_id_;
    std::chrono::milliseconds timeout_;
    TimeoutCallback callback_;

  public:
    Timer(const Timer &) = delete;
    Timer &operator=(const Timer &) = delete;

    explicit Timer():
        timeout_event_source_id_(0)
    {}

    bool start(std::chrono::milliseconds timeout, TimeoutCallback callback);
    void stop();

  private:
    bool keep_or_restart(std::chrono::milliseconds timeout);
    static int expired(void *timer_object);
};

}

#endif /* !TIMEOUT_HH */
