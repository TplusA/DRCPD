/*
 * Copyright (C) 2015, 2016, 2017  T+A elektroakustik GmbH & Co. KG
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

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <string.h>
#include <errno.h>

#include "dbus_iface.h"
#include "dbus_iface_deep.h"
#include "dbus_handlers.h"
#include "dbus_common.h"
#include "messages.h"
#include "messages_dbus.h"

struct dbus_data
{
    guint owner_id;
    int acquired;

    void *handler_data;

    tdbusdcpdPlayback *dcpd_playback_proxy;
    tdbusdcpdViews *dcpd_views_proxy;
    tdbusdcpdListNavigation *dcpd_list_navigation_proxy;
    tdbusdcpdListItem *dcpd_list_item_proxy;

    tdbuslistsNavigation *filebroker_lists_navigation_proxy;
    tdbuslistsNavigation *tuneinbroker_lists_navigation_proxy;
    tdbuslistsNavigation *upnpbroker_lists_navigation_proxy;

    tdbussplayURLFIFO *splay_urlfifo_proxy;
    tdbussplayPlayback *splay_playback_proxy;

    tdbussplayPlayback *roonplayer_playback_proxy;

    tdbusAirable *airable_sec_proxy;

    tdbusaupathSource *audiopath_source_iface;
    tdbusaupathManager *audiopath_manager_proxy;

    tdbusConfigurationProxy *configuration_proxy;
    tdbusConfigurationRead *configuration_read_iface;
    tdbusConfigurationWrite *configuration_write_iface;

    tdbusdebugLogging *debug_logging_iface;
    tdbusdebugLoggingConfig *debug_logging_config_proxy;
};

struct dbus_process_data
{
    GThread *thread;
    GMainLoop *loop;
    GMainContext *ctx;
};

static gpointer process_dbus(gpointer user_data)
{
    struct dbus_process_data *data = user_data;

    log_assert(data->loop != NULL);

    g_main_context_push_thread_default(data->ctx);
    g_main_loop_run(data->loop);

    return NULL;
}

static void try_export_iface(GDBusConnection *connection,
                             GDBusInterfaceSkeleton *iface)
{
    GError *error = NULL;

    g_dbus_interface_skeleton_export(iface, connection, "/de/tahifi/Drcpd", &error);

    dbus_common_handle_error(&error, "Export interface");
}

static void bus_acquired(GDBusConnection *connection,
                         const gchar *name, gpointer user_data)
{
    struct dbus_data *data = user_data;

    msg_info("D-Bus \"%s\" acquired", name);

    data->audiopath_source_iface = tdbus_aupath_source_skeleton_new();
    data->configuration_read_iface = tdbus_configuration_read_skeleton_new();
    data->configuration_write_iface = tdbus_configuration_write_skeleton_new();
    data->debug_logging_iface = tdbus_debug_logging_skeleton_new();

    g_signal_connect(data->audiopath_source_iface, "handle-selected",
                     G_CALLBACK(dbusmethod_audiopath_source_selected), data->handler_data);
    g_signal_connect(data->audiopath_source_iface, "handle-deselected",
                     G_CALLBACK(dbusmethod_audiopath_source_deselected), data->handler_data);

    g_signal_connect(data->configuration_read_iface, "handle-get-all-keys",
                     G_CALLBACK(dbusmethod_config_get_all_keys), data->handler_data);
    g_signal_connect(data->configuration_read_iface, "handle-get-value",
                     G_CALLBACK(dbusmethod_config_get_value), data->handler_data);
    g_signal_connect(data->configuration_read_iface, "handle-get-all-values",
                     G_CALLBACK(dbusmethod_config_get_all_values), data->handler_data);

    g_signal_connect(data->configuration_write_iface, "handle-set-value",
                     G_CALLBACK(dbusmethod_config_set_value), data->handler_data);
    g_signal_connect(data->configuration_write_iface, "handle-set-multiple-values",
                     G_CALLBACK(dbusmethod_config_set_multiple_values), data->handler_data);

    g_signal_connect(data->debug_logging_iface,
                     "handle-debug-level",
                     G_CALLBACK(msg_dbus_handle_debug_level), NULL);

    try_export_iface(connection, G_DBUS_INTERFACE_SKELETON(data->audiopath_source_iface));
    try_export_iface(connection, G_DBUS_INTERFACE_SKELETON(data->configuration_read_iface));
    try_export_iface(connection, G_DBUS_INTERFACE_SKELETON(data->configuration_write_iface));
    try_export_iface(connection, G_DBUS_INTERFACE_SKELETON(data->debug_logging_iface));
}

static void created_debug_config_proxy(GObject *source_object, GAsyncResult *res,
                                       gpointer user_data)
{
    struct dbus_data *data = user_data;
    GError *error = NULL;

    data->debug_logging_config_proxy =
        tdbus_debug_logging_config_proxy_new_finish(res, &error);

    if(dbus_common_handle_error(&error, "Create debug logging proxy") == 0)
        g_signal_connect(data->debug_logging_config_proxy, "g-signal",
                         G_CALLBACK(msg_dbus_handle_global_debug_level_changed),
                         NULL);
}

static void created_configuration_proxy(GObject *source_object, GAsyncResult *res,
                                        gpointer user_data)
{
    struct dbus_data *data = user_data;
    GError *error = NULL;

    data->configuration_proxy =
        tdbus_configuration_proxy_proxy_new_finish(res, &error);

    if(dbus_common_handle_error(&error, "Create configuration proxy") == 0)
        tdbus_configuration_proxy_call_register(data->configuration_proxy,
                                                "drcpd", "/de/tahifi/Drcpd",
                                                NULL, NULL, NULL);
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
    dbus_common_handle_error(&error, "Create playback proxy");

    data->dcpd_views_proxy =
        tdbus_dcpd_views_proxy_new_sync(connection, flags,
                                        bus_name, object_path,
                                        NULL, &error);
    dbus_common_handle_error(&error, "Create views proxy");

    data->dcpd_list_navigation_proxy =
        tdbus_dcpd_list_navigation_proxy_new_sync(connection, flags,
                                                  bus_name, object_path,
                                                  NULL, &error);
    dbus_common_handle_error(&error, "Create own navigation proxy");

    data->dcpd_list_item_proxy =
        tdbus_dcpd_list_item_proxy_new_sync(connection, flags,
                                            bus_name, object_path,
                                            NULL, &error);
    dbus_common_handle_error(&error, "Create list item proxy");

    data->debug_logging_config_proxy = NULL;
    tdbus_debug_logging_config_proxy_new(connection, flags,
                                         bus_name, object_path, NULL,
                                         created_debug_config_proxy, data);

    data->configuration_proxy = NULL;
    tdbus_configuration_proxy_proxy_new(connection, flags,
                                        bus_name, object_path, NULL,
                                        created_configuration_proxy, data);
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
    dbus_common_handle_error(&error, "Create list broker navigation proxy");
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
    dbus_common_handle_error(&error, "Create URL FIFO proxy");

    data->splay_playback_proxy =
        tdbus_splay_playback_proxy_new_sync(connection, flags,
                                            bus_name, object_path,
                                            NULL, &error);
    dbus_common_handle_error(&error, "Create stream player proxy");
}

static void connect_signals_roonplayer(GDBusConnection *connection,
                                       struct dbus_data *data,
                                       GDBusProxyFlags flags,
                                       const char *bus_name,
                                       const char *object_path)
{
    GError *error = NULL;

    data->roonplayer_playback_proxy =
        tdbus_splay_playback_proxy_new_sync(connection, flags,
                                            bus_name, object_path,
                                            NULL, &error);
    dbus_common_handle_error(&error, "Create Roon player proxy");
}

static void connect_signals_airable(GDBusConnection *connection,
                                    tdbusAirable **proxy,
                                    GDBusProxyFlags flags,
                                    const char *bus_name,
                                    const char *object_path)
{
    GError *error = NULL;

    *proxy =
        tdbus_airable_proxy_new_sync(connection, flags,
                                     bus_name, object_path,
                                     NULL, &error);
    dbus_common_handle_error(&error, "Create Airable proxy");
}

static void connect_signals_audiopath(GDBusConnection *connection,
                                      tdbusaupathManager **proxy,
                                      GDBusProxyFlags flags,
                                      const char *bus_name,
                                      const char *object_path)
{
    GError *error = NULL;

    *proxy =
        tdbus_aupath_manager_proxy_new_sync(connection, flags,
                                            bus_name, object_path,
                                            NULL, &error);
    dbus_common_handle_error(&error, "Create audio path manager proxy");
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
    connect_signals_roonplayer(connection, data, G_DBUS_PROXY_FLAGS_NONE,
                               "de.tahifi.Roon", "/de/tahifi/Roon");
    connect_signals_airable(connection, &data->airable_sec_proxy,
                            G_DBUS_PROXY_FLAGS_NONE,
                            "de.tahifi.TuneInBroker", "/de/tahifi/TuneInBroker");
    connect_signals_audiopath(connection, &data->audiopath_manager_proxy,
                              G_DBUS_PROXY_FLAGS_NONE,
                              "de.tahifi.TAPSwitch", "/de/tahifi/TAPSwitch");
}

static void name_lost(GDBusConnection *connection,
                      const gchar *name, gpointer user_data)
{
    struct dbus_data *data = user_data;

    msg_vinfo(MESSAGE_LEVEL_IMPORTANT, "D-Bus name \"%s\" lost", name);
    data->acquired = -1;
}

static void destroy_notification(gpointer data)
{
    msg_vinfo(MESSAGE_LEVEL_IMPORTANT, "Bus destroyed.");
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

tdbussplayPlayback *dbus_get_roonplayer_playback_iface(void)
{
    return dbus_data.roonplayer_playback_proxy;
}

tdbusdcpdPlayback *dbus_get_dcpd_playback_iface(void)
{
    return dbus_data.dcpd_playback_proxy;
}

tdbusAirable *dbus_get_airable_sec_iface(void)
{
    return dbus_data.airable_sec_proxy;
}

tdbusaupathManager *dbus_audiopath_get_manager_iface(void)
{
    return dbus_data.audiopath_manager_proxy;
}

static struct dbus_process_data process_data;

int dbus_setup(bool connect_to_session_bus,
               void *dbus_signal_data_for_dbus_handlers)
{
#if !GLIB_CHECK_VERSION(2, 36, 0)
    g_type_init();
#endif

    memset(&dbus_data, 0, sizeof(dbus_data));
    memset(&process_data, 0, sizeof(process_data));

    process_data.ctx = g_main_context_new();
    process_data.loop = g_main_loop_new(process_data.ctx, FALSE);
    if(process_data.loop == NULL)
    {
        msg_error(ENOMEM, LOG_EMERG, "Failed creating GLib main loop");
        return -1;
    }

    g_main_context_push_thread_default(process_data.ctx);

    GBusType bus_type =
        connect_to_session_bus ? G_BUS_TYPE_SESSION : G_BUS_TYPE_SYSTEM;

    static const char bus_name[] = "de.tahifi.Drcpd";

    dbus_data.handler_data = dbus_signal_data_for_dbus_handlers;
    dbus_data.owner_id =
        g_bus_own_name(bus_type, bus_name, G_BUS_NAME_OWNER_FLAGS_NONE,
                       bus_acquired, name_acquired, name_lost, &dbus_data,
                       destroy_notification);

    while(dbus_data.acquired == 0)
    {
        /* do whatever has to be done behind the scenes until one of the
         * guaranteed callbacks gets called */
        g_main_context_iteration(process_data.ctx, TRUE);
    }

    if(dbus_data.acquired < 0)
    {
        msg_error(0, LOG_EMERG, "Failed acquiring D-Bus name");
        g_main_context_pop_thread_default(process_data.ctx);
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
    log_assert(dbus_data.roonplayer_playback_proxy != NULL);
    log_assert(dbus_data.airable_sec_proxy != NULL);
    log_assert(dbus_data.audiopath_source_iface != NULL);
    log_assert(dbus_data.audiopath_manager_proxy != NULL);
    log_assert(dbus_data.configuration_read_iface != NULL);
    log_assert(dbus_data.configuration_write_iface != NULL);
    log_assert(dbus_data.debug_logging_iface != NULL);

    g_signal_connect(dbus_data.dcpd_playback_proxy, "g-signal",
                     G_CALLBACK(dbussignal_dcpd_playback),
                     dbus_signal_data_for_dbus_handlers);

    g_signal_connect(dbus_data.dcpd_views_proxy, "g-signal",
                     G_CALLBACK(dbussignal_dcpd_views),
                     dbus_signal_data_for_dbus_handlers);

    g_signal_connect(dbus_data.dcpd_list_navigation_proxy, "g-signal",
                     G_CALLBACK(dbussignal_dcpd_listnav),
                     dbus_signal_data_for_dbus_handlers);

    g_signal_connect(dbus_data.dcpd_list_item_proxy, "g-signal",
                     G_CALLBACK(dbussignal_dcpd_listitem),
                     dbus_signal_data_for_dbus_handlers);

    g_signal_connect(dbus_data.filebroker_lists_navigation_proxy, "g-signal",
                     G_CALLBACK(dbussignal_lists_navigation),
                     dbus_signal_data_for_dbus_handlers);

    g_signal_connect(dbus_data.tuneinbroker_lists_navigation_proxy, "g-signal",
                     G_CALLBACK(dbussignal_lists_navigation),
                     dbus_signal_data_for_dbus_handlers);

    g_signal_connect(dbus_data.upnpbroker_lists_navigation_proxy, "g-signal",
                     G_CALLBACK(dbussignal_lists_navigation),
                     dbus_signal_data_for_dbus_handlers);

    g_signal_connect(dbus_data.splay_urlfifo_proxy, "g-signal",
                     G_CALLBACK(dbussignal_splay_urlfifo),
                     dbus_signal_data_for_dbus_handlers);

    g_signal_connect(dbus_data.splay_playback_proxy, "g-signal",
                     G_CALLBACK(dbussignal_splay_playback),
                     dbus_signal_data_for_dbus_handlers);

    g_signal_connect(dbus_data.roonplayer_playback_proxy, "g-signal",
                     G_CALLBACK(dbussignal_splay_playback),
                     dbus_signal_data_for_dbus_handlers);

    g_signal_connect(dbus_data.airable_sec_proxy, "g-signal",
                     G_CALLBACK(dbussignal_airable_sec),
                     dbus_signal_data_for_dbus_handlers);

    g_signal_connect(dbus_data.audiopath_manager_proxy, "g-signal",
                     G_CALLBACK(dbussignal_audiopath_manager),
                     dbus_signal_data_for_dbus_handlers);

    process_data.thread = g_thread_new("D-Bus I/O", process_dbus, &process_data);

    if(process_data.thread == NULL)
        msg_error(EAGAIN, LOG_EMERG, "Failed spawning D-Bus I/O thread");

    g_main_context_pop_thread_default(process_data.ctx);

    return (process_data.thread != NULL) ? 0 : -1;
}

void dbus_shutdown(void)
{
    if(process_data.loop == NULL)
        return;

    g_bus_unown_name(dbus_data.owner_id);

    g_main_loop_quit(process_data.loop);
    if(process_data.thread != NULL)
        (void)g_thread_join(process_data.thread);
    g_main_loop_unref(process_data.loop);

    g_object_unref(dbus_data.dcpd_playback_proxy);
    g_object_unref(dbus_data.dcpd_views_proxy);
    g_object_unref(dbus_data.dcpd_list_navigation_proxy);
    g_object_unref(dbus_data.dcpd_list_item_proxy);
    g_object_unref(dbus_data.filebroker_lists_navigation_proxy);
    g_object_unref(dbus_data.tuneinbroker_lists_navigation_proxy);
    g_object_unref(dbus_data.upnpbroker_lists_navigation_proxy);
    g_object_unref(dbus_data.splay_urlfifo_proxy);
    g_object_unref(dbus_data.splay_playback_proxy);
    g_object_unref(dbus_data.roonplayer_playback_proxy);
    g_object_unref(dbus_data.airable_sec_proxy);
    g_object_unref(dbus_data.audiopath_source_iface);
    g_object_unref(dbus_data.audiopath_manager_proxy);
    g_object_unref(dbus_data.configuration_read_iface);
    g_object_unref(dbus_data.configuration_write_iface);
    g_object_unref(dbus_data.debug_logging_iface);

    if(dbus_data.debug_logging_config_proxy != NULL)
        g_object_unref(dbus_data.debug_logging_config_proxy);

    if(dbus_data.configuration_proxy != NULL)
        g_object_unref(dbus_data.configuration_proxy);

    process_data.loop = NULL;
}
