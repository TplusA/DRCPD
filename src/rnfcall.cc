/*
 * Copyright (C) 2019, 2020  T+A elektroakustik GmbH & Co. KG
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

#include "rnfcall.hh"
#include "dump_enum_value.hh"

#include <sstream>

bool DBusRNF::CallBase::abort_request()
{
    return abort_request_internal(false);
}

bool DBusRNF::CallBase::abort_request_internal(bool suppress_errors)
{
    std::unique_lock<LoggedLock::Mutex> lock(lock_);

    switch(state_)
    {
      case CallState::INITIALIZED:
      case CallState::WAIT_FOR_NOTIFICATION:
      case CallState::READY_TO_FETCH:
        break;

      case CallState::RESULT_FETCHED:
      case CallState::FAILED:
        if(was_aborted_after_done_)
        {
            if(!suppress_errors)
                BUG("Multiple aborts of finished RNF call (state %d)", int(state_));

            return false;
        }

        was_aborted_after_done_ = true;
        return true;

      case CallState::ABORTING:
      case CallState::ABORTED_BY_LIST_BROKER:
        if(!suppress_errors)
            BUG("Multiple aborts of RNF call (state %d)", int(state_));

        return false;

      case CallState::ABOUT_TO_DESTROY:
        return true;
    }

    const auto cookie = clear_cookie();
    if(cookie == 0)
    {
        set_state(CallState::ABORTED_BY_LIST_BROKER);
        return true;
    }

    lock.unlock();

    try
    {
        if(!abort_cookie_fn_(cookie))
            return false;

        lock.lock();
        set_state(CallState::ABORTING);
        return true;
    }
    catch(...)
    {
        BUG("Got exception while aborting cookie %u", cookie);
    }

    return false;
}

void DBusRNF::CallBase::notification(uint32_t cookie, CallState new_state,
                                     const char *what)
{
    std::lock_guard<LoggedLock::Mutex> lock(lock_);

    if(cookie == 0)
    {
        BUG("%s notification for invalid cookie [%p]",
            what, static_cast<void *>(this));
        return;
    }

    if(cookie != cookie_ && cookie != cleared_cookie_)
    {
        BUG("%s notification for wrong cookie %u (expected %u or %u) [%p]",
            what, cookie, cookie_, cleared_cookie_, static_cast<void *>(this));
        return;
    }

    switch(state_)
    {
      case CallState::WAIT_FOR_NOTIFICATION:
      case CallState::ABORTING:
        set_state(new_state);
        notified_.notify_all();
        break;

      case CallState::INITIALIZED:
      case CallState::READY_TO_FETCH:
      case CallState::RESULT_FETCHED:
      case CallState::ABORTED_BY_LIST_BROKER:
      case CallState::FAILED:
      case CallState::ABOUT_TO_DESTROY:
        BUG("%s notification in unexpected state %d [%p]",
            what, int(state_), static_cast<void *>(this));
        break;
    }
}

std::string DBusRNF::CallBase::get_description() const
{
    static const std::array<const char *const, 8> state_names
    {
        "INITIALIZED", "WAIT_FOR_NOTIFICATION", "READY_TO_FETCH",
        "RESULT_FETCHED", "ABORTING", "ABORTED_BY_LIST_BROKER",
        "FAILED", "ABOUT_TO_DESTROY",
    };

    std::ostringstream os;
    dump_enum_value(os, state_names, "CallState", state_);

    return
        std::string("state ") + os.str() +
        ", cookie " + std::to_string(int(cookie_)) +
        " [" + std::to_string(int(cleared_cookie_)) + "], " +
        (detached_ ? "" : "not ") + "detached";
}
