/*
 * Copyright (C) 2016, 2017, 2019, 2022  T+A elektroakustik GmbH & Co. KG
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

#ifndef PLAYER_PERMISSIONS_HH
#define PLAYER_PERMISSIONS_HH

#include <cinttypes>

namespace Player
{

class LocalPermissionsIface
{
  protected:
    constexpr explicit LocalPermissionsIface() {}

  public:
    LocalPermissionsIface(const LocalPermissionsIface &) = delete;
    LocalPermissionsIface &operator=(const LocalPermissionsIface &) = delete;

    virtual ~LocalPermissionsIface() {}

    virtual bool can_play() const = 0;
    virtual bool can_pause() const = 0;
    virtual bool can_resume() const = 0;
    virtual bool can_skip_backward() const = 0;
    virtual bool can_skip_forward() const = 0;
    virtual bool can_fast_wind_backward() const = 0;
    virtual bool can_fast_wind_forward() const = 0;
    virtual bool can_set_shuffle() const = 0;
    virtual bool can_toggle_shuffle() const = 0;
    virtual bool can_repeat_single() const = 0;
    virtual bool can_repeat_all() const = 0;
    virtual bool can_toggle_repeat() const = 0;
    virtual bool can_show_listing() const = 0;
    virtual bool can_prefetch_for_gapless() const = 0;
    virtual bool can_skip_on_error() const = 0;
    virtual bool retry_if_stream_broken() const = 0;
    virtual uint8_t maximum_number_of_prefetched_streams() const = 0;
};

class DefaultLocalPermissions: public LocalPermissionsIface
{
  public:
    DefaultLocalPermissions(const DefaultLocalPermissions &) = delete;
    DefaultLocalPermissions &operator=(const DefaultLocalPermissions &) = delete;

    constexpr explicit DefaultLocalPermissions() {}

    bool can_play()                 const override { return true; }
    bool can_pause()                const override { return true; }
    bool can_resume()               const override { return true; }
    bool can_skip_backward()        const override { return true; }
    bool can_skip_forward()         const override { return true; }
    bool can_fast_wind_backward()   const override { return true; }
    bool can_fast_wind_forward()    const override { return true; }
    bool can_set_shuffle()          const override { return true; }
    bool can_toggle_shuffle()       const override { return true; }
    bool can_repeat_single()        const override { return true; }
    bool can_repeat_all()           const override { return true; }
    bool can_toggle_repeat()        const override { return true; }
    bool can_show_listing()         const override { return true; }
    bool can_prefetch_for_gapless() const override { return true; }
    bool can_skip_on_error()        const override { return true; }
    bool retry_if_stream_broken()   const override { return false; }
    uint8_t maximum_number_of_prefetched_streams() const override { return 5; }
};

}

#endif /* !PLAYER_PERMISSIONS_HH */
