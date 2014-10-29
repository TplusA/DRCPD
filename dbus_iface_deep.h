#ifndef DBUS_IFACE_DEEP_H
#define DBUS_IFACE_DEEP_H

#include "dcpd_dbus.h"
#include "lists_dbus.h"
#include "streamplayer_dbus.h"

#ifdef __cplusplus
extern "C" {
#endif

tdbuslistsNavigation *dbus_get_filebroker_lists_navigation_iface(void);

#ifdef __cplusplus
}
#endif

#endif /* !DBUS_IFACE_DEEP_H */
