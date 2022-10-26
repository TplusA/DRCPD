/*
 * Copyright (C) 2016--2020, 2023  T+A elektroakustik GmbH & Co. KG
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

#ifndef BUSY_HH
#define BUSY_HH

#include <functional>
#include <inttypes.h>

namespace Busy
{

/*!
 * Busy sources with activation/deactivation counters.
 *
 * These are suitable for internal actions which are completely under our own
 * control.
 */
enum class Source
{
    /* stream player */
    WAITING_FOR_PLAYER,
    FILLING_PLAYER_QUEUE,
    BUFFERING_STREAM,

    /* list operations */
    GETTING_LIST_ID,
    GETTING_PARENT_LINK,
    GETTING_LIST_CONTEXT_ROOT_LINK,
    GETTING_ITEM_URI,
    GETTING_ITEM_STREAM_LINKS,
    GETTING_LIST_RANGE,
    CHECKING_LIST_RANGE,
    RESUMING_PLAYBACK,
    GETTING_LOCATION_TRACE,
    REALIZING_LOCATION_TRACE,

    /* internal */
    FIRST_SOURCE = WAITING_FOR_PLAYER,
    LAST_SOURCE = REALIZING_LOCATION_TRACE,
};

/*!
 * Busy sources without counters.
 *
 * These are suitable for externally observed actions.
 */
enum class DirectSource
{
    /* audio sources */
    WAITING_FOR_APPLIANCE_AUDIO,

    /* internal */
    FIRST_SOURCE = WAITING_FOR_APPLIANCE_AUDIO,
    LAST_SOURCE = WAITING_FOR_APPLIANCE_AUDIO,
};

void init(const std::function<void(bool)> &state_changed_callback);
bool set(Source src);
bool clear(Source src);
bool set(DirectSource src);
bool clear(DirectSource src);
bool is_busy();

}
#endif /* !BUSY_HH */
