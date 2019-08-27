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

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "view_src_app.hh"
#include "player_permissions.hh"

class AppPermissions: public Player::DefaultLocalPermissions
{
  public:
    AppPermissions(const AppPermissions &) = delete;
    AppPermissions &operator=(const AppPermissions &) = delete;

    constexpr explicit AppPermissions() {}

    bool can_resume()               const override { return false; }
    bool can_skip_backward()        const override { return false; }
    bool can_skip_forward()         const override { return false; }
    bool can_set_shuffle()          const override { return false; }
    bool can_toggle_shuffle()       const override { return false; }
    bool can_repeat_single()        const override { return false; }
    bool can_repeat_all()           const override { return false; }
    bool can_toggle_repeat()        const override { return false; }
    bool can_show_listing()         const override { return false; }
    bool can_prefetch_for_gapless() const override { return false; }
};

const Player::LocalPermissionsIface &ViewSourceApp::View::get_local_permissions() const
{
    static const AppPermissions permissions;
    return permissions;
}
