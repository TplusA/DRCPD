/*
 * Copyright (C) 2017, 2019  T+A elektroakustik GmbH & Co. KG
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

#ifndef CACHEENFORCER_HH
#define CACHEENFORCER_HH

#include "dbuslist.hh"

namespace Playlist
{

class CacheEnforcer
{
  private:
    enum class State
    {
        CREATED,
        STARTED,
        STOPPED,
    };

    std::mutex lock_;

    std::unique_ptr<CacheEnforcer> pointer_to_self_;

    State state_;
    const List::DBusList &list_;
    ID::List list_id_;

    guint timer_id_;
    tdbuslistsNavigation *nav_proxy_;

  public:
    CacheEnforcer(const CacheEnforcer &) = delete;
    CacheEnforcer &operator=(const CacheEnforcer &) = delete;

    explicit CacheEnforcer(const List::DBusList &list, ID::List list_id):
        state_(State::CREATED),
        list_(list),
        list_id_(list_id),
        timer_id_(0),
        nav_proxy_(nullptr)
    {}

    ~CacheEnforcer()
    {
        log_assert(pointer_to_self_ == nullptr);
    }

    void start();
    static void stop(std::unique_ptr<CacheEnforcer> self, bool remove_override);

    bool is_stopped() const { return state_ == State::STOPPED; }

    ID::List get_list_id() const { return list_id_; }

  private:
    static void process_dbus(GObject *source_object, GAsyncResult *res, gpointer user_data);
    static gboolean process_timer(gpointer user_data);
};

}

#endif /* !CACHEENFORCER_HH */
