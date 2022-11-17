/*
 * Copyright (C) 2016, 2019, 2021, 2022  T+A elektroakustik GmbH & Co. KG
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

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <glib.h>
#include <errno.h>

#include "messages.h"
#include "timeout.hh"

bool Timeout::Timer::start(std::chrono::milliseconds &&timeout,
                           TimeoutCallback &&callback)
{
    msg_log_assert(callback != nullptr);

    if(timeout_event_source_id_ > 0)
    {
        MSG_BUG("Timer already started");
        return false;
    }

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
    callback_ = std::move(callback);

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

bool Timeout::Timer::keep_or_restart(std::chrono::milliseconds &&timeout)
{
    if(timeout == timeout_ || timeout == timeout.zero())
        return true;

    timeout_event_source_id_ = 0;

    if(timeout != timeout.min())
        start(std::move(timeout), std::move(callback_));

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
