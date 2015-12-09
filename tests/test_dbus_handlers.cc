/*
 * Copyright (C) 2015  T+A elektroakustik GmbH & Co. KG
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
#include "mock_view_manager.hh"
#include "mock_messages.hh"

constexpr const char *ListError::names_[];

/*!
 * \addtogroup dbus_handlers_tests Unit tests
 * \ingroup dbus_handlers
 *
 * DBus handlers unit tests.
 */
/*!@{*/

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

    player = new Playback::Player;
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

/*!\test
 * Check if de.tahifi.Dcpd.Playback.Start is handled correctly.
 */
void test_dcpd_playback_start(void)
{
    DBusSignalData dbus_signal_data = { .mgr = *mock_view_manager, .player = *player, };
    mock_messages->expect_msg_info_formatted("de.tahifi.Dcpd.Playback signal from ':1.123': Start");
    mock_view_manager->expect_input(DrcpCommand::PLAYBACK_START);
    dbussignal_dcpd_playback(dummy_gdbus_proxy, dummy_sender_name,
                             "Start", nullptr, &dbus_signal_data);
}

/*!\test
 * Check if de.tahifi.Dcpd.Playback.Stop is handled correctly.
 */
void test_dcpd_playback_stop(void)
{
    DBusSignalData dbus_signal_data = { .mgr = *mock_view_manager, .player = *player, };
    mock_messages->expect_msg_info_formatted("de.tahifi.Dcpd.Playback signal from ':1.123': Stop");
    mock_view_manager->expect_input(DrcpCommand::PLAYBACK_STOP);
    dbussignal_dcpd_playback(dummy_gdbus_proxy, dummy_sender_name,
                             "Stop", nullptr, &dbus_signal_data);
}

/*!\test
 * Check if de.tahifi.Dcpd.Playback.Pause is handled correctly.
 */
void test_dcpd_playback_pause(void)
{
    DBusSignalData dbus_signal_data = { .mgr = *mock_view_manager, .player = *player, };
    mock_messages->expect_msg_info_formatted("de.tahifi.Dcpd.Playback signal from ':1.123': Pause");
    mock_view_manager->expect_input(DrcpCommand::PLAYBACK_PAUSE);
    dbussignal_dcpd_playback(dummy_gdbus_proxy, dummy_sender_name,
                             "Pause", nullptr, &dbus_signal_data);
}

/*!\test
 * Check if de.tahifi.Dcpd.Playback.Next is handled correctly.
 */
void test_dcpd_playback_next(void)
{
    DBusSignalData dbus_signal_data = { .mgr = *mock_view_manager, .player = *player, };
    mock_messages->expect_msg_info_formatted("de.tahifi.Dcpd.Playback signal from ':1.123': Next");
    mock_view_manager->expect_input(DrcpCommand::PLAYBACK_NEXT);
    dbussignal_dcpd_playback(dummy_gdbus_proxy, dummy_sender_name,
                             "Next", nullptr, &dbus_signal_data);
}

/*!\test
 * Check if de.tahifi.Dcpd.Playback.Previous is handled correctly.
 */
void test_dcpd_playback_previous(void)
{
    DBusSignalData dbus_signal_data = { .mgr = *mock_view_manager, .player = *player, };
    mock_messages->expect_msg_info_formatted("de.tahifi.Dcpd.Playback signal from ':1.123': Previous");
    mock_view_manager->expect_input(DrcpCommand::PLAYBACK_PREVIOUS);
    dbussignal_dcpd_playback(dummy_gdbus_proxy, dummy_sender_name,
                             "Previous", nullptr, &dbus_signal_data);
}

/*!\test
 * Check if de.tahifi.Dcpd.Playback.FastForward is handled correctly.
 */
void test_dcpd_playback_fast_forward(void)
{
    DBusSignalData dbus_signal_data = { .mgr = *mock_view_manager, .player = *player, };
    mock_messages->expect_msg_info_formatted("de.tahifi.Dcpd.Playback signal from ':1.123': FastForward");
    mock_view_manager->expect_input(DrcpCommand::FAST_WIND_FORWARD);
    dbussignal_dcpd_playback(dummy_gdbus_proxy, dummy_sender_name,
                             "FastForward", nullptr, &dbus_signal_data);
}

/*!\test
 * Check if de.tahifi.Dcpd.Playback.FastRewind is handled correctly.
 */
void test_dcpd_playback_fast_rewind(void)
{
    DBusSignalData dbus_signal_data = { .mgr = *mock_view_manager, .player = *player, };
    mock_messages->expect_msg_info_formatted("de.tahifi.Dcpd.Playback signal from ':1.123': FastRewind");
    mock_view_manager->expect_input(DrcpCommand::FAST_WIND_REVERSE);
    dbussignal_dcpd_playback(dummy_gdbus_proxy, dummy_sender_name,
                             "FastRewind", nullptr, &dbus_signal_data);
}

/*!\test
 * Check if de.tahifi.Dcpd.Playback.FastWindStop is handled correctly.
 */
void test_dcpd_playback_fast_wind_stop(void)
{
    DBusSignalData dbus_signal_data = { .mgr = *mock_view_manager, .player = *player, };
    mock_messages->expect_msg_info_formatted("de.tahifi.Dcpd.Playback signal from ':1.123': FastWindStop");
    mock_view_manager->expect_input(DrcpCommand::FAST_WIND_STOP);
    dbussignal_dcpd_playback(dummy_gdbus_proxy, dummy_sender_name,
                             "FastWindStop", nullptr, &dbus_signal_data);
}

/*!\test
 * Check if de.tahifi.Dcpd.Playback.FastWindSetFactor is handled correctly.
 */
void test_dcpd_playback_fast_wind_set_factor(void)
{
    DBusSignalData dbus_signal_data = { .mgr = *mock_view_manager, .player = *player, };
    mock_messages->expect_msg_info_formatted("de.tahifi.Dcpd.Playback signal from ':1.123': FastWindSetFactor");
    mock_view_manager->expect_input_set_fast_wind_factor(6.2);

    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("(d)"));
    g_variant_builder_add(&builder, "d", double(6.2));
    GVariant *factor = g_variant_builder_end(&builder);

    dbussignal_dcpd_playback(dummy_gdbus_proxy, dummy_sender_name,
                             "FastWindSetFactor", factor, &dbus_signal_data);

    g_variant_unref(factor);
}

/*!\test
 * Check if de.tahifi.Dcpd.Playback.RepeatModeToggle is handled correctly.
 */
void test_dcpd_playback_repeat_mode_toggle(void)
{
    DBusSignalData dbus_signal_data = { .mgr = *mock_view_manager, .player = *player, };
    mock_messages->expect_msg_info_formatted("de.tahifi.Dcpd.Playback signal from ':1.123': RepeatModeToggle");
    mock_view_manager->expect_input(DrcpCommand::REPEAT_MODE_TOGGLE);
    dbussignal_dcpd_playback(dummy_gdbus_proxy, dummy_sender_name,
                             "RepeatModeToggle", nullptr, &dbus_signal_data);
}

/*!\test
 * Check if de.tahifi.Dcpd.Playback.ShuffleModeToggle is handled correctly.
 */
void test_dcpd_playback_shuffle_mode_toggle(void)
{
    DBusSignalData dbus_signal_data = { .mgr = *mock_view_manager, .player = *player, };
    mock_messages->expect_msg_info_formatted("de.tahifi.Dcpd.Playback signal from ':1.123': ShuffleModeToggle");
    mock_view_manager->expect_input(DrcpCommand::SHUFFLE_MODE_TOGGLE);
    dbussignal_dcpd_playback(dummy_gdbus_proxy, dummy_sender_name,
                             "ShuffleModeToggle", nullptr, &dbus_signal_data);
}

/*!\test
 * Check if unknown signals on de.tahifi.Dcpd.Playback are handled correctly.
 */
void test_dcpd_playback_unknown_signal_name(void)
{
    DBusSignalData dbus_signal_data = { .mgr = *mock_view_manager, .player = *player, };
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
    DBusSignalData dbus_signal_data = { .mgr = *mock_view_manager, .player = *player, };
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
    DBusSignalData dbus_signal_data = { .mgr = *mock_view_manager, .player = *player, };
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
    DBusSignalData dbus_signal_data = { .mgr = *mock_view_manager, .player = *player, };
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
    DBusSignalData dbus_signal_data = { .mgr = *mock_view_manager, .player = *player, };
    mock_messages->expect_msg_info_formatted("de.tahifi.Dcpd.ListNavigation signal from ':1.123': LevelUp");
    mock_view_manager->expect_input(DrcpCommand::GO_BACK_ONE_LEVEL);

    dbussignal_dcpd_listnav(dummy_gdbus_proxy, dummy_sender_name,
                            "LevelUp", nullptr, &dbus_signal_data);
}

/*!\test
 * Check if de.tahifi.Dcpd.ListNavigation.LevelDown is handled correctly.
 */
void test_dcpd_listnav_level_down(void)
{
    DBusSignalData dbus_signal_data = { .mgr = *mock_view_manager, .player = *player, };
    mock_messages->expect_msg_info_formatted("de.tahifi.Dcpd.ListNavigation signal from ':1.123': LevelDown");
    mock_view_manager->expect_input(DrcpCommand::SELECT_ITEM);

    dbussignal_dcpd_listnav(dummy_gdbus_proxy, dummy_sender_name,
                            "LevelDown", nullptr, &dbus_signal_data);
}

/*!\test
 * Check if de.tahifi.Dcpd.ListNavigation.MoveLines is handled correctly.
 */
void test_dcpd_listnav_move_lines(void)
{
    DBusSignalData dbus_signal_data = { .mgr = *mock_view_manager, .player = *player, };
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
    DBusSignalData dbus_signal_data = { .mgr = *mock_view_manager, .player = *player, };
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
    DBusSignalData dbus_signal_data = { .mgr = *mock_view_manager, .player = *player, };
    mock_messages->expect_msg_info_formatted("de.tahifi.Dcpd.ListNavigation signal from ':1.123': UnsupportedSignalName");
    mock_messages->expect_msg_error_formatted(ENOSYS, LOG_NOTICE,
                                              "Got unknown signal de.tahifi.Dcpd.ListNavigation.UnsupportedSignalName from :1.123 (Function not implemented)");
    dbussignal_dcpd_listnav(dummy_gdbus_proxy, dummy_sender_name,
                            "UnsupportedSignalName", nullptr, &dbus_signal_data);
}

};

/*!@}*/
