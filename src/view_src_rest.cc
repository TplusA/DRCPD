/*
 * Copyright (C) 2021  T+A elektroakustik GmbH & Co. KG
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

#include "view_src_rest.hh"
#include "player_permissions.hh"

class RESTPermissions: public Player::DefaultLocalPermissions
{
  public:
    RESTPermissions(const RESTPermissions &) = delete;
    RESTPermissions &operator=(const RESTPermissions &) = delete;

    constexpr explicit RESTPermissions() {}

    bool can_show_listing()         const override { return false; }
    bool can_prefetch_for_gapless() const override { return false; }
};

const Player::LocalPermissionsIface &ViewSourceREST::View::get_local_permissions() const
{
    static const RESTPermissions permissions;
    return permissions;
}
