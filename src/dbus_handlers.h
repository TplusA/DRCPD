/*
 * Copyright (C) 2015--2019  T+A elektroakustik GmbH & Co. KG
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

#ifndef DBUS_HANDLERS_H
#define DBUS_HANDLERS_H

#include <gio/gio.h>

#include "de_tahifi_audiopath.h"
#include "de_tahifi_configuration.h"

/*!
 * \addtogroup dbus_handlers DBus handlers for signals
 * \ingroup dbus
 */
/*!@{*/

#ifdef __cplusplus
extern "C" {
#endif

void dbussignal_dcpd_playback(GDBusProxy *proxy, const gchar *sender_name,
                              const gchar *signal_name, GVariant *parameters,
                              gpointer user_data);
void dbussignal_dcpd_views(GDBusProxy *proxy, const gchar *sender_name,
                           const gchar *signal_name, GVariant *parameters,
                           gpointer user_data);
void dbussignal_dcpd_listnav(GDBusProxy *proxy, const gchar *sender_name,
                             const gchar *signal_name, GVariant *parameters,
                             gpointer user_data);
void dbussignal_dcpd_listitem(GDBusProxy *proxy, const gchar *sender_name,
                              const gchar *signal_name, GVariant *parameters,
                              gpointer user_data);
void dbussignal_lists_navigation(GDBusProxy *proxy, const gchar *sender_name,
                                 const gchar *signal_name, GVariant *parameters,
                                 gpointer user_data);
void dbussignal_splay_urlfifo(GDBusProxy *proxy, const gchar *sender_name,
                              const gchar *signal_name, GVariant *parameters,
                              gpointer user_data);
void dbussignal_splay_playback(GDBusProxy *proxy, const gchar *sender_name,
                               const gchar *signal_name, GVariant *parameters,
                               gpointer user_data);
void dbussignal_airable_sec(GDBusProxy *proxy, const gchar *sender_name,
                            const gchar *signal_name, GVariant *parameters,
                            gpointer user_data);
void dbussignal_audiopath_manager(GDBusProxy *proxy, const gchar *sender_name,
                                  const gchar *signal_name, GVariant *parameters,
                                  gpointer user_data);

gboolean dbusmethod_audiopath_source_selected_on_hold(tdbusaupathSource *object,
                                                      GDBusMethodInvocation *invocation,
                                                      const char *source_id,
                                                      GVariant *request_data,
                                                      gpointer user_data);
gboolean dbusmethod_audiopath_source_selected(tdbusaupathSource *object,
                                              GDBusMethodInvocation *invocation,
                                              const char *source_id,
                                              GVariant *request_data,
                                              gpointer user_data);
gboolean dbusmethod_audiopath_source_deselected(tdbusaupathSource *object,
                                                GDBusMethodInvocation *invocation,
                                                const char *source_id,
                                                GVariant *request_data,
                                                gpointer user_data);

gboolean dbusmethod_config_get_all_keys(tdbusConfigurationRead *object,
                                        GDBusMethodInvocation *invocation,
                                        gpointer user_data);
gboolean dbusmethod_config_get_value(tdbusConfigurationRead *object,
                                     GDBusMethodInvocation *invocation,
                                     const gchar *key, gpointer user_data);
gboolean dbusmethod_config_get_all_values(tdbusConfigurationRead *object,
                                          GDBusMethodInvocation *invocation,
                                          const gchar *database, gpointer user_data);

gboolean dbusmethod_config_set_value(tdbusConfigurationWrite *object,
                                     GDBusMethodInvocation *invocation,
                                     const char *origin, const char *key,
                                     GVariant *value, gpointer user_data);
gboolean dbusmethod_config_set_multiple_values(tdbusConfigurationWrite *object,
                                               GDBusMethodInvocation *invocation,
                                               const char *origin, GVariant *values,
                                               gpointer user_data);

#ifdef __cplusplus
}
#endif

/*!@}*/

#endif /* !DBUS_HANDLERS_H */
