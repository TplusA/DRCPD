#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <string.h>
#include <errno.h>

#include "dbus_iface.h"
#include "dbus_iface_deep.h"
#include "dbus_handlers.h"
#include "messages.h"

struct dbus_data
{
    guint owner_id;
    int acquired;

    tdbusdcpdPlayback *dcpd_playback_proxy;
    tdbusdcpdViews *dcpd_views_proxy;
    tdbusdcpdListNavigation *dcpd_list_navigation_proxy;
    tdbusdcpdListItem *dcpd_list_item_proxy;

    tdbuslistsNavigation *filebroker_lists_navigation_proxy;
    tdbuslistsNavigation *tuneinbroker_lists_navigation_proxy;
    tdbuslistsNavigation *upnpbroker_lists_navigation_proxy;

    tdbussplayURLFIFO *splay_urlfifo_proxy;
    tdbussplayPlayback *splay_playback_proxy;
};

static void bus_acquired(GDBusConnection *connection,
                         const gchar *name, gpointer user_data)
{
    msg_info("D-Bus \"%s\" acquired", name);
}


static void handle_error(GError **error)
{
    if(*error != NULL)
    {
        msg_error(0, LOG_EMERG, "%s", (*error)->message);
        g_error_free(*error);
        *error = NULL;
    }
}

static void connect_signals_dcpd(GDBusConnection *connection,
                                 struct dbus_data *data, GDBusProxyFlags flags,
                                 const char *bus_name, const char *object_path)
{
    GError *error = NULL;

    data->dcpd_playback_proxy =
        tdbus_dcpd_playback_proxy_new_sync(connection, flags,
                                           bus_name, object_path,
                                           NULL, &error);
    handle_error(&error);

    data->dcpd_views_proxy =
        tdbus_dcpd_views_proxy_new_sync(connection, flags,
                                        bus_name, object_path,
                                        NULL, &error);
    handle_error(&error);

    data->dcpd_list_navigation_proxy =
        tdbus_dcpd_list_navigation_proxy_new_sync(connection, flags,
                                                  bus_name, object_path,
                                                  NULL, &error);
    handle_error(&error);

    data->dcpd_list_item_proxy =
        tdbus_dcpd_list_item_proxy_new_sync(connection, flags,
                                            bus_name, object_path,
                                            NULL, &error);
    handle_error(&error);
}

static void connect_signals_list_broker(GDBusConnection *connection,
                                        tdbuslistsNavigation **proxy,
                                        GDBusProxyFlags flags,
                                        const char *bus_name,
                                        const char *object_path)
{
    GError *error = NULL;

    *proxy =
        tdbus_lists_navigation_proxy_new_sync(connection, flags,
                                              bus_name, object_path,
                                              NULL, &error);
    handle_error(&error);
}

static void connect_signals_streamplayer(GDBusConnection *connection,
                                         struct dbus_data *data,
                                         GDBusProxyFlags flags,
                                         const char *bus_name,
                                         const char *object_path)
{
    GError *error = NULL;

    data->splay_urlfifo_proxy =
        tdbus_splay_urlfifo_proxy_new_sync(connection, flags,
                                           bus_name, object_path,
                                           NULL, &error);
    handle_error(&error);

    data->splay_playback_proxy =
        tdbus_splay_playback_proxy_new_sync(connection, flags,
                                            bus_name, object_path,
                                            NULL, &error);
    handle_error(&error);
}

static void name_acquired(GDBusConnection *connection,
                          const gchar *name, gpointer user_data)
{
    struct dbus_data *data = user_data;

    msg_info("D-Bus name \"%s\" acquired", name);
    data->acquired = 1;

    connect_signals_dcpd(connection, data, G_DBUS_PROXY_FLAGS_NONE,
                         "de.tahifi.Dcpd", "/de/tahifi/Dcpd");
    connect_signals_list_broker(connection,
                                &data->filebroker_lists_navigation_proxy,
                                G_DBUS_PROXY_FLAGS_NONE,
                                "de.tahifi.FileBroker", "/de/tahifi/FileBroker");
    connect_signals_list_broker(connection,
                                &data->tuneinbroker_lists_navigation_proxy,
                                G_DBUS_PROXY_FLAGS_NONE,
                                "de.tahifi.TuneInBroker", "/de/tahifi/TuneInBroker");
    connect_signals_list_broker(connection,
                                &data->upnpbroker_lists_navigation_proxy,
                                G_DBUS_PROXY_FLAGS_NONE,
                                "de.tahifi.UPnPBroker", "/de/tahifi/UPnPBroker");
    connect_signals_streamplayer(connection, data, G_DBUS_PROXY_FLAGS_NONE,
                                 "de.tahifi.Streamplayer", "/de/tahifi/Streamplayer");
}

static void name_lost(GDBusConnection *connection,
                      const gchar *name, gpointer user_data)
{
    struct dbus_data *data = user_data;

    msg_info("D-Bus name \"%s\" lost", name);
    data->acquired = -1;
}

static void destroy_notification(gpointer data)
{
    msg_info("Bus destroyed.");
}

static struct dbus_data dbus_data;

tdbuslistsNavigation *dbus_get_lists_navigation_iface(dbus_listbroker_id_t listbroker_id)
{
    switch(listbroker_id)
    {
      case DBUS_LISTBROKER_ID_FILESYSTEM:
        return dbus_data.filebroker_lists_navigation_proxy;

      case DBUS_LISTBROKER_ID_TUNEIN:
        return dbus_data.tuneinbroker_lists_navigation_proxy;

      case DBUS_LISTBROKER_ID_UPNP:
        return dbus_data.upnpbroker_lists_navigation_proxy;
    }

    return NULL;
}

tdbussplayURLFIFO *dbus_get_streamplayer_urlfifo_iface(void)
{
    return dbus_data.splay_urlfifo_proxy;
}

tdbussplayPlayback *dbus_get_streamplayer_playback_iface(void)
{
    return dbus_data.splay_playback_proxy;
}

int dbus_setup(GMainLoop *loop, bool connect_to_session_bus,
               void *view_manager_iface_for_dbus_handlers)
{
#if !GLIB_CHECK_VERSION(2, 36, 0)
    g_type_init();
#endif

    memset(&dbus_data, 0, sizeof(dbus_data));

    GBusType bus_type =
        connect_to_session_bus ? G_BUS_TYPE_SESSION : G_BUS_TYPE_SYSTEM;

    static const char bus_name[] = "de.tahifi.Drcpd";

    dbus_data.owner_id =
        g_bus_own_name(bus_type, bus_name, G_BUS_NAME_OWNER_FLAGS_NONE,
                       bus_acquired, name_acquired, name_lost, &dbus_data,
                       destroy_notification);

    while(dbus_data.acquired == 0)
    {
        /* do whatever has to be done behind the scenes until one of the
         * guaranteed callbacks gets called */
        g_main_context_iteration(NULL, TRUE);
    }

    if(dbus_data.acquired < 0)
    {
        msg_error(EPIPE, LOG_EMERG, "Failed acquiring D-Bus name");
        return -1;
    }

    log_assert(dbus_data.dcpd_playback_proxy != NULL);
    log_assert(dbus_data.dcpd_views_proxy != NULL);
    log_assert(dbus_data.dcpd_list_navigation_proxy != NULL);
    log_assert(dbus_data.dcpd_list_item_proxy != NULL);
    log_assert(dbus_data.filebroker_lists_navigation_proxy != NULL);
    log_assert(dbus_data.tuneinbroker_lists_navigation_proxy != NULL);
    log_assert(dbus_data.upnpbroker_lists_navigation_proxy != NULL);
    log_assert(dbus_data.splay_urlfifo_proxy != NULL);
    log_assert(dbus_data.splay_playback_proxy != NULL);

    g_signal_connect(dbus_data.dcpd_playback_proxy, "g-signal",
                     G_CALLBACK(dbussignal_dcpd_playback),
                     view_manager_iface_for_dbus_handlers);

    g_signal_connect(dbus_data.dcpd_views_proxy, "g-signal",
                     G_CALLBACK(dbussignal_dcpd_views),
                     view_manager_iface_for_dbus_handlers);

    g_signal_connect(dbus_data.dcpd_list_navigation_proxy, "g-signal",
                     G_CALLBACK(dbussignal_dcpd_listnav),
                     view_manager_iface_for_dbus_handlers);

    g_signal_connect(dbus_data.dcpd_list_item_proxy, "g-signal",
                     G_CALLBACK(dbussignal_dcpd_listitem),
                     view_manager_iface_for_dbus_handlers);

    g_signal_connect(dbus_data.filebroker_lists_navigation_proxy, "g-signal",
                     G_CALLBACK(dbussignal_lists_navigation),
                     view_manager_iface_for_dbus_handlers);

    g_signal_connect(dbus_data.tuneinbroker_lists_navigation_proxy, "g-signal",
                     G_CALLBACK(dbussignal_lists_navigation),
                     view_manager_iface_for_dbus_handlers);

    g_signal_connect(dbus_data.upnpbroker_lists_navigation_proxy, "g-signal",
                     G_CALLBACK(dbussignal_lists_navigation),
                     view_manager_iface_for_dbus_handlers);

    g_signal_connect(dbus_data.splay_urlfifo_proxy, "g-signal",
                     G_CALLBACK(dbussignal_splay_urlfifo),
                     view_manager_iface_for_dbus_handlers);

    g_signal_connect(dbus_data.splay_playback_proxy, "g-signal",
                     G_CALLBACK(dbussignal_splay_playback),
                     view_manager_iface_for_dbus_handlers);

    g_main_loop_ref(loop);

    return 0;
}

void dbus_shutdown(GMainLoop *loop)
{
    if(loop == NULL)
        return;

    g_bus_unown_name(dbus_data.owner_id);
    g_main_loop_unref(loop);

    g_object_unref(dbus_data.dcpd_playback_proxy);
    g_object_unref(dbus_data.dcpd_views_proxy);
    g_object_unref(dbus_data.dcpd_list_navigation_proxy);
    g_object_unref(dbus_data.dcpd_list_item_proxy);
    g_object_unref(dbus_data.filebroker_lists_navigation_proxy);
    g_object_unref(dbus_data.tuneinbroker_lists_navigation_proxy);
    g_object_unref(dbus_data.upnpbroker_lists_navigation_proxy);
    g_object_unref(dbus_data.splay_urlfifo_proxy);
    g_object_unref(dbus_data.splay_playback_proxy);

}
