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

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "cacheenforcer.hh"
#include "dbus_common.h"

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

    std::lock_guard<std::mutex> lock(enforcer.lock_);

    log_assert(enforcer.nav_proxy_ != nullptr);

    guint64 list_expiry_ms = 0;
    GError *error = nullptr;

    tdbus_lists_navigation_call_force_in_cache_finish(enforcer.nav_proxy_,
                                                      &list_expiry_ms,
                                                      res, &error);
    if(dbus_common_handle_error(&error, "Force list into cache") < 0)
        enforcer.state_ = State::STOPPED;
    else if(list_expiry_ms == 0)
    {
        msg_error(0, LOG_NOTICE, "List %u cannot be forced into cache",
                  enforcer.list_id_.get_raw_id());
        enforcer.state_ = State::STOPPED;
    }

    enforcer.nav_proxy_ = nullptr;

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
        enforcer.pointer_to_self_ = nullptr;
        break;
    }
}

gboolean Playlist::CacheEnforcer::process_timer(gpointer user_data)
{
    auto &enforcer = *static_cast<Playlist::CacheEnforcer *>(user_data);

    std::lock_guard<std::mutex> lock(enforcer.lock_);

    enforcer.timer_id_ = 0;

    switch(enforcer.state_)
    {
      case State::CREATED:
        enforcer.state_ = State::STARTED;

        /* fall-through */

      case State::STARTED:
        log_assert(enforcer.nav_proxy_ == nullptr);
        enforcer.nav_proxy_ = enforcer.list_.get_dbus_proxy();

        if(enforcer.nav_proxy_ != nullptr)
        {
            tdbus_lists_navigation_call_force_in_cache(enforcer.nav_proxy_,
                                                       enforcer.list_id_.get_raw_id(),
                                                       true, nullptr,
                                                       process_dbus, user_data);
            break;
        }

        msg_error(0, LOG_ERR, "No D-Bus proxy, cannot force list into cache");
        enforcer.state_ = State::STOPPED;

        /* fall-through */

      case State::STOPPED:
        enforcer.pointer_to_self_ = nullptr;
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
        self_raw_ptr->nav_proxy_ = self_raw_ptr->list_.get_dbus_proxy();

        if(self_raw_ptr->nav_proxy_ != nullptr && self_raw_ptr->list_id_.is_valid())
            tdbus_lists_navigation_call_force_in_cache(self_raw_ptr->nav_proxy_,
                                                       self_raw_ptr->list_id_.get_raw_id(),
                                                       false, nullptr, nullptr, nullptr);
    }

    self_raw_ptr->nav_proxy_ = nullptr;
}
