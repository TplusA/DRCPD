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

#include "rnfcall_death_row.hh"
#include "rnfcall.hh"

#include <glib.h>

static gboolean do_execute(gpointer user_data)
{
    static_cast<DBusRNF::DeathRow *>(user_data)->execute();
    return G_SOURCE_REMOVE;
}

void DBusRNF::DeathRow::enter(std::shared_ptr<CallBase> &&call)
{
    if(call == nullptr)
        return;

    std::lock_guard<LoggedLock::Mutex> lock(lock_);
    zombies_.emplace_back(std::move(call));
    g_idle_add(do_execute, this);
}

void DBusRNF::DeathRow::execute()
{
    std::lock_guard<LoggedLock::Mutex> lock(lock_);
    zombies_.clear();
}
