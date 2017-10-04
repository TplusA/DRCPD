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

#include "view_src_roon.hh"
#include "player_permissions.hh"

class RoonPermissions: public Player::DefaultLocalPermissions
{
  public:
    RoonPermissions(const RoonPermissions &) = delete;
    RoonPermissions &operator=(const RoonPermissions &) = delete;

    constexpr explicit RoonPermissions() {}

    bool can_fast_wind_backward()   const override { return true; }
    bool can_fast_wind_forward()    const override { return true; }
    bool can_shuffle()              const override { return false; }
    bool can_repeat_single()        const override { return false; }
    bool can_repeat_all()           const override { return false; }
    bool can_show_listing()         const override { return false; }
    bool can_prefetch_for_gapless() const override { return false; }
    bool can_skip_on_error()        const override { return false; }
};

const Player::LocalPermissionsIface &ViewSourceRoon::View::get_local_permissions() const
{
    static const RoonPermissions permissions;
    return permissions;
}
