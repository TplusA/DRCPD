/*
 * Copyright (C) 2015, 2019, 2022  T+A elektroakustik GmbH & Co. KG
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

#ifndef DBUS_IFACE_HH
#define DBUS_IFACE_HH

/*!
 * \addtogroup dbus DBus handling
 */
/*!@{*/

namespace DBus
{

enum class ListbrokerID
{
    FILESYSTEM,
    AIRABLE,
    UPNP,
};

enum class PlaybackSignalSenderID
{
    DCPD,
    REST_API,

    SENDER_ID_LAST = REST_API,
};

int setup(bool connect_to_session_bus, void *dbus_signal_data_for_dbus_handlers);
void shutdown();

}

/*!@}*/

#endif /* !DBUS_IFACE_HH */
