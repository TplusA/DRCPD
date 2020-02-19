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

#ifndef RNFCALL_DEATH_ROW_HH
#define RNFCALL_DEATH_ROW_HH

#include "logged_lock.hh"

#include <list>
#include <memory>

namespace DBusRNF
{

class CallBase;

class DeathRow
{
  private:
    LoggedLock::Mutex lock_;
    std::list<std::shared_ptr<CallBase>> zombies_;

  public:
    DeathRow(const DeathRow &) = delete;
    DeathRow(DeathRow &&) = default;
    DeathRow &operator=(const DeathRow &) = delete;
    DeathRow &operator=(DeathRow &&) = default;

    explicit DeathRow()
    {
        LoggedLock::configure(lock_, "DBusRNF::DeathRow", MESSAGE_LEVEL_DEBUG);
    }

    void enter(std::shared_ptr<CallBase> &&call);
    void execute();
};

}

#endif /* !RNFCALL_DEATH_ROW_HH */
