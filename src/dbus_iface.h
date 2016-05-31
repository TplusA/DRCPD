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

#ifndef DBUS_IFACE_H
#define DBUS_IFACE_H

#include <stdbool.h>

/*!
 * \addtogroup dbus DBus handling
 */
/*!@{*/

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    DBUS_LISTBROKER_ID_FILESYSTEM,
    DBUS_LISTBROKER_ID_TUNEIN,
    DBUS_LISTBROKER_ID_UPNP,
}
dbus_listbroker_id_t;

int dbus_setup(bool connect_to_session_bus,
               void *dbus_signal_data_for_dbus_handlers);
void dbus_shutdown(void);

#ifdef __cplusplus
}
#endif

/*!@}*/

#endif /* !DBUS_IFACE_H */
