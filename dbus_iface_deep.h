#ifndef DBUS_IFACE_DEEP_H
#define DBUS_IFACE_DEEP_H

#include "dbus_iface.h"
#include "dcpd_dbus.h"
#include "lists_dbus.h"
#include "streamplayer_dbus.h"

#ifdef __cplusplus
extern "C" {
#endif

tdbuslistsNavigation *dbus_get_lists_navigation_iface(dbus_listbroker_id_t id);
tdbussplayURLFIFO *dbus_get_streamplayer_urlfifo_iface(void);
tdbussplayPlayback *dbus_get_streamplayer_playback_iface(void);

#ifdef __cplusplus
}
#endif

#endif /* !DBUS_IFACE_DEEP_H */
