#ifndef DBUS_IFACE_H
#define DBUS_IFACE_H

#include <stdbool.h>
#include <glib.h>

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
}
dbus_listbroker_id_t;

int dbus_setup(GMainLoop *loop, bool connect_to_session_bus,
               void *view_manager_iface_for_dbus_handlers);
void dbus_shutdown(GMainLoop *loop);

#ifdef __cplusplus
}
#endif

/*!@}*/

#endif /* !DBUS_IFACE_H */
