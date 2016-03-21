/*
 * Copyright (C) 2016  T+A elektroakustik GmbH & Co. KG
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

#include <glib.h>
#include <errno.h>

#include "messages.h"
#include "timeout.hh"

bool Timeout::Timer::start(std::chrono::milliseconds timeout,
                           TimeoutCallback callback)
{
    log_assert(callback != nullptr);
    log_assert(timeout_event_source_id_ == 0);

    static constexpr const std::chrono::milliseconds minimum_timeout =
        std::chrono::milliseconds(50);

    if(timeout < minimum_timeout)
        timeout = minimum_timeout;

    GSource *src = g_timeout_source_new(timeout.count());

    if(src == nullptr)
    {
        msg_error(ENOMEM, LOG_EMERG, "Failed allocating timeout event source");
        return false;
    }

    timeout_ = timeout;
    callback_ = callback;

    g_source_set_callback(src, Timeout::Timer::expired, this, nullptr);
    timeout_event_source_id_ = g_source_attach(src, nullptr);

    return true;
}

void Timeout::Timer::stop()
{
    if(timeout_event_source_id_ != 0)
    {
        g_source_remove(timeout_event_source_id_);
        timeout_event_source_id_ = 0;
    }
}

bool Timeout::Timer::keep_or_restart(std::chrono::milliseconds timeout)
{
    if(timeout == timeout_ || timeout == timeout.zero())
        return true;

    timeout_event_source_id_ = 0;

    if(timeout != timeout.min())
        start(timeout, callback_);

    return false;
}

int Timeout::Timer::expired(void *timer_object)
{
    auto *const timer = static_cast<Timeout::Timer *>(timer_object);

    if(timer == nullptr || timer->timeout_event_source_id_ == 0)
        return G_SOURCE_REMOVE;

    if(timer->callback_ == nullptr)
    {
        timer->timeout_event_source_id_ = 0;
        return G_SOURCE_REMOVE;
    }

    if(timer->keep_or_restart(timer->callback_()))
        return G_SOURCE_CONTINUE;
    else
        return G_SOURCE_REMOVE;
}
