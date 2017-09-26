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
    void stop(std::unique_ptr<CacheEnforcer> self, bool remove_override);

    bool is_stopped() const { return state_ == State::STOPPED; }

    ID::List get_list_id() const { return list_id_; }

  private:
    static void process_dbus(GObject *source_object, GAsyncResult *res, gpointer user_data);
    static gboolean process_timer(gpointer user_data);
};

}

#endif /* !CACHEENFORCER_HH */
