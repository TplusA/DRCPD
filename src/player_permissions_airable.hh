/*
 * Copyright (C) 2016  T+A elektroakustik GmbH & Co. KG
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

#ifndef PLAYER_PERMISSIONS_AIRABLE_HH
#define PLAYER_PERMISSIONS_AIRABLE_HH

namespace Player
{

class AirablePermissions: public DefaultLocalPermissions
{
  public:
    AirablePermissions(const AirablePermissions &) = delete;
    AirablePermissions &operator=(const AirablePermissions &) = delete;

    constexpr explicit AirablePermissions() {}

    /* playing from root directory is prohibited because of the various local
     * permissions in directories below, plus there are filters, search
     * buttons, logout buttons... */
    bool can_play()                 const override { return false; }
};

class AirableRadiosPermissions: public DefaultLocalPermissions
{
  public:
    AirableRadiosPermissions(const AirableRadiosPermissions &) = delete;
    AirableRadiosPermissions &operator=(const AirableRadiosPermissions &) = delete;

    constexpr explicit AirableRadiosPermissions() {}

    bool can_pause()                const override { return false; }
    bool can_fast_wind_backward()   const override { return false; }
    bool can_fast_wind_forward()    const override { return false; }
    bool can_shuffle()              const override { return false; }
    bool can_repeat_single()        const override { return false; }
    bool can_repeat_all()           const override { return false; }
    bool can_prefetch_for_gapless() const override { return false; }
};

class DeezerProgramPermissions: public DefaultLocalPermissions
{
  public:
    DeezerProgramPermissions(const DeezerProgramPermissions &) = delete;
    DeezerProgramPermissions &operator=(const DeezerProgramPermissions &) = delete;

    constexpr explicit DeezerProgramPermissions() {}

    bool can_skip_backward()        const override { return false; }
    bool can_skip_forward()         const override { return false; }
    bool can_fast_wind_backward()   const override { return false; }
    bool can_fast_wind_forward()    const override { return false; }
    bool can_shuffle()              const override { return false; }
    bool can_repeat_single()        const override { return false; }
    bool can_repeat_all()           const override { return false; }
    bool can_show_listing()         const override { return false; }
    bool can_prefetch_for_gapless() const override { return false; }
};

class QobuzPermissions: public DefaultLocalPermissions
{
  public:
    QobuzPermissions(const QobuzPermissions &) = delete;
    QobuzPermissions &operator=(const QobuzPermissions &) = delete;

    constexpr explicit QobuzPermissions() {}

    bool can_prefetch_for_gapless() const override { return false; }
};

}

#endif /* !PLAYER_PERMISSIONS_AIRABLE_HH */
