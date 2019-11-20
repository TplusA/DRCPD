/*
 * Copyright (C) 2017, 2019, 2020  T+A elektroakustik GmbH & Co. KG
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

#include "cacheenforcer.hh"
#include "gerrorwrapper.hh"

static uint32_t compute_enforcer_refresh_seconds(guint64 timeout_ms)
{
    static constexpr const uint64_t plausible_maximum_seconds = 30U * 60U;
    static constexpr const uint64_t maximum_difference_to_timeout_ms = 42U * 1000U;

    const uint64_t t_ms = (timeout_ms * guint64(80)) / guint64(100);
    uint64_t t_seconds = t_ms / guint(1000);

    if(t_seconds > 0 && t_seconds <= plausible_maximum_seconds)
    {
        if(t_ms + maximum_difference_to_timeout_ms >= timeout_ms)
            return t_seconds;

        t_seconds = (timeout_ms - maximum_difference_to_timeout_ms) / guint64(1000);
    }

    return t_seconds == 0 ? 1U : plausible_maximum_seconds;
}

void Playlist::CacheEnforcer::process_dbus(GObject *source_object,
                                           GAsyncResult *res, gpointer user_data)
{
    auto &enforcer = *static_cast<Playlist::CacheEnforcer *>(user_data);
    std::unique_lock<std::mutex> lock(enforcer.lock_);

    guint64 list_expiry_ms = 0;
    GErrorWrapper error;

    tdbus_lists_navigation_call_force_in_cache_finish(TDBUS_LISTS_NAVIGATION(source_object),
                                                      &list_expiry_ms,
                                                      res, error.await());
    if(error.log_failure("Force list into cache"))
        enforcer.state_ = State::STOPPED;
    else if(list_expiry_ms == 0)
    {
        msg_error(0, LOG_NOTICE, "List %u cannot be forced into cache",
                  enforcer.list_id_.get_raw_id());
        enforcer.state_ = State::STOPPED;
    }

    switch(enforcer.state_)
    {
      case State::CREATED:
        BUG("Impossible state");
        break;

      case State::STARTED:
        enforcer.timer_id_ =
            g_timeout_add_seconds(compute_enforcer_refresh_seconds(list_expiry_ms),
                                  process_timer, user_data);
        break;

      case State::STOPPED:
        {
            /* we need to keep the lock which we have acquired at the top of
             * this function in valid state, making sure to destroy the
             * referenced object only after the lock has been released */
            // cppcheck-suppress unreadVariable
            auto last_ref = std::move(enforcer.pointer_to_self_);
            lock.unlock();
        }

        break;
    }
}

gboolean Playlist::CacheEnforcer::process_timer(gpointer user_data)
{
    auto &enforcer = *static_cast<Playlist::CacheEnforcer *>(user_data);
    std::unique_lock<std::mutex> lock(enforcer.lock_);

    enforcer.timer_id_ = 0;

    switch(enforcer.state_)
    {
      case State::CREATED:
        enforcer.state_ = State::STARTED;

        /* fall-through */

      case State::STARTED:
        {
            auto *proxy = enforcer.list_.get_dbus_proxy();

            if(proxy != nullptr)
            {
                tdbus_lists_navigation_call_force_in_cache(
                    proxy, enforcer.list_id_.get_raw_id(), true, nullptr,
                    process_dbus, user_data);
                break;
            }
        }

        msg_error(0, LOG_ERR, "No D-Bus proxy, cannot force list into cache");
        enforcer.state_ = State::STOPPED;

        /* fall-through */

      case State::STOPPED:
        {
            /* we need to keep the lock which we have acquired at the top of
             * this function in valid state, making sure to destroy the
             * referenced object only after the lock has been released */
            // cppcheck-suppress unreadVariable
            auto last_ref = std::move(enforcer.pointer_to_self_);
            lock.unlock();
        }

        break;
    }

    return G_SOURCE_REMOVE;
}

void Playlist::CacheEnforcer::start()
{
    log_assert(state_ == State::CREATED);
    process_timer(this);
}

void Playlist::CacheEnforcer::stop(std::unique_ptr<CacheEnforcer> self,
                                   bool remove_override)
{
    if(self == nullptr)
        return;

    std::lock_guard<std::mutex> lock(self->lock_);

    CacheEnforcer *const self_raw_ptr = self.get();

    switch(self->state_)
    {
      case State::CREATED:
      case State::STOPPED:
        break;

      case State::STARTED:
        self_raw_ptr->pointer_to_self_ = std::move(self);
        break;
    }

    /* dereferencing self is *unsafe* from this point on */
    self_raw_ptr->state_ = State::STOPPED;

    if(remove_override)
    {
        auto *proxy = self_raw_ptr->list_.get_dbus_proxy();

        if(proxy != nullptr && self_raw_ptr->list_id_.is_valid())
            tdbus_lists_navigation_call_force_in_cache(
                proxy, self_raw_ptr->list_id_.get_raw_id(), false, nullptr,
                nullptr, nullptr);
    }
}
