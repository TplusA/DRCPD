/*
 * Copyright (C) 2015, 2016, 2017, 2019  T+A elektroakustik GmbH & Co. KG
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

#ifndef DBUS_IFACE_PROXIES_HH
#define DBUS_IFACE_PROXIES_HH

#include "dbus_iface.hh"
#include "dcpd_dbus.h"
#include "lists_dbus.h"
#include "streamplayer_dbus.h"
#include "airable_dbus.h"
#include "audiopath_dbus.h"
#include "configuration_dbus.h"

namespace DBus
{

tdbuslistsNavigation *get_lists_navigation_iface(ListbrokerID listbroker_id);
tdbussplayURLFIFO *get_streamplayer_urlfifo_iface();
tdbussplayPlayback *get_streamplayer_playback_iface();
tdbussplayPlayback *get_roonplayer_playback_iface();
tdbusdcpdPlayback *get_dcpd_playback_iface();
tdbusAirable *get_airable_sec_iface();
tdbusaupathManager *audiopath_get_manager_iface();

}

#endif /* !DBUS_IFACE_PROXIES_HH */
