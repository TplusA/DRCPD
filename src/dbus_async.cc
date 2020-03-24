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

#include "dbus_async.hh"

#include <set>

struct Data
{
    LoggedLock::Mutex lock;

    /* Yet another workaround to get memory management with shitty GLib right:
     * async operations must register themselves when started, and unregister
     * inside the GLib async callback when they are not needed anymore; of
     * course, this resembles malloc()/free(), so it effectively defeats the
     * benefits of smart pointers... */
    std::set<std::shared_ptr<DBus::AsyncCall_>> active_queries;

    Data()
    {
        LoggedLock::configure(lock, "DBus::AsyncCallPool", MESSAGE_LEVEL_DEBUG);
    }
};

static Data async_call_pool_data;

namespace DBus
{
namespace AsyncCallPool
{

void register_call(std::shared_ptr<AsyncCall_> call)
{
    LOGGED_LOCK_CONTEXT_HINT;
    std::lock_guard<LoggedLock::Mutex> lock(async_call_pool_data.lock);
    auto &aq(async_call_pool_data.active_queries);

    BUG_IF(aq.find(call) != aq.end(),
           "Async call %p already registered", static_cast<void *>(call.get()));
    aq.emplace(std::move(call));
}

void unregister_call(std::shared_ptr<AsyncCall_> call)
{
    LOGGED_LOCK_CONTEXT_HINT;
    std::lock_guard<LoggedLock::Mutex> lock(async_call_pool_data.lock);
    auto &aq(async_call_pool_data.active_queries);

    BUG_IF(aq.find(call) == aq.end(),
           "Async call %p not registered", static_cast<void *>(call.get()));
    aq.erase(call);
}

}
}
