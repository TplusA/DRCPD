/*
 * Copyright (C) 2016, 2017, 2019  T+A elektroakustik GmbH & Co. KG
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

#include <cppcutter.h>

/*
 * Prototypes for the dummies used in here.
 */
#include "de_tahifi_lists.h"

/*
 * Dummy for the linker.
 */
void tdbus_lists_navigation_call_get_list_id(tdbuslistsNavigation *proxy,
        guint arg_list_id, guint arg_item_id, GCancellable *cancellable,
        GAsyncReadyCallback callback, gpointer user_data)
{
    cut_fail("Unexpected call of %s()", __func__);
}

/*
 * Dummy for the linker.
 */
gboolean tdbus_lists_navigation_call_get_list_id_finish(
        tdbuslistsNavigation *proxy, guchar *out_error_code,
        guint *out_child_list_id, GAsyncResult *res, GError **error)
{
    cut_fail("Unexpected call of %s()", __func__);
    return FALSE;
}

/*
 * Dummy for the linker.
 */
void tdbus_lists_navigation_call_check_range(tdbuslistsNavigation *proxy,
        guint arg_list_id, guint arg_first_item_id, guint arg_count,
        GCancellable *cancellable, GAsyncReadyCallback callback,
        gpointer user_data)
{
    cut_fail("Unexpected call of %s()", __func__);
}

/*
 * Dummy for the linker.
 */
gboolean tdbus_lists_navigation_call_check_range_finish(
        tdbuslistsNavigation *proxy, guchar *out_error_code,
        guint *out_first_item, guint *out_number_of_items,
        GAsyncResult *res, GError **error)
{
    cut_fail("Unexpected call of %s()", __func__);
    return FALSE;
}

/*
 * Dummy for the linker.
 */
void tdbus_lists_navigation_call_get_range(tdbuslistsNavigation *proxy,
        guint arg_list_id, guint arg_first_item_id, guint arg_count,
        GCancellable *cancellable, GAsyncReadyCallback callback,
        gpointer user_data)
{
    cut_fail("Unexpected call of %s()", __func__);
}

/*
 * Dummy for the linker.
 */
gboolean tdbus_lists_navigation_call_get_range_finish(tdbuslistsNavigation *proxy,
        guchar *out_error_code, guint *out_first_item, GVariant *out_list,
        GAsyncResult *res, GError **error)
{
    cut_fail("Unexpected call of %s()", __func__);
    return FALSE;
}

/*
 * Dummy for the linker.
 */
void tdbus_lists_navigation_call_get_range_with_meta_data(
        tdbuslistsNavigation *proxy, guint arg_list_id, guint arg_first_item_id,
        guint arg_count, GCancellable *cancellable, GAsyncReadyCallback callback,
        gpointer user_data)
{
    cut_fail("Unexpected call of %s()", __func__);
}

/*
 * Dummy for the linker.
 */
gboolean tdbus_lists_navigation_call_get_range_with_meta_data_finish(
        tdbuslistsNavigation *proxy, guchar *out_error_code,
        guint *out_first_item, GVariant *out_list,
        GAsyncResult *res, GError **error)
{
    cut_fail("Unexpected call of %s()", __func__);
    return FALSE;
}

/*
 * Dummy for the linker.
 */
void tdbus_lists_navigation_call_get_parent_link(tdbuslistsNavigation *proxy,
    guint arg_list_id, GCancellable *cancellable,
    GAsyncReadyCallback callback, gpointer user_data)
{
    cut_fail("Unexpected call of %s()", __func__);
}

/*
 * Dummy for the linker.
 */
gboolean tdbus_lists_navigation_call_get_parent_link_finish(
    tdbuslistsNavigation *proxy, guint *out_parent_list_id,
    guint *out_parent_item_id, GAsyncResult *res, GError **error)
{
    cut_fail("Unexpected call of %s()", __func__);
    return FALSE;
}

/*
 * Dummy for the linker.
 */
void tdbus_lists_navigation_call_get_root_link_to_context(tdbuslistsNavigation *proxy,
        const gchar *arg_context, GCancellable *cancellable,
        GAsyncReadyCallback callback, gpointer user_data)
{
    cut_fail("Unexpected call of %s()", __func__);
}

/*
 * Dummy for the linker.
 */
void tdbus_lists_navigation_call_get_parameterized_list_id(
    tdbuslistsNavigation *proxy, guint arg_list_id, guint arg_item_id,
    const gchar *arg_parameter, GCancellable *cancellable,
    GAsyncReadyCallback callback, gpointer user_data)
{
    cut_fail("Unexpected call of %s()", __func__);
}

/*
 * Dummy for the linker.
 */
gboolean tdbus_lists_navigation_call_get_parameterized_list_id_finish(
    tdbuslistsNavigation *proxy, guchar *out_error_code,
    guint *out_child_list_id, GAsyncResult *res, GError **error)
{
    cut_fail("Unexpected call of %s()", __func__);
    return FALSE;
}

/*
 * Dummy for the linker.
 */
void tdbus_lists_navigation_call_get_uris(tdbuslistsNavigation *proxy,
    guint arg_list_id, guint arg_item_id, GCancellable *cancellable,
    GAsyncReadyCallback callback, gpointer user_data)
{
    cut_fail("Unexpected call of %s()", __func__);
}

/*
 * Dummy for the linker.
 */
gboolean tdbus_lists_navigation_call_get_uris_finish(tdbuslistsNavigation *proxy,
    guchar *out_error_code, gchar ***out_uri_list, GAsyncResult *res,
    GError **error)
{
    cut_fail("Unexpected call of %s()", __func__);
    return FALSE;
}

/*
 * Dummy for the linker.
 */
void tdbus_lists_navigation_call_get_ranked_stream_links(tdbuslistsNavigation *proxy,
    guint arg_list_id, guint arg_item_id, GCancellable *cancellable,
    GAsyncReadyCallback callback, gpointer user_data)
{
    cut_fail("Unexpected call of %s()", __func__);
}

/*
 * Dummy for the linker.
 */
gboolean tdbus_lists_navigation_call_get_ranked_stream_links_finish(
    tdbuslistsNavigation *proxy, guchar *out_error_code,
    GVariant **out_link_list, GAsyncResult *res, GError **error)
{
    cut_fail("Unexpected call of %s()", __func__);
    return FALSE;
}
