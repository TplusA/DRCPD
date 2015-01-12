#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <cstring>
#include <cerrno>

#include "dbus_handlers.h"
#include "view_manager.hh"
#include "messages.h"

static void unknown_signal(const char *iface_name, const char *signal_name,
                           const char *sender_name)
{
    msg_error(ENOSYS, LOG_NOTICE, "Got unknown signal %s.%s from %s",
              iface_name, signal_name, sender_name);
}

static void check_parameter_assertions(GVariant *parameters,
                                       guint expected_number_of_parameters)
{
    /* we may use #log_assert() here because the GDBus code is supposed to do
     * any type checks before calling us---here, we just make sure we can
     * trust those type checks */
    log_assert(g_variant_type_is_tuple(g_variant_get_type(parameters)));
    log_assert(g_variant_n_children(parameters) == expected_number_of_parameters);
}

void dbussignal_dcpd_playback(GDBusProxy *proxy, const gchar *sender_name,
                              const gchar *signal_name, GVariant *parameters,
                              gpointer user_data)
{
    static const char iface_name[] = "de.tahifi.Dcpd.Playback";

    msg_info("%s signal from '%s': %s", iface_name, sender_name, signal_name);

    auto *mgr = static_cast<ViewManagerIface *>(user_data);
    log_assert(mgr != nullptr);

    if(strcmp(signal_name, "Start") == 0)
        mgr->input(DrcpCommand::PLAYBACK_START);
    else if(strcmp(signal_name, "Stop") == 0)
        mgr->input(DrcpCommand::PLAYBACK_STOP);
    else if(strcmp(signal_name, "Pause") == 0)
        mgr->input(DrcpCommand::PLAYBACK_PAUSE);
    else if(strcmp(signal_name, "Next") == 0)
        mgr->input(DrcpCommand::PLAYBACK_NEXT);
    else if(strcmp(signal_name, "Previous") == 0)
        mgr->input(DrcpCommand::PLAYBACK_PREVIOUS);
    else if(strcmp(signal_name, "FastForward") == 0)
        mgr->input(DrcpCommand::FAST_WIND_FORWARD);
    else if(strcmp(signal_name, "FastRewind") == 0)
        mgr->input(DrcpCommand::FAST_WIND_REVERSE);
    else if(strcmp(signal_name, "FastWindStop") == 0)
        mgr->input(DrcpCommand::FAST_WIND_STOP);
    else if(strcmp(signal_name, "FastWindSetFactor") == 0)
    {
        check_parameter_assertions(parameters, 1);

        GVariant *val = g_variant_get_child_value(parameters, 0);
        log_assert(val != nullptr);

        mgr->input_set_fast_wind_factor(g_variant_get_double(val));
        g_variant_unref(val);
    }
    else if(strcmp(signal_name, "RepeatModeToggle") == 0)
        mgr->input(DrcpCommand::REPEAT_MODE_TOGGLE);
    else if(strcmp(signal_name, "ShuffleModeToggle") == 0)
        mgr->input(DrcpCommand::SHUFFLE_MODE_TOGGLE);
    else
        unknown_signal(iface_name, signal_name, sender_name);
}

void dbussignal_dcpd_views(GDBusProxy *proxy, const gchar *sender_name,
                           const gchar *signal_name, GVariant *parameters,
                           gpointer user_data)
{
    static const char iface_name[] = "de.tahifi.Dcpd.Views";

    msg_info("%s signal from '%s': %s", iface_name, sender_name, signal_name);

    auto *mgr = static_cast<ViewManagerIface *>(user_data);
    log_assert(mgr != nullptr);

    if(strcmp(signal_name, "Open") == 0)
    {
        check_parameter_assertions(parameters, 1);

        GVariant *view_name = g_variant_get_child_value(parameters, 0);
        log_assert(view_name != nullptr);

        mgr->activate_view_by_name(g_variant_get_string(view_name, NULL));

        g_variant_unref(view_name);
    }
    else if(strcmp(signal_name, "Toggle") == 0)
    {
        check_parameter_assertions(parameters, 2);

        GVariant *first_view_name = g_variant_get_child_value(parameters, 0);
        GVariant *second_view_name = g_variant_get_child_value(parameters, 1);
        log_assert(first_view_name != nullptr);
        log_assert(second_view_name != nullptr);

        mgr->toggle_views_by_name(g_variant_get_string(first_view_name, NULL),
                                  g_variant_get_string(second_view_name, NULL));

        g_variant_unref(first_view_name);
        g_variant_unref(second_view_name);
    }
    else
        unknown_signal(iface_name, signal_name, sender_name);
}

void dbussignal_dcpd_listnav(GDBusProxy *proxy, const gchar *sender_name,
                             const gchar *signal_name, GVariant *parameters,
                             gpointer user_data)
{
    static const char iface_name[] = "de.tahifi.Dcpd.ListNavigation";

    msg_info("%s signal from '%s': %s", iface_name, sender_name, signal_name);

    auto *mgr = static_cast<ViewManagerIface *>(user_data);
    log_assert(mgr != nullptr);

    if(strcmp(signal_name, "LevelUp") == 0)
        mgr->input(DrcpCommand::GO_BACK_ONE_LEVEL);
    else if(strcmp(signal_name, "LevelDown") == 0)
        mgr->input(DrcpCommand::SELECT_ITEM);
    else if(strcmp(signal_name, "MoveLines") == 0)
    {
        check_parameter_assertions(parameters, 1);

        GVariant *lines = g_variant_get_child_value(parameters, 0);
        log_assert(lines != nullptr);

        mgr->input_move_cursor_by_line(g_variant_get_int32(lines));

        g_variant_unref(lines);
    }
    else if(strcmp(signal_name, "MovePages") == 0)
    {
        check_parameter_assertions(parameters, 1);

        GVariant *pages = g_variant_get_child_value(parameters, 0);
        log_assert(pages != nullptr);

        mgr->input_move_cursor_by_page(g_variant_get_int32(pages));

        g_variant_unref(pages);
    }
    else
        unknown_signal(iface_name, signal_name, sender_name);
}

void dbussignal_dcpd_listitem(GDBusProxy *proxy, const gchar *sender_name,
                              const gchar *signal_name, GVariant *parameters,
                              gpointer user_data)
{
    static const char iface_name[] = "de.tahifi.Dcpd.ListItem";

    msg_info("%s signal from '%s': %s", iface_name, sender_name, signal_name);
}

void dbussignal_lists_navigation(GDBusProxy *proxy, const gchar *sender_name,
                                 const gchar *signal_name, GVariant *parameters,
                                 gpointer user_data)
{
    static const char iface_name[] = "de.tahifi.Lists.Navigation";

    msg_info("%s signal from '%s': %s", iface_name, sender_name, signal_name);
}

void dbussignal_splay_urlfifo(GDBusProxy *proxy, const gchar *sender_name,
                              const gchar *signal_name, GVariant *parameters,
                              gpointer user_data)
{
    static const char iface_name[] = "de.tahifi.Streamplayer.URLFIFO";

    msg_info("%s signal from '%s': %s", iface_name, sender_name, signal_name);
}

void dbussignal_splay_playback(GDBusProxy *proxy, const gchar *sender_name,
                               const gchar *signal_name, GVariant *parameters,
                               gpointer user_data)
{
    static const char iface_name[] = "de.tahifi.Streamplayer.Playback";

    msg_info("%s signal from '%s': %s", iface_name, sender_name, signal_name);
}
