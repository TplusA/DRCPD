#include <cppcutter.h>

#include "dbus_handlers.h"
#include "mock_view_manager.hh"
#include "mock_messages.hh"


/*!
 * \addtogroup dbus_handlers_tests Unit tests
 * \ingroup dbus_handlers
 *
 * DBus handlers unit tests.
 */
/*!@{*/

namespace dbus_handlers_tests
{

/*!\test
 * Check if de.tahifi.Dcpd.Playback.Start is handled correctly.
 */
void test_dcpd_playback_start(void);

/*!\test
 * Check if de.tahifi.Dcpd.Playback.Stop is handled correctly.
 */
void test_dcpd_playback_stop(void);

/*!\test
 * Check if de.tahifi.Dcpd.Playback.Pause is handled correctly.
 */
void test_dcpd_playback_pause(void);

/*!\test
 * Check if de.tahifi.Dcpd.Playback.Next is handled correctly.
 */
void test_dcpd_playback_next(void);

/*!\test
 * Check if de.tahifi.Dcpd.Playback.Previous is handled correctly.
 */
void test_dcpd_playback_previous(void);

/*!\test
 * Check if de.tahifi.Dcpd.Playback.FastForward is handled correctly.
 */
void test_dcpd_playback_fast_forward(void);

/*!\test
 * Check if de.tahifi.Dcpd.Playback.FastRewind is handled correctly.
 */
void test_dcpd_playback_fast_rewind(void);

/*!\test
 * Check if de.tahifi.Dcpd.Playback.FastWindStop is handled correctly.
 */
void test_dcpd_playback_fast_wind_stop(void);

/*!\test
 * Check if de.tahifi.Dcpd.Playback.FastWindSetFactor is handled correctly.
 */
void test_dcpd_playback_fast_wind_set_factor(void);

/*!\test
 * Check if de.tahifi.Dcpd.Playback.RepeatModeToggle is handled correctly.
 */
void test_dcpd_playback_repeat_mode_toggle(void);

/*!\test
 * Check if de.tahifi.Dcpd.Playback.ShuffleModeToggle is handled correctly.
 */
void test_dcpd_playback_shuffle_mode_toggle(void);

/*!\test
 * Check if unknown signals on de.tahifi.Dcpd.Playback are handled correctly.
 */
void test_dcpd_playback_unknown_signal_name(void);

};

/*!@}*/


namespace dbus_handlers_tests
{

static MockMessages mock_messages;
static MockViewManager mock_view_manager;

static GDBusProxy *dummy_gdbus_proxy;
static const char dummy_sender_name[] = ":1.123";

void cut_setup(void)
{
    dummy_gdbus_proxy = nullptr;

    mock_messages_singleton = &mock_messages;
    mock_messages.init();

    mock_view_manager.init();
}

void cut_teardown(void)
{
    mock_messages.check();
    mock_view_manager.check();
}

void test_dcpd_playback_start(void)
{
    mock_messages.expect_msg_info_formatted("de.tahifi.Dcpd.Playback signal from ':1.123': Start");
    mock_view_manager.expect_input(DrcpCommand::PLAYBACK_START);
    dbussignal_dcpd_playback(dummy_gdbus_proxy, dummy_sender_name,
                             "Start", nullptr, &mock_view_manager);
}

void test_dcpd_playback_stop(void)
{
    mock_messages.expect_msg_info_formatted("de.tahifi.Dcpd.Playback signal from ':1.123': Stop");
    mock_view_manager.expect_input(DrcpCommand::PLAYBACK_STOP);
    dbussignal_dcpd_playback(dummy_gdbus_proxy, dummy_sender_name,
                             "Stop", nullptr, &mock_view_manager);
}

void test_dcpd_playback_pause(void)
{
    mock_messages.expect_msg_info_formatted("de.tahifi.Dcpd.Playback signal from ':1.123': Pause");
    mock_view_manager.expect_input(DrcpCommand::PLAYBACK_PAUSE);
    dbussignal_dcpd_playback(dummy_gdbus_proxy, dummy_sender_name,
                             "Pause", nullptr, &mock_view_manager);
}

void test_dcpd_playback_next(void)
{
    mock_messages.expect_msg_info_formatted("de.tahifi.Dcpd.Playback signal from ':1.123': Next");
    mock_view_manager.expect_input(DrcpCommand::PLAYBACK_NEXT);
    dbussignal_dcpd_playback(dummy_gdbus_proxy, dummy_sender_name,
                             "Next", nullptr, &mock_view_manager);
}

void test_dcpd_playback_previous(void)
{
    mock_messages.expect_msg_info_formatted("de.tahifi.Dcpd.Playback signal from ':1.123': Previous");
    mock_view_manager.expect_input(DrcpCommand::PLAYBACK_PREVIOUS);
    dbussignal_dcpd_playback(dummy_gdbus_proxy, dummy_sender_name,
                             "Previous", nullptr, &mock_view_manager);
}

void test_dcpd_playback_fast_forward(void)
{
    mock_messages.expect_msg_info_formatted("de.tahifi.Dcpd.Playback signal from ':1.123': FastForward");
    mock_view_manager.expect_input(DrcpCommand::FAST_WIND_FORWARD);
    dbussignal_dcpd_playback(dummy_gdbus_proxy, dummy_sender_name,
                             "FastForward", nullptr, &mock_view_manager);
}

void test_dcpd_playback_fast_rewind(void)
{
    mock_messages.expect_msg_info_formatted("de.tahifi.Dcpd.Playback signal from ':1.123': FastRewind");
    mock_view_manager.expect_input(DrcpCommand::FAST_WIND_REVERSE);
    dbussignal_dcpd_playback(dummy_gdbus_proxy, dummy_sender_name,
                             "FastRewind", nullptr, &mock_view_manager);
}

void test_dcpd_playback_fast_wind_stop(void)
{
    mock_messages.expect_msg_info_formatted("de.tahifi.Dcpd.Playback signal from ':1.123': FastWindStop");
    mock_view_manager.expect_input(DrcpCommand::FAST_WIND_STOP);
    dbussignal_dcpd_playback(dummy_gdbus_proxy, dummy_sender_name,
                             "FastWindStop", nullptr, &mock_view_manager);
}

void test_dcpd_playback_fast_wind_set_factor(void)
{
    mock_messages.expect_msg_info_formatted("de.tahifi.Dcpd.Playback signal from ':1.123': FastWindSetFactor");
    mock_view_manager.expect_input_set_fast_wind_factor(6.2);

    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("(d)"));
    g_variant_builder_add(&builder, "d", double(6.2));
    GVariant *factor = g_variant_builder_end(&builder);

    dbussignal_dcpd_playback(dummy_gdbus_proxy, dummy_sender_name,
                             "FastWindSetFactor", factor, &mock_view_manager);

    g_variant_unref(factor);
}

void test_dcpd_playback_repeat_mode_toggle(void)
{
    mock_messages.expect_msg_info_formatted("de.tahifi.Dcpd.Playback signal from ':1.123': RepeatModeToggle");
    mock_view_manager.expect_input(DrcpCommand::REPEAT_MODE_TOGGLE);
    dbussignal_dcpd_playback(dummy_gdbus_proxy, dummy_sender_name,
                             "RepeatModeToggle", nullptr, &mock_view_manager);
}

void test_dcpd_playback_shuffle_mode_toggle(void)
{
    mock_messages.expect_msg_info_formatted("de.tahifi.Dcpd.Playback signal from ':1.123': ShuffleModeToggle");
    mock_view_manager.expect_input(DrcpCommand::SHUFFLE_MODE_TOGGLE);
    dbussignal_dcpd_playback(dummy_gdbus_proxy, dummy_sender_name,
                             "ShuffleModeToggle", nullptr, &mock_view_manager);
}

void test_dcpd_playback_unknown_signal_name(void)
{
    mock_messages.expect_msg_info_formatted("de.tahifi.Dcpd.Playback signal from ':1.123': UnsupportedSignalName");
    mock_messages.expect_msg_error_formatted(ENOSYS, LOG_NOTICE,
                                             "Got unknown signal de.tahifi.Dcpd.Playback.UnsupportedSignalName from :1.123 (Function not implemented)");
    dbussignal_dcpd_playback(dummy_gdbus_proxy, dummy_sender_name,
                             "UnsupportedSignalName", nullptr, &mock_view_manager);
}

};
