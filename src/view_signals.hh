/*
 * Copyright (C) 2015  T+A elektroakustik GmbH & Co. KG
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

#ifndef VIEW_SIGNALS_HH
#define VIEW_SIGNALS_HH

#include <inttypes.h>

class ViewIface;

class ViewSignalsIface
{
  protected:
    static constexpr uint16_t signal_display_serialize_request = 1U << 0;
    static constexpr uint16_t signal_display_serialize_pending = 1U << 1;
    static constexpr uint16_t signal_display_update_request    = 1U << 2;
    static constexpr uint16_t signal_display_update_pending    = 1U << 3;
    static constexpr uint16_t signal_request_hide_view         = 1U << 4;

    explicit ViewSignalsIface() {}

  public:
    ViewSignalsIface(const ViewSignalsIface &) = delete;
    ViewSignalsIface &operator=(const ViewSignalsIface &) = delete;

    virtual ~ViewSignalsIface() {}

    /*!
     * Current view has new information to show on the display.
     */
    virtual void request_display_update(ViewIface *view) = 0;

    /*!
     * Current view wants to be hidden.
     */
    virtual void request_hide_view(ViewIface *view) = 0;

    /*!
     * Attempt to fully serialize failed, need to try again later.
     */
    virtual void display_serialize_pending(ViewIface *view) = 0;

    /*!
     * Attempt to partially update failed, need to try again later.
     */
    virtual void display_update_pending(ViewIface *view) = 0;
};

#endif /* !VIEW_SIGNALS_HH */
