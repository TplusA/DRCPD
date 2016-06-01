/*
 * Copyright (C) 2015, 2016  T+A elektroakustik GmbH & Co. KG
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

#include <cppcutter.h>

#include "dbus_handlers.h"
#include "dbus_handlers.hh"
#include "de_tahifi_lists_errors.hh"
#include "player.hh"
#include "view_play.hh"
#include "ui_parameters_predefined.hh"

#include "mock_view_manager.hh"
#include "mock_messages.hh"

/*
 * Prototypes for the dummies used in here.
 */
#include "lists_dbus.h"

/*!
 * \addtogroup dbus_handlers_tests Unit tests
 * \ingroup dbus_handlers
 *
 * DBus handlers unit tests.
 */
/*!@{*/

/*
 * Dummy for the linker.
 */
void tdbus_lists_navigation_call_get_list_id(tdbuslistsNavigation *proxy,
                                             guint arg_list_id,
                                             guint arg_item_id,
                                             GCancellable *cancellable,
                                             GAsyncReadyCallback callback,
                                             gpointer user_data)
{
    cut_fail("Unexpected call of %s()", __func__);
}

/*
 * Dummy for the linker.
 */
gboolean tdbus_lists_navigation_call_get_list_id_finish(tdbuslistsNavigation *proxy,
                                                        guchar *out_error_code,
                                                        guint *out_child_list_id,
                                                        GAsyncResult *res,
                                                        GError **error)
{
    cut_fail("Unexpected call of %s()", __func__);
    return FALSE;
}

/*
 * Dummy for the linker.
 */
void tdbus_lists_navigation_call_check_range(tdbuslistsNavigation *proxy,
                                             guint arg_list_id,
                                             guint arg_first_item_id,
                                             guint arg_count,
                                             GCancellable *cancellable,
                                             GAsyncReadyCallback callback,
                                             gpointer user_data)
{
    cut_fail("Unexpected call of %s()", __func__);
}

/*
 * Dummy for the linker.
 */
gboolean tdbus_lists_navigation_call_check_range_finish(tdbuslistsNavigation *proxy,
                                                        guchar *out_error_code,
                                                        guint *out_first_item,
                                                        guint *out_number_of_items,
                                                        GAsyncResult *res,
                                                        GError **error)
{
    cut_fail("Unexpected call of %s()", __func__);
    return FALSE;
}

/*
 * Dummy for the linker.
 */
void tdbus_lists_navigation_call_get_uris(tdbuslistsNavigation *proxy,
                                          guint arg_list_id, guint arg_item_id,
                                          GCancellable *cancellable,
                                          GAsyncReadyCallback callback,
                                          gpointer user_data)
{
    cut_fail("Unexpected call of %s()", __func__);
}

/*
 * Dummy for the linker.
 */
gboolean tdbus_lists_navigation_call_get_uris_finish(tdbuslistsNavigation *proxy,
                                                     guchar *out_error_code,
                                                     gchar ***out_uri_list,
                                                     GAsyncResult *res,
                                                     GError **error)
{
    cut_fail("Unexpected call of %s()", __func__);
    return FALSE;
}

namespace dbus_handlers_tests
{

static MockMessages *mock_messages;
static MockViewManager *mock_view_manager;
static Playback::Player *player;

static GDBusProxy *dummy_gdbus_proxy;
static const char dummy_sender_name[] = ":1.123";

void cut_setup(void)
{
    dummy_gdbus_proxy = nullptr;

    mock_messages = new MockMessages;
    cppcut_assert_not_null(mock_messages);
    mock_messages->init();
    mock_messages_singleton = mock_messages;

    mock_view_manager = new MockViewManager;
    cppcut_assert_not_null(mock_view_manager);
    mock_view_manager->init();

    player = new Playback::Player(ViewPlay::meta_data_reformatters);
    cppcut_assert_not_null(player);
}

void cut_teardown(void)
{
    delete player;
    player = nullptr;

    mock_messages->check();
    mock_view_manager->check();

    mock_messages_singleton = nullptr;

    delete mock_messages;
    delete mock_view_manager;

    mock_messages = nullptr;
    mock_view_manager = nullptr;
}

static DBus::SignalData mk_dbus_signal_data()
{
    return DBus::SignalData(*mock_view_manager, *player, *player);
}

/*!\test
 * Check if de.tahifi.Dcpd.Playback.Start is handled correctly.
 */
void test_dcpd_playback_start(void)
{
    DBus::SignalData dbus_signal_data(mk_dbus_signal_data());
    mock_messages->expect_msg_info_formatted("de.tahifi.Dcpd.Playback signal from ':1.123': Start");
    mock_view_manager->expect_input(DrcpCommand::PLAYBACK_START, false);
    dbussignal_dcpd_playback(dummy_gdbus_proxy, dummy_sender_name,
                             "Start", nullptr, &dbus_signal_data);
}

/*!\test
 * Check if de.tahifi.Dcpd.Playback.Stop is handled correctly.
 */
void test_dcpd_playback_stop(void)
{
    DBus::SignalData dbus_signal_data(mk_dbus_signal_data());
    mock_messages->expect_msg_info_formatted("de.tahifi.Dcpd.Playback signal from ':1.123': Stop");
    mock_view_manager->expect_input(DrcpCommand::PLAYBACK_STOP, false);
    dbussignal_dcpd_playback(dummy_gdbus_proxy, dummy_sender_name,
                             "Stop", nullptr, &dbus_signal_data);
}

/*!\test
 * Check if de.tahifi.Dcpd.Playback.Pause is handled correctly.
 */
void test_dcpd_playback_pause(void)
{
    DBus::SignalData dbus_signal_data(mk_dbus_signal_data());
    mock_messages->expect_msg_info_formatted("de.tahifi.Dcpd.Playback signal from ':1.123': Pause");
    mock_view_manager->expect_input(DrcpCommand::PLAYBACK_PAUSE, false);
    dbussignal_dcpd_playback(dummy_gdbus_proxy, dummy_sender_name,
                             "Pause", nullptr, &dbus_signal_data);
}

/*!\test
 * Check if de.tahifi.Dcpd.Playback.Next is handled correctly.
 */
void test_dcpd_playback_next(void)
{
    DBus::SignalData dbus_signal_data(mk_dbus_signal_data());
    mock_messages->expect_msg_info_formatted("de.tahifi.Dcpd.Playback signal from ':1.123': Next");
    mock_view_manager->expect_input(DrcpCommand::PLAYBACK_NEXT, false);
    dbussignal_dcpd_playback(dummy_gdbus_proxy, dummy_sender_name,
                             "Next", nullptr, &dbus_signal_data);
}

/*!\test
 * Check if de.tahifi.Dcpd.Playback.Previous is handled correctly.
 */
void test_dcpd_playback_previous(void)
{
    DBus::SignalData dbus_signal_data(mk_dbus_signal_data());
    mock_messages->expect_msg_info_formatted("de.tahifi.Dcpd.Playback signal from ':1.123': Previous");
    mock_view_manager->expect_input(DrcpCommand::PLAYBACK_PREVIOUS, false);
    dbussignal_dcpd_playback(dummy_gdbus_proxy, dummy_sender_name,
                             "Previous", nullptr, &dbus_signal_data);
}

/*!\test
 * Check if de.tahifi.Dcpd.Playback.FastForward is handled correctly.
 */
void test_dcpd_playback_fast_forward(void)
{
    DBus::SignalData dbus_signal_data(mk_dbus_signal_data());
    mock_messages->expect_msg_info_formatted("de.tahifi.Dcpd.Playback signal from ':1.123': FastForward");
    mock_view_manager->expect_input(DrcpCommand::FAST_WIND_FORWARD, false);
    dbussignal_dcpd_playback(dummy_gdbus_proxy, dummy_sender_name,
                             "FastForward", nullptr, &dbus_signal_data);
}

/*!\test
 * Check if de.tahifi.Dcpd.Playback.FastRewind is handled correctly.
 */
void test_dcpd_playback_fast_rewind(void)
{
    DBus::SignalData dbus_signal_data(mk_dbus_signal_data());
    mock_messages->expect_msg_info_formatted("de.tahifi.Dcpd.Playback signal from ':1.123': FastRewind");
    mock_view_manager->expect_input(DrcpCommand::FAST_WIND_REVERSE, false);
    dbussignal_dcpd_playback(dummy_gdbus_proxy, dummy_sender_name,
                             "FastRewind", nullptr, &dbus_signal_data);
}

/*!\test
 * Check if de.tahifi.Dcpd.Playback.FastWindStop is handled correctly.
 */
void test_dcpd_playback_fast_wind_stop(void)
{
    DBus::SignalData dbus_signal_data(mk_dbus_signal_data());
    mock_messages->expect_msg_info_formatted("de.tahifi.Dcpd.Playback signal from ':1.123': FastWindStop");
    mock_view_manager->expect_input(DrcpCommand::FAST_WIND_STOP, false);
    dbussignal_dcpd_playback(dummy_gdbus_proxy, dummy_sender_name,
                             "FastWindStop", nullptr, &dbus_signal_data);
}

static bool check_speed_parameter_called;
static void check_speed_parameter(const UI::Parameters *expected_parameters,
                                  const std::unique_ptr<const UI::Parameters> &actual_parameters)
{
    check_speed_parameter_called = true;

    const auto *expected = dynamic_cast<const UI::ParamsFWSpeed *>(expected_parameters);
    const auto *actual = dynamic_cast<const UI::ParamsFWSpeed *>(actual_parameters.get());

    cppcut_assert_not_null(expected);
    cppcut_assert_not_null(actual);

    cut_assert_operator(expected->get_specific(), <=, actual->get_specific());
    cut_assert_operator(expected->get_specific(), >=, actual->get_specific());
}

/*!\test
 * Check if de.tahifi.Dcpd.Playback.FastWindSetFactor is handled correctly.
 */
void test_dcpd_playback_fast_wind_set_factor(void)
{
    DBus::SignalData dbus_signal_data(mk_dbus_signal_data());
    mock_messages->expect_msg_info_formatted("de.tahifi.Dcpd.Playback signal from ':1.123': FastWindSetFactor");

    const UI::ParamsFWSpeed speed_factor(6.2);
    mock_view_manager->expect_input_with_callback(DrcpCommand::FAST_WIND_SET_SPEED,
                                                  &speed_factor,
                                                  check_speed_parameter);

    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("(d)"));
    g_variant_builder_add(&builder, "d", double(6.2));
    GVariant *factor = g_variant_builder_end(&builder);

    check_speed_parameter_called = false;
    dbussignal_dcpd_playback(dummy_gdbus_proxy, dummy_sender_name,
                             "FastWindSetFactor", factor, &dbus_signal_data);
    cut_assert_true(check_speed_parameter_called);

    g_variant_unref(factor);
}

/*!\test
 * Check if de.tahifi.Dcpd.Playback.RepeatModeToggle is handled correctly.
 */
void test_dcpd_playback_repeat_mode_toggle(void)
{
    DBus::SignalData dbus_signal_data(mk_dbus_signal_data());
    mock_messages->expect_msg_info_formatted("de.tahifi.Dcpd.Playback signal from ':1.123': RepeatModeToggle");
    mock_view_manager->expect_input(DrcpCommand::REPEAT_MODE_TOGGLE, false);
    dbussignal_dcpd_playback(dummy_gdbus_proxy, dummy_sender_name,
                             "RepeatModeToggle", nullptr, &dbus_signal_data);
}

/*!\test
 * Check if de.tahifi.Dcpd.Playback.ShuffleModeToggle is handled correctly.
 */
void test_dcpd_playback_shuffle_mode_toggle(void)
{
    DBus::SignalData dbus_signal_data(mk_dbus_signal_data());
    mock_messages->expect_msg_info_formatted("de.tahifi.Dcpd.Playback signal from ':1.123': ShuffleModeToggle");
    mock_view_manager->expect_input(DrcpCommand::SHUFFLE_MODE_TOGGLE, false);
    dbussignal_dcpd_playback(dummy_gdbus_proxy, dummy_sender_name,
                             "ShuffleModeToggle", nullptr, &dbus_signal_data);
}

/*!\test
 * Check if unknown signals on de.tahifi.Dcpd.Playback are handled correctly.
 */
void test_dcpd_playback_unknown_signal_name(void)
{
    DBus::SignalData dbus_signal_data(mk_dbus_signal_data());
    mock_messages->expect_msg_info_formatted("de.tahifi.Dcpd.Playback signal from ':1.123': UnsupportedSignalName");
    mock_messages->expect_msg_error_formatted(ENOSYS, LOG_NOTICE,
                                              "Got unknown signal de.tahifi.Dcpd.Playback.UnsupportedSignalName from :1.123 (Function not implemented)");
    dbussignal_dcpd_playback(dummy_gdbus_proxy, dummy_sender_name,
                             "UnsupportedSignalName", nullptr, &dbus_signal_data);
}

/*!\test
 * Check if de.tahifi.Dcpd.Views.Open is handled correctly.
 */
void test_dcpd_views_open(void)
{
    DBus::SignalData dbus_signal_data(mk_dbus_signal_data());
    mock_messages->expect_msg_info_formatted("de.tahifi.Dcpd.Views signal from ':1.123': Open");
    mock_view_manager->expect_activate_view_by_name("SomeViewName");

    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("(s)"));
    g_variant_builder_add(&builder, "s", "SomeViewName");
    GVariant *view_name = g_variant_builder_end(&builder);

    dbussignal_dcpd_views(dummy_gdbus_proxy, dummy_sender_name,
                          "Open", view_name, &dbus_signal_data);

    g_variant_unref(view_name);
}

/*!\test
 * Check if de.tahifi.Dcpd.Views.Toggle is handled correctly.
 */
void test_dcpd_views_toggle(void)
{
    DBus::SignalData dbus_signal_data(mk_dbus_signal_data());
    mock_messages->expect_msg_info_formatted("de.tahifi.Dcpd.Views signal from ':1.123': Toggle");
    mock_view_manager->expect_toggle_views_by_name("Foo", "Bar");

    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("(ss)"));
    g_variant_builder_add(&builder, "s", "Foo");
    g_variant_builder_add(&builder, "s", "Bar");
    GVariant *view_names = g_variant_builder_end(&builder);

    dbussignal_dcpd_views(dummy_gdbus_proxy, dummy_sender_name,
                          "Toggle", view_names, &dbus_signal_data);

    g_variant_unref(view_names);
}

/*!\test
 * Check if unknown signals on de.tahifi.Dcpd.Views are handled correctly.
 */
void test_dcpd_views_unknown_signal_name(void)
{
    DBus::SignalData dbus_signal_data(mk_dbus_signal_data());
    mock_messages->expect_msg_info_formatted("de.tahifi.Dcpd.Views signal from ':1.123': UnsupportedSignalName");
    mock_messages->expect_msg_error_formatted(ENOSYS, LOG_NOTICE,
                                              "Got unknown signal de.tahifi.Dcpd.Views.UnsupportedSignalName from :1.123 (Function not implemented)");
    dbussignal_dcpd_views(dummy_gdbus_proxy, dummy_sender_name,
                          "UnsupportedSignalName", nullptr, &dbus_signal_data);
}

/*!\test
 * Check if de.tahifi.Dcpd.ListNavigation.LevelUp is handled correctly.
 */
void test_dcpd_listnav_level_up(void)
{
    DBus::SignalData dbus_signal_data(mk_dbus_signal_data());
    mock_messages->expect_msg_info_formatted("de.tahifi.Dcpd.ListNavigation signal from ':1.123': LevelUp");
    mock_view_manager->expect_input(DrcpCommand::GO_BACK_ONE_LEVEL, false);

    dbussignal_dcpd_listnav(dummy_gdbus_proxy, dummy_sender_name,
                            "LevelUp", nullptr, &dbus_signal_data);
}

/*!\test
 * Check if de.tahifi.Dcpd.ListNavigation.LevelDown is handled correctly.
 */
void test_dcpd_listnav_level_down(void)
{
    DBus::SignalData dbus_signal_data(mk_dbus_signal_data());
    mock_messages->expect_msg_info_formatted("de.tahifi.Dcpd.ListNavigation signal from ':1.123': LevelDown");
    mock_view_manager->expect_input(DrcpCommand::SELECT_ITEM, false);

    dbussignal_dcpd_listnav(dummy_gdbus_proxy, dummy_sender_name,
                            "LevelDown", nullptr, &dbus_signal_data);
}

/*!\test
 * Check if de.tahifi.Dcpd.ListNavigation.MoveLines is handled correctly.
 */
void test_dcpd_listnav_move_lines(void)
{
    DBus::SignalData dbus_signal_data(mk_dbus_signal_data());
    mock_messages->expect_msg_info_formatted("de.tahifi.Dcpd.ListNavigation signal from ':1.123': MoveLines");
    mock_view_manager->expect_input_move_cursor_by_line(3);

    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("(i)"));
    g_variant_builder_add(&builder, "i", 3);
    GVariant *lines = g_variant_builder_end(&builder);

    dbussignal_dcpd_listnav(dummy_gdbus_proxy, dummy_sender_name,
                            "MoveLines", lines, &dbus_signal_data);

    g_variant_unref(lines);
}

/*!\test
 * Check if de.tahifi.Dcpd.ListNavigation.MovePages is handled correctly.
 */
void test_dcpd_listnav_move_cursor_by_page(void)
{
    DBus::SignalData dbus_signal_data(mk_dbus_signal_data());
    mock_messages->expect_msg_info_formatted("de.tahifi.Dcpd.ListNavigation signal from ':1.123': MovePages");
    mock_view_manager->expect_input_move_cursor_by_page(-2);

    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("(i)"));
    g_variant_builder_add(&builder, "i", -2);
    GVariant *pages = g_variant_builder_end(&builder);

    dbussignal_dcpd_listnav(dummy_gdbus_proxy, dummy_sender_name,
                            "MovePages", pages, &dbus_signal_data);

    g_variant_unref(pages);
}

/*!\test
 * Check if unknown signals on de.tahifi.Dcpd.ListNavigation are handled
 * correctly.
 */
void test_dcpd_listnav_unknown_signal_name(void)
{
    DBus::SignalData dbus_signal_data(mk_dbus_signal_data());
    mock_messages->expect_msg_info_formatted("de.tahifi.Dcpd.ListNavigation signal from ':1.123': UnsupportedSignalName");
    mock_messages->expect_msg_error_formatted(ENOSYS, LOG_NOTICE,
                                              "Got unknown signal de.tahifi.Dcpd.ListNavigation.UnsupportedSignalName from :1.123 (Function not implemented)");
    dbussignal_dcpd_listnav(dummy_gdbus_proxy, dummy_sender_name,
                            "UnsupportedSignalName", nullptr, &dbus_signal_data);
}

};

/*!@}*/
