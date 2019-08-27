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

#ifndef DBUS_IFACE_DEEP_H
#define DBUS_IFACE_DEEP_H

#include "dbus_iface.h"
#include "dcpd_dbus.h"
#include "lists_dbus.h"
#include "streamplayer_dbus.h"
#include "airable_dbus.h"
#include "audiopath_dbus.h"
#include "configuration_dbus.h"

#ifdef __cplusplus
extern "C" {
#endif

tdbuslistsNavigation *dbus_get_lists_navigation_iface(dbus_listbroker_id_t listbroker_id);
tdbussplayURLFIFO *dbus_get_streamplayer_urlfifo_iface(void);
tdbussplayPlayback *dbus_get_streamplayer_playback_iface(void);
tdbussplayPlayback *dbus_get_roonplayer_playback_iface(void);
tdbusdcpdPlayback *dbus_get_dcpd_playback_iface(void);
tdbusAirable *dbus_get_airable_sec_iface(void);
tdbusaupathManager *dbus_audiopath_get_manager_iface(void);

#ifdef __cplusplus
}
#endif

#endif /* !DBUS_IFACE_DEEP_H */
