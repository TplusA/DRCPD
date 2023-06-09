/*
 * Copyright (C) 2015--2019, 2021, 2022  T+A elektroakustik GmbH & Co. KG
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

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <string.h>
#include <errno.h>

#include "dbus_iface.hh"
#include "dbus_iface_proxies.hh"
#include "dbus_handlers.h"
#include "de_tahifi_errors.h"
#include "gerrorwrapper.hh"
#include "messages.h"
#include "messages_dbus.h"
#include "logged_lock.hh"

struct DBusData
{
    guint owner_id;
    int acquired;

    void *handler_data;

    tdbusdcpdPlayback *dcpd_playback_proxy;
    tdbusdcpdViews *dcpd_views_proxy;
    tdbusdcpdListNavigation *dcpd_list_navigation_proxy;
    tdbusdcpdListItem *dcpd_list_item_proxy;

    tdbuslistsNavigation *filebroker_lists_navigation_proxy;
    tdbuslistsNavigation *airablebroker_lists_navigation_proxy;
    tdbuslistsNavigation *upnpbroker_lists_navigation_proxy;

    tdbussplayURLFIFO *splay_urlfifo_proxy;
    tdbussplayPlayback *splay_playback_proxy;

    tdbussplayPlayback *roonplayer_playback_proxy;

    tdbusAirable *airable_sec_proxy;
    tdbusErrors *airable_errors_proxy;

    tdbusdcpdPlayback *rest_dcpd_playback_proxy;
    tdbusJSONEmitter *rest_display_updates_proxy;

    tdbusaupathSource *audiopath_source_iface;
    tdbusaupathManager *audiopath_manager_proxy;

    guint bus_watch_dcpd;
    tdbusConfigurationProxy *configuration_proxy;
    tdbusConfigurationRead *configuration_read_iface;
    tdbusConfigurationWrite *configuration_write_iface;

    tdbusdebugLogging *debug_logging_iface;
    tdbusdebugLoggingConfig *debug_logging_config_proxy;
};

struct ProcessData
{
    GThread *thread;
    GMainLoop *loop;
    GMainContext *ctx;
};

static gpointer process_dbus(gpointer user_data)
{
    LoggedLock::set_context_name("D-Bus I/O");

    auto &data = *static_cast<ProcessData *>(user_data);

    msg_log_assert(data.loop != nullptr);

    while(!g_main_context_acquire(data.ctx))
        ;

    g_main_context_push_thread_default(data.ctx);
    g_main_loop_run(data.loop);

    return nullptr;
}

static void try_export_iface(GDBusConnection *connection,
                             GDBusInterfaceSkeleton *iface)
{
    GErrorWrapper error;
    g_dbus_interface_skeleton_export(iface, connection, "/de/tahifi/Drcpd",
                                     error.await());
    error.log_failure("Export interface");
}

static void bus_acquired(GDBusConnection *connection,
                         const gchar *name, gpointer user_data)
{
    auto &data = *static_cast<DBusData *>(user_data);

    msg_info("D-Bus \"%s\" acquired", name);

    data.audiopath_source_iface = tdbus_aupath_source_skeleton_new();
    data.configuration_read_iface = tdbus_configuration_read_skeleton_new();
    data.configuration_write_iface = tdbus_configuration_write_skeleton_new();
    data.debug_logging_iface = tdbus_debug_logging_skeleton_new();

    g_signal_connect(data.audiopath_source_iface, "handle-selected-on-hold",
                     G_CALLBACK(dbusmethod_audiopath_source_selected_on_hold), data.handler_data);
    g_signal_connect(data.audiopath_source_iface, "handle-selected",
                     G_CALLBACK(dbusmethod_audiopath_source_selected), data.handler_data);
    g_signal_connect(data.audiopath_source_iface, "handle-deselected",
                     G_CALLBACK(dbusmethod_audiopath_source_deselected), data.handler_data);

    g_signal_connect(data.configuration_read_iface, "handle-get-all-keys",
                     G_CALLBACK(dbusmethod_config_get_all_keys), data.handler_data);
    g_signal_connect(data.configuration_read_iface, "handle-get-value",
                     G_CALLBACK(dbusmethod_config_get_value), data.handler_data);
    g_signal_connect(data.configuration_read_iface, "handle-get-all-values",
                     G_CALLBACK(dbusmethod_config_get_all_values), data.handler_data);

    g_signal_connect(data.configuration_write_iface, "handle-set-value",
                     G_CALLBACK(dbusmethod_config_set_value), data.handler_data);
    g_signal_connect(data.configuration_write_iface, "handle-set-multiple-values",
                     G_CALLBACK(dbusmethod_config_set_multiple_values), data.handler_data);

    g_signal_connect(data.debug_logging_iface,
                     "handle-debug-level",
                     G_CALLBACK(msg_dbus_handle_debug_level), nullptr);

    try_export_iface(connection, G_DBUS_INTERFACE_SKELETON(data.audiopath_source_iface));
    try_export_iface(connection, G_DBUS_INTERFACE_SKELETON(data.configuration_read_iface));
    try_export_iface(connection, G_DBUS_INTERFACE_SKELETON(data.configuration_write_iface));
    try_export_iface(connection, G_DBUS_INTERFACE_SKELETON(data.debug_logging_iface));
}

static void created_debug_config_proxy(GObject *source_object, GAsyncResult *res,
                                       gpointer user_data)
{
    auto &data = *static_cast<DBusData *>(user_data);
    GErrorWrapper error;

    data.debug_logging_config_proxy =
        tdbus_debug_logging_config_proxy_new_finish(res, error.await());

    if(!error.log_failure("Create debug logging proxy"))
        g_signal_connect(data.debug_logging_config_proxy, "g-signal",
                         G_CALLBACK(msg_dbus_handle_global_debug_level_changed),
                         nullptr);
}

static void dcpd_appeared(GDBusConnection *connection, const gchar *name,
                          const gchar *name_owner, gpointer user_data)
{
    auto &data = *static_cast<DBusData *>(user_data);
    GErrorWrapper error;

    tdbus_configuration_proxy_call_register(data.configuration_proxy,
                                            "drcpd", "/de/tahifi/Drcpd",
                                            nullptr, nullptr, error.await());
    error.log_failure("Register configuration proxy");

    g_bus_unwatch_name(data.bus_watch_dcpd);
    data.bus_watch_dcpd = 0;
}

static void connect_signals_dcpd(GDBusConnection *connection,
                                 DBusData &data, GDBusProxyFlags flags,
                                 const char *bus_name, const char *object_path)
{
    GErrorWrapper error;

    data.dcpd_playback_proxy =
        tdbus_dcpd_playback_proxy_new_sync(connection, flags,
                                           bus_name, object_path,
                                           nullptr, error.await());
    error.log_failure("Create playback proxy");

    data.dcpd_views_proxy =
        tdbus_dcpd_views_proxy_new_sync(connection, flags,
                                        bus_name, object_path,
                                        nullptr, error.await());
    error.log_failure("Create views proxy");

    data.dcpd_list_navigation_proxy =
        tdbus_dcpd_list_navigation_proxy_new_sync(connection, flags,
                                                  bus_name, object_path,
                                                  nullptr, error.await());
    error.log_failure("Create own navigation proxy");

    data.dcpd_list_item_proxy =
        tdbus_dcpd_list_item_proxy_new_sync(connection, flags,
                                            bus_name, object_path,
                                            nullptr, error.await());
    error.log_failure("Create list item proxy");

    data.configuration_proxy =
        tdbus_configuration_proxy_proxy_new_sync(connection, flags,
                                                 bus_name, object_path,
                                                 nullptr, error.await());
    error.log_failure("Create configuration proxy");

    data.debug_logging_config_proxy = nullptr;
    tdbus_debug_logging_config_proxy_new(connection, flags,
                                         bus_name, object_path, nullptr,
                                         created_debug_config_proxy, &data);

    data.bus_watch_dcpd =
        g_bus_watch_name_on_connection(connection, bus_name,
                                       G_BUS_NAME_WATCHER_FLAGS_NONE,
                                       dcpd_appeared, nullptr,
                                       &data, nullptr);
}

static void connect_signals_rest_api(GDBusConnection *connection,
                                     DBusData &data, GDBusProxyFlags flags,
                                     const char *bus_name,
                                     const char *object_path_dcpd,
                                     const char *object_path_display)
{
    GErrorWrapper error;

    data.rest_dcpd_playback_proxy =
        tdbus_dcpd_playback_proxy_new_sync(connection, flags,
                                           bus_name, object_path_dcpd,
                                           nullptr, error.await());
    error.log_failure("Create REST playback proxy");

    data.rest_display_updates_proxy =
        tdbus_jsonemitter_proxy_new_sync(connection, flags,
                                         bus_name, object_path_display,
                                         nullptr, error.await());
    error.log_failure("Create REST playback proxy");
}

static void connect_signals_list_broker(GDBusConnection *connection,
                                        tdbuslistsNavigation *&proxy,
                                        GDBusProxyFlags flags,
                                        const char *bus_name,
                                        const char *object_path)
{
    GErrorWrapper error;

    proxy = tdbus_lists_navigation_proxy_new_sync(connection, flags,
                                                  bus_name, object_path,
                                                  nullptr, error.await());
    error.log_failure("Create list broker navigation proxy");
}

static void connect_signals_streamplayer(GDBusConnection *connection,
                                         DBusData &data,
                                         GDBusProxyFlags flags,
                                         const char *bus_name,
                                         const char *object_path)
{
    GErrorWrapper error;

    data.splay_urlfifo_proxy =
        tdbus_splay_urlfifo_proxy_new_sync(connection, flags,
                                           bus_name, object_path,
                                           nullptr, error.await());
    error.log_failure("Create URL FIFO proxy");

    data.splay_playback_proxy =
        tdbus_splay_playback_proxy_new_sync(connection, flags,
                                            bus_name, object_path,
                                            nullptr, error.await());
    error.log_failure("Create stream player proxy");
}

static void connect_signals_roonplayer(GDBusConnection *connection,
                                       DBusData &data,
                                       GDBusProxyFlags flags,
                                       const char *bus_name,
                                       const char *object_path)
{
    GErrorWrapper error;

    data.roonplayer_playback_proxy =
        tdbus_splay_playback_proxy_new_sync(connection, flags,
                                            bus_name, object_path,
                                            nullptr, error.await());
    error.log_failure("Create Roon player proxy");
}

static void connect_signals_airable(GDBusConnection *connection,
                                    tdbusAirable *&proxy,
                                    GDBusProxyFlags flags,
                                    const char *bus_name,
                                    const char *object_path)
{
    GErrorWrapper error;

    proxy = tdbus_airable_proxy_new_sync(connection, flags,
                                         bus_name, object_path,
                                         nullptr, error.await());
    error.log_failure("Create Airable proxy");
}

static void connect_signals_audiopath(GDBusConnection *connection,
                                      tdbusaupathManager *&proxy,
                                      GDBusProxyFlags flags,
                                      const char *bus_name,
                                      const char *object_path)
{
    GErrorWrapper error;

    proxy = tdbus_aupath_manager_proxy_new_sync(connection, flags,
                                                bus_name, object_path,
                                                nullptr, error.await());
    error.log_failure("Create audio path manager proxy");
}

static void connect_signals_errors(GDBusConnection *connection,
                                   tdbusErrors *&proxy,
                                   GDBusProxyFlags flags,
                                   const char *bus_name,
                                   const char *object_path)
{
    GErrorWrapper error;

    proxy = tdbus_errors_proxy_new_sync(connection, flags,
                                        bus_name, object_path,
                                        nullptr, error.await());
    error.log_failure("Create Errors proxy");
}

static void name_acquired(GDBusConnection *connection,
                          const gchar *name, gpointer user_data)
{
    auto &data = *static_cast<DBusData *>(user_data);

    msg_info("D-Bus name \"%s\" acquired", name);
    data.acquired = 1;

    connect_signals_dcpd(connection, data, G_DBUS_PROXY_FLAGS_NONE,
                         "de.tahifi.Dcpd", "/de/tahifi/Dcpd");
    connect_signals_rest_api(connection, data, G_DBUS_PROXY_FLAGS_NONE,
                             "de.tahifi.REST",
                             "/de/tahifi/REST_DCPD",
                             "/de/tahifi/REST_DISPLAY");
    connect_signals_list_broker(connection,
                                data.filebroker_lists_navigation_proxy,
                                G_DBUS_PROXY_FLAGS_NONE,
                                "de.tahifi.FileBroker", "/de/tahifi/FileBroker");
    connect_signals_list_broker(connection,
                                data.airablebroker_lists_navigation_proxy,
                                G_DBUS_PROXY_FLAGS_NONE,
                                "de.tahifi.TuneInBroker", "/de/tahifi/TuneInBroker");
    connect_signals_list_broker(connection,
                                data.upnpbroker_lists_navigation_proxy,
                                G_DBUS_PROXY_FLAGS_NONE,
                                "de.tahifi.UPnPBroker", "/de/tahifi/UPnPBroker");
    connect_signals_streamplayer(connection, data, G_DBUS_PROXY_FLAGS_NONE,
                                 "de.tahifi.Streamplayer", "/de/tahifi/Streamplayer");
    connect_signals_roonplayer(connection, data, G_DBUS_PROXY_FLAGS_NONE,
                               "de.tahifi.Roon", "/de/tahifi/Roon");
    connect_signals_airable(connection, data.airable_sec_proxy,
                            G_DBUS_PROXY_FLAGS_NONE,
                            "de.tahifi.TuneInBroker", "/de/tahifi/TuneInBroker");
    connect_signals_errors(connection, data.airable_errors_proxy,
                           G_DBUS_PROXY_FLAGS_NONE,
                           "de.tahifi.TuneInBroker", "/de/tahifi/TuneInBroker");
    connect_signals_audiopath(connection, data.audiopath_manager_proxy,
                              G_DBUS_PROXY_FLAGS_NONE,
                              "de.tahifi.TAPSwitch", "/de/tahifi/TAPSwitch");
}

static void name_lost(GDBusConnection *connection,
                      const gchar *name, gpointer user_data)
{
    auto &data = *static_cast<DBusData *>(user_data);

    msg_vinfo(MESSAGE_LEVEL_IMPORTANT, "D-Bus name \"%s\" lost", name);
    data.acquired = -1;
}

static void destroy_notification(gpointer data)
{
    msg_vinfo(MESSAGE_LEVEL_IMPORTANT, "Bus destroyed.");
}

static DBusData dbus_data;

tdbuslistsNavigation *DBus::get_lists_navigation_iface(DBus::ListbrokerID listbroker_id)
{
    switch(listbroker_id)
    {
      case DBus::ListbrokerID::FILESYSTEM:
        return dbus_data.filebroker_lists_navigation_proxy;

      case DBus::ListbrokerID::AIRABLE:
        return dbus_data.airablebroker_lists_navigation_proxy;

      case DBus::ListbrokerID::UPNP:
        return dbus_data.upnpbroker_lists_navigation_proxy;
    }

    return nullptr;
}

tdbussplayURLFIFO *DBus::get_streamplayer_urlfifo_iface()
{
    return dbus_data.splay_urlfifo_proxy;
}

tdbussplayPlayback *DBus::get_streamplayer_playback_iface()
{
    return dbus_data.splay_playback_proxy;
}

tdbussplayPlayback *DBus::get_roonplayer_playback_iface()
{
    return dbus_data.roonplayer_playback_proxy;
}

tdbusdcpdPlayback *DBus::get_dcpd_playback_iface()
{
    return dbus_data.dcpd_playback_proxy;
}

tdbusAirable *DBus::get_airable_sec_iface()
{
    return dbus_data.airable_sec_proxy;
}

tdbusdcpdPlayback *DBus::get_rest_dcpd_playback_iface()
{
    return dbus_data.rest_dcpd_playback_proxy;
}

tdbusJSONEmitter *DBus::get_rest_display_updates_iface()
{
    return dbus_data.rest_display_updates_proxy;
}

tdbusaupathManager *DBus::audiopath_get_manager_iface()
{
    return dbus_data.audiopath_manager_proxy;
}

static ProcessData process_data;

int DBus::setup(bool connect_to_session_bus,
                void *dbus_signal_data_for_dbus_handlers)
{
#if !GLIB_CHECK_VERSION(2, 36, 0)
    g_type_init();
#endif

    memset(&dbus_data, 0, sizeof(dbus_data));
    memset(&process_data, 0, sizeof(process_data));

    process_data.ctx = g_main_context_new();
    process_data.loop = g_main_loop_new(process_data.ctx, FALSE);
    if(process_data.loop == nullptr)
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

    msg_log_assert(dbus_data.dcpd_playback_proxy != nullptr);
    msg_log_assert(dbus_data.dcpd_views_proxy != nullptr);
    msg_log_assert(dbus_data.dcpd_list_navigation_proxy != nullptr);
    msg_log_assert(dbus_data.dcpd_list_item_proxy != nullptr);
    msg_log_assert(dbus_data.filebroker_lists_navigation_proxy != nullptr);
    msg_log_assert(dbus_data.airablebroker_lists_navigation_proxy != nullptr);
    msg_log_assert(dbus_data.upnpbroker_lists_navigation_proxy != nullptr);
    msg_log_assert(dbus_data.splay_urlfifo_proxy != nullptr);
    msg_log_assert(dbus_data.splay_playback_proxy != nullptr);
    msg_log_assert(dbus_data.roonplayer_playback_proxy != nullptr);
    msg_log_assert(dbus_data.airable_sec_proxy != nullptr);
    msg_log_assert(dbus_data.airable_errors_proxy != nullptr);
    msg_log_assert(dbus_data.rest_dcpd_playback_proxy != nullptr);
    msg_log_assert(dbus_data.rest_display_updates_proxy != nullptr);
    msg_log_assert(dbus_data.audiopath_source_iface != nullptr);
    msg_log_assert(dbus_data.audiopath_manager_proxy != nullptr);
    msg_log_assert(dbus_data.configuration_read_iface != nullptr);
    msg_log_assert(dbus_data.configuration_write_iface != nullptr);
    msg_log_assert(dbus_data.debug_logging_iface != nullptr);

    g_signal_connect(dbus_data.dcpd_playback_proxy, "g-signal",
                     G_CALLBACK(dbussignal_dcpd_playback_from_dcpd),
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

    g_signal_connect(dbus_data.airablebroker_lists_navigation_proxy, "g-signal",
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

    g_signal_connect(dbus_data.airable_errors_proxy, "g-signal",
                     G_CALLBACK(dbussignal_error_messages),
                     dbus_signal_data_for_dbus_handlers);

    g_signal_connect(dbus_data.rest_dcpd_playback_proxy, "g-signal",
                     G_CALLBACK(dbussignal_dcpd_playback_from_rest),
                     dbus_signal_data_for_dbus_handlers);

    g_signal_connect(dbus_data.rest_display_updates_proxy, "g-signal",
                     G_CALLBACK(dbussignal_rest_display_updates),
                     dbus_signal_data_for_dbus_handlers);

    g_signal_connect(dbus_data.audiopath_manager_proxy, "g-signal",
                     G_CALLBACK(dbussignal_audiopath_manager),
                     dbus_signal_data_for_dbus_handlers);

    process_data.thread = g_thread_new("D-Bus I/O", process_dbus, &process_data);

    if(process_data.thread == nullptr)
        msg_error(EAGAIN, LOG_EMERG, "Failed spawning D-Bus I/O thread");

    g_main_context_pop_thread_default(process_data.ctx);

    return (process_data.thread != nullptr) ? 0 : -1;
}

void DBus::shutdown()
{
    if(process_data.loop == nullptr)
        return;

    if(dbus_data.bus_watch_dcpd != 0)
    {
        g_bus_unwatch_name(dbus_data.bus_watch_dcpd);
        dbus_data.bus_watch_dcpd = 0;
    }

    g_bus_unown_name(dbus_data.owner_id);

    g_main_loop_quit(process_data.loop);
    if(process_data.thread != nullptr)
        (void)g_thread_join(process_data.thread);
    g_main_loop_unref(process_data.loop);

    g_object_unref(dbus_data.dcpd_playback_proxy);
    g_object_unref(dbus_data.dcpd_views_proxy);
    g_object_unref(dbus_data.dcpd_list_navigation_proxy);
    g_object_unref(dbus_data.dcpd_list_item_proxy);
    g_object_unref(dbus_data.filebroker_lists_navigation_proxy);
    g_object_unref(dbus_data.airablebroker_lists_navigation_proxy);
    g_object_unref(dbus_data.upnpbroker_lists_navigation_proxy);
    g_object_unref(dbus_data.splay_urlfifo_proxy);
    g_object_unref(dbus_data.splay_playback_proxy);
    g_object_unref(dbus_data.roonplayer_playback_proxy);
    g_object_unref(dbus_data.airable_sec_proxy);
    g_object_unref(dbus_data.airable_errors_proxy);
    g_object_unref(dbus_data.rest_dcpd_playback_proxy);
    g_object_unref(dbus_data.rest_display_updates_proxy);
    g_object_unref(dbus_data.audiopath_source_iface);
    g_object_unref(dbus_data.audiopath_manager_proxy);
    g_object_unref(dbus_data.configuration_read_iface);
    g_object_unref(dbus_data.configuration_write_iface);
    g_object_unref(dbus_data.debug_logging_iface);

    if(dbus_data.debug_logging_config_proxy != nullptr)
        g_object_unref(dbus_data.debug_logging_config_proxy);

    if(dbus_data.configuration_proxy != nullptr)
        g_object_unref(dbus_data.configuration_proxy);

    process_data.loop = nullptr;
}
