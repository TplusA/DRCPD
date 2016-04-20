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
#include <sstream>

#include "view_manager.hh"
#include "view_nop.hh"
#include "ui_parameters_predefined.hh"

#include "view_mock.hh"
#include "mock_messages.hh"

/*!
 * \addtogroup view_manager_tests Unit tests
 * \ingroup view_manager
 *
 * View manager unit tests.
 */
/*!@{*/

static void clear_ostream(std::ostringstream &ss)
{
    ss.str("");
    ss.clear();
}

static void check_and_clear_ostream(const char *string, std::ostringstream &ss)
{
    cppcut_assert_equal(string, ss.str().c_str());
    clear_ostream(ss);
}

static void dcp_transaction_setup_timeout(bool start_timeout_timer)
{
    /* nothing */
}

static void dcp_deferred_tx()
{
    /* nothing */
}

static const std::function<void(bool)> transaction_observer(dcp_transaction_setup_timeout);
static const std::function<void()> deferred_dcp_transfer_observer(dcp_deferred_tx);

namespace view_manager_tests_basics
{

static MockMessages *mock_messages;
static DCP::Queue *queue;
static ViewManager::Manager *vm;
static std::ostringstream *views_output;
static const char standard_mock_view_name[] = "Mock";

void cut_setup(void)
{
    views_output = new std::ostringstream();
    cppcut_assert_not_null(views_output);

    mock_messages = new MockMessages();
    mock_messages->init();
    mock_messages_singleton = mock_messages;

    queue = new DCP::Queue(transaction_observer, deferred_dcp_transfer_observer);
    cppcut_assert_not_null(queue);

    vm = new ViewManager::Manager(*queue);
    cppcut_assert_not_null(vm);
    vm->set_output_stream(*views_output);
}

void cut_teardown(void)
{
    cppcut_assert_equal("", views_output->str().c_str());
    cut_assert_true(queue->is_idle());

    mock_messages->check();

    delete mock_messages;
    delete vm;
    delete queue;
    delete views_output;

    mock_messages = nullptr;
    vm =nullptr;
    queue = nullptr;
    views_output = nullptr;
}

/*!\test
 * Attempt to add nothingness to the views is handled and leads to failure.
 */
void test_add_nullptr_view_fails(void)
{
    cut_assert_false(vm->add_view(nullptr));
}

/*!\test
 * Attempt to add a NOP view is rejected and leads to failure.
 */
void test_add_nop_view_fails(void)
{
    ViewNop::View view;

    cut_assert_true(view.init());
    cut_assert_false(vm->add_view(&view));
}

/*!\test
 * Adding a regular view to a fresh view manager works.
 */
void test_add_view(void)
{
    ViewMock::View view(standard_mock_view_name, false);

    cut_assert_true(view.init());
    cut_assert_true(vm->add_view(&view));
    view.check();
}

/*!\test
 * Attempt to add views with the same name only works for the first attempt.
 */
void test_add_views_with_same_name_fails(void)
{
    ViewMock::View view(standard_mock_view_name, false);

    cut_assert_true(view.init());
    cut_assert_true(vm->add_view(&view));
    cut_assert_false(vm->add_view(&view));
    view.check();
}

/*!\test
 * Adding a regular view to a fresh view manager and activating it works.
 */
void test_add_view_and_activate(void)
{
    ViewMock::View view(standard_mock_view_name, false);

    cut_assert_true(view.init());
    cut_assert_true(vm->add_view(&view));
    view.check();

    mock_messages->expect_msg_info_formatted("Requested to activate view \"Mock\"");
    view.expect_focus();
    view.expect_serialize(*views_output);
    view.expect_write_xml_begin(true, true);
    vm->activate_view_by_name(standard_mock_view_name);
    vm->serialization_result(DCP::Transaction::OK);
    view.check();

    check_and_clear_ostream("Mock serialize\n", *views_output);
}

/*!\test
 * Look up non-existent view returns null pointer.
 */
void test_get_nonexistent_view_by_name_fails(void)
{
    cut_assert_null(vm->get_view_by_name("DoesNotExist"));
}

/*!\test
 * Look up existent view returns non-null pointer.
 */
void test_get_existent_view_by_name_returns_view_interface(void)
{
    ViewMock::View view(standard_mock_view_name, false);

    cut_assert_true(view.init());
    cut_assert_true(vm->add_view(&view));
    cut_assert_not_null(vm->get_view_by_name(standard_mock_view_name));
    view.check();
}

};

namespace view_manager_tests
{

static MockMessages *mock_messages;
static DCP::Queue *queue;
static ViewManager::Manager *vm;
static std::ostringstream *views_output;
static const char standard_mock_view_name[] = "Mock";
static ViewMock::View *mock_view;

void cut_setup(void)
{
    views_output = new std::ostringstream();
    cppcut_assert_not_null(views_output);

    mock_messages = new MockMessages();
    cppcut_assert_not_null(mock_messages);
    mock_messages->init();
    mock_messages_singleton = mock_messages;

    mock_view = new ViewMock::View(standard_mock_view_name, false);
    cppcut_assert_not_null(mock_view);
    cut_assert_true(mock_view->init());

    queue = new DCP::Queue(dcp_transaction_setup_timeout, dcp_deferred_tx);
    cppcut_assert_not_null(queue);

    vm = new ViewManager::Manager(*queue);
    cppcut_assert_not_null(vm);
    vm->set_output_stream(*views_output);
    cut_assert_true(vm->add_view(mock_view));

    mock_messages->ignore_all_ = true;
    mock_view->ignore_all_ = true;
    vm->activate_view_by_name(standard_mock_view_name);
    vm->serialization_result(DCP::Transaction::OK);
    mock_view->ignore_all_ = false;
    mock_messages->ignore_all_ = false;
}

void cut_teardown(void)
{
    cppcut_assert_equal("", views_output->str().c_str());
    cut_assert_true(queue->is_idle());

    mock_messages->check();
    mock_view->check();

    delete vm;
    delete queue;
    delete mock_view;
    delete mock_messages;
    delete views_output;

    mock_messages = nullptr;
    mock_view = nullptr;
    vm =nullptr;
    queue = nullptr;
    views_output = nullptr;
}

/*!\test
 * Requests to move the cursor by zero lines have no effect.
 */
void test_move_cursor_by_zero_lines(void)
{
    vm->input_move_cursor_by_line(0);
}

static bool check_equal_lines_parameter_called;
static void check_equal_lines_parameter(const UI::Parameters *expected_parameters,
                                        const std::unique_ptr<const UI::Parameters> &actual_parameters)
{
    check_equal_lines_parameter_called = true;

    const auto *expected = dynamic_cast<const UI::ParamsUpDownSteps *>(expected_parameters);
    const auto *actual = dynamic_cast<const UI::ParamsUpDownSteps *>(actual_parameters.get());

    cppcut_assert_not_null(expected);
    cppcut_assert_not_null(actual);

    cppcut_assert_equal(expected->get_specific(), actual->get_specific());
}

/*!\test
 * Requests to move the cursor by multiple lines up are transformed into
 * multiple virtual key presses for the current view.
 *
 * There is only a single update call in the end.
 */
void test_move_cursor_up_by_multiple_lines(void)
{
    auto lines =
        std::unique_ptr<UI::ParamsUpDownSteps>(new UI::ParamsUpDownSteps(2));

    mock_view->expect_input_with_callback(ViewIface::InputResult::UPDATE_NEEDED,
                                          DrcpCommand::SCROLL_UP_MANY,
                                          lines.get(), check_equal_lines_parameter);
    mock_view->expect_update(*views_output);
    mock_view->expect_write_xml_begin(true, false);

    check_equal_lines_parameter_called = false;
    vm->input_move_cursor_by_line(-2);
    cut_assert_true(check_equal_lines_parameter_called);

    vm->serialization_result(DCP::Transaction::OK);

    check_and_clear_ostream("Mock update\n", *views_output);
}

/*!\test
 * Requests to move the cursor by multiple lines down are transformed into
 * multiple virtual key presses for the current view.
 *
 * There is only a single update call in the end.
 */
void test_move_cursor_down_by_multiple_lines(void)
{
    auto lines =
        std::unique_ptr<UI::ParamsUpDownSteps>(new UI::ParamsUpDownSteps(3));

    mock_view->expect_input_with_callback(ViewIface::InputResult::UPDATE_NEEDED,
                                          DrcpCommand::SCROLL_DOWN_MANY,
                                          lines.get(), check_equal_lines_parameter);
    mock_view->expect_update(*views_output);
    mock_view->expect_write_xml_begin(true, false);

    check_equal_lines_parameter_called = false;
    vm->input_move_cursor_by_line(3);
    cut_assert_true(check_equal_lines_parameter_called);

    vm->serialization_result(DCP::Transaction::OK);

    check_and_clear_ostream("Mock update\n", *views_output);
}

/*!\test
 * If the view indicates that after an input nothing has changed, then upwards
 * cursor movement is stopped.
 */
void test_move_cursor_by_multiple_lines_up_stops_at_beginning_of_list(void)
{
    auto lines =
        std::unique_ptr<UI::ParamsUpDownSteps>(new UI::ParamsUpDownSteps(5));

    mock_view->expect_input_with_callback(ViewIface::InputResult::UPDATE_NEEDED,
                                          DrcpCommand::SCROLL_UP_MANY,
                                          lines.get(), check_equal_lines_parameter);
    mock_view->expect_update(*views_output);
    mock_view->expect_write_xml_begin(true, false);

    check_equal_lines_parameter_called = false;
    vm->input_move_cursor_by_line(-5);
    cut_assert_true(check_equal_lines_parameter_called);

    vm->serialization_result(DCP::Transaction::OK);

    check_and_clear_ostream("Mock update\n", *views_output);
}

/*!\test
 * If the view indicates that after an input nothing has changed, then
 * downwards cursor movement is stopped.
 */
void test_move_cursor_by_multiple_lines_down_stops_at_end_of_list(void)
{
    auto lines =
        std::unique_ptr<UI::ParamsUpDownSteps>(new UI::ParamsUpDownSteps(5));

    mock_view->expect_input_with_callback(ViewIface::InputResult::UPDATE_NEEDED,
                                          DrcpCommand::SCROLL_DOWN_MANY,
                                          lines.get(), check_equal_lines_parameter);
    mock_view->expect_update(*views_output);
    mock_view->expect_write_xml_begin(true, false);

    check_equal_lines_parameter_called = false;
    vm->input_move_cursor_by_line(5);
    cut_assert_true(check_equal_lines_parameter_called);

    vm->serialization_result(DCP::Transaction::OK);

    check_and_clear_ostream("Mock update\n", *views_output);
}

/*!\test
 * Requests to move the cursor by zero pages have no effect.
 */
void test_move_cursor_by_zero_pages(void)
{
    vm->input_move_cursor_by_page(0);
}

/*!\test
 * Requests to move the cursor by multiple pages up are transformed into
 * multiple virtual key presses for the current view.
 *
 * There is only a single update call in the end.
 */
void test_move_cursor_up_by_multiple_pages(void)
{
    auto lines =
        std::unique_ptr<UI::ParamsUpDownSteps>(new UI::ParamsUpDownSteps(4 * ViewManager::VMIface::NUMBER_OF_LINES_ON_DISPLAY));

    mock_view->expect_input_with_callback(ViewIface::InputResult::UPDATE_NEEDED,
                                          DrcpCommand::SCROLL_UP_MANY,
                                          lines.get(), check_equal_lines_parameter);
    mock_view->expect_update(*views_output);
    mock_view->expect_write_xml_begin(true, false);

    check_equal_lines_parameter_called = false;
    vm->input_move_cursor_by_page(-4);
    cut_assert_true(check_equal_lines_parameter_called);

    vm->serialization_result(DCP::Transaction::OK);

    check_and_clear_ostream("Mock update\n", *views_output);
}

/*!\test
 * Requests to move the cursor by multiple pages down are transformed into
 * multiple virtual key presses for the current view.
 *
 * There is only a single update call in the end.
 */
void test_move_cursor_down_by_multiple_pages(void)
{
    auto lines =
        std::unique_ptr<UI::ParamsUpDownSteps>(new UI::ParamsUpDownSteps(2 * ViewManager::VMIface::NUMBER_OF_LINES_ON_DISPLAY));

    mock_view->expect_input_with_callback(ViewIface::InputResult::UPDATE_NEEDED,
                                          DrcpCommand::SCROLL_DOWN_MANY,
                                          lines.get(), check_equal_lines_parameter);
    mock_view->expect_update(*views_output);
    mock_view->expect_write_xml_begin(true, false);

    check_equal_lines_parameter_called = false;
    vm->input_move_cursor_by_page(2);
    cut_assert_true(check_equal_lines_parameter_called);

    vm->serialization_result(DCP::Transaction::OK);

    check_and_clear_ostream("Mock update\n", *views_output);
}

};

namespace view_manager_tests_multiple_views
{

static void populate_view_manager(ViewManager::Manager &vm,
                                  std::array<ViewMock::View *, 4> &all_views)
{
    static const struct
    {
        const char *name;
        const bool is_browse_view;
    }
    names[] =
    {
        { "First",  true, },
        { "Second", true, },
        { "Third",  false, },
        { "Fourth", false, },
    };

    for(size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i)
    {
        ViewMock::View *view =
            new ViewMock::View(names[i].name, names[i].is_browse_view);

        cut_assert_true(view->init());
        cut_assert_true(vm.add_view(view));
        view->check();

        all_views[i] = view;
    }
}

static MockMessages *mock_messages;
static std::array<ViewMock::View *, 4> all_mock_views;
static DCP::Queue *queue;
static ViewManager::Manager *vm;
static std::ostringstream *views_output;

void cut_setup(void)
{
    views_output = new std::ostringstream();
    cppcut_assert_not_null(views_output);

    mock_messages = new MockMessages();
    cppcut_assert_not_null(mock_messages);
    mock_messages->init();
    mock_messages_singleton = mock_messages;

    queue = new DCP::Queue(dcp_transaction_setup_timeout, dcp_deferred_tx);
    cppcut_assert_not_null(queue);

    vm = new ViewManager::Manager(*queue);
    cppcut_assert_not_null(vm);

    mock_messages->ignore_all_ = true;
    all_mock_views.fill(nullptr);
    populate_view_manager(*vm,  all_mock_views);
    all_mock_views[0]->ignore_all_ = true;
    vm->activate_view_by_name("First");
    vm->serialization_result(DCP::Transaction::OK);
    all_mock_views[0]->ignore_all_ = false;
    mock_messages->ignore_all_ = false;

    vm->set_output_stream(*views_output);
}

void cut_teardown(void)
{
    cppcut_assert_equal("", views_output->str().c_str());
    cut_assert_true(queue->is_idle());

    mock_messages->check();

    for(auto view: all_mock_views)
        view->check();

    delete mock_messages;
    delete vm;
    delete queue;
    delete views_output;

    mock_messages = nullptr;
    vm = nullptr;
    queue = nullptr;
    views_output = nullptr;

    for(auto view: all_mock_views)
        delete view;

    all_mock_views.fill(nullptr);
}

/*!\test
 * Look up non-existent view in multiple views returns null pointer.
 */
void test_get_nonexistent_view_by_name_fails(void)
{
    cut_assert_null(vm->get_view_by_name("DoesNotExist"));
}

/*!\test
 * Look up existent view in multiple views returns non-null pointer.
 */
void test_get_existent_view_by_name_returns_view_interface(void)
{
    cut_assert_not_null(vm->get_view_by_name("First"));
    cut_assert_not_null(vm->get_view_by_name("Second"));
    cut_assert_not_null(vm->get_view_by_name("Third"));
    cut_assert_not_null(vm->get_view_by_name("Fourth"));
}

/*!\test
 * Activating an active view serializes the view.
 */
void test_reactivate_active_view_serializes_the_view_again(void)
{
    mock_messages->expect_msg_info_formatted("Requested to activate view \"First\"");

    all_mock_views[0]->expect_defocus();

    all_mock_views[0]->expect_focus();
    all_mock_views[0]->expect_serialize(*views_output);
    all_mock_views[0]->expect_write_xml_begin(true, true);

    vm->activate_view_by_name("First");
    vm->serialization_result(DCP::Transaction::OK);

    check_and_clear_ostream("First serialize\n", *views_output);
}

/*!\test
 * Activating a view with unknown name does not disturb the view.
 */
void test_activate_nonexistent_view_does_nothing(void)
{
    mock_messages->expect_msg_info_formatted("Requested to activate view \"DoesNotExist\"");
    vm->activate_view_by_name("DoesNotExist");
}

/*!\test
 * Activating the NOP view does not disturb the view.
 */
void test_activate_nop_view_does_nothing(void)
{
    mock_messages->expect_msg_info_formatted("Requested to activate view \"#NOP\"");
    vm->activate_view_by_name(ViewNames::NOP);
}

/*!\test
 * Activating a view takes the focus from one view and gives it to the other.
 */
void test_activate_different_view(void)
{
    mock_messages->expect_msg_info_formatted("Requested to activate view \"Second\"");

    all_mock_views[0]->expect_defocus();

    all_mock_views[1]->expect_focus();
    all_mock_views[1]->expect_serialize(*views_output);
    all_mock_views[1]->expect_write_xml_begin(true, true);

    vm->activate_view_by_name("Second");
    vm->serialization_result(DCP::Transaction::OK);

    check_and_clear_ostream("Second serialize\n", *views_output);
}

/*!\test
 * Command sent to view manager is sent to the active view, the view tells that
 * there is nothing to do.
 */
void test_input_command_with_no_need_to_refresh(void)
{
    mock_messages->expect_msg_info("Dispatching DRCP command %d%s");
    all_mock_views[0]->expect_input(ViewIface::InputResult::OK,
                                    DrcpCommand::PLAYBACK_START, false);
    vm->input(DrcpCommand::PLAYBACK_START, nullptr);
}

/*!\test
 * Command sent to view manager is sent to the active view, the view tells
 * that the display content needs be updated.
 */
void test_input_command_with_need_to_refresh(void)
{
    mock_messages->expect_msg_info("Dispatching DRCP command %d%s");
    all_mock_views[0]->expect_input(ViewIface::InputResult::UPDATE_NEEDED,
                                    DrcpCommand::PLAYBACK_START, false);
    all_mock_views[0]->expect_update(*views_output);
    all_mock_views[0]->expect_write_xml_begin(true, false);
    vm->input(DrcpCommand::PLAYBACK_START, nullptr);
    vm->serialization_result(DCP::Transaction::OK);

    check_and_clear_ostream("First update\n", *views_output);
}

/*!\test
 * Current view indicates it needs to be hidden, but the request is ignored
 * because there is no previous browse view.
 */
void test_input_command_with_need_to_hide_view_may_fail(void)
{
    mock_messages->expect_msg_info("Dispatching DRCP command %d%s");
    all_mock_views[0]->expect_input(ViewIface::InputResult::SHOULD_HIDE,
                                    DrcpCommand::PLAYBACK_START, false);
    vm->input(DrcpCommand::PLAYBACK_START, nullptr);
}

/*!\test
 * Current non-browse view indicates it needs to be hidden, works because there
 * is a previous browse view.
 */
void test_input_command_with_need_to_hide_nonbrowse_view(void)
{
    /* switch over from first to a non-browser view */
    mock_messages->expect_msg_info_formatted("Requested to activate view \"Third\"");
    all_mock_views[0]->expect_defocus();
    all_mock_views[2]->expect_focus();
    all_mock_views[2]->expect_serialize(*views_output);
    all_mock_views[2]->expect_write_xml_begin(true, true);
    vm->activate_view_by_name("Third");
    vm->serialization_result(DCP::Transaction::OK);
    check_and_clear_ostream("Third serialize\n", *views_output);

    /* hide request from active view, view manager switches back to previous
     * browse view in turn (view "First") */
    mock_messages->expect_msg_info("Dispatching DRCP command %d%s");
    all_mock_views[2]->expect_input(ViewIface::InputResult::SHOULD_HIDE,
                                    DrcpCommand::PLAYBACK_START, false);
    all_mock_views[2]->expect_defocus();
    all_mock_views[0]->expect_focus();
    all_mock_views[0]->expect_serialize(*views_output);
    all_mock_views[0]->expect_write_xml_begin(true, true);
    vm->input(DrcpCommand::PLAYBACK_START, nullptr);
    vm->serialization_result(DCP::Transaction::OK);
    check_and_clear_ostream("First serialize\n", *views_output);
}

/*!\test
 * Current browse view indicates it needs to be hidden, but this never works
 * because browse views are expected to actively switch between views.
 */
void test_input_command_with_need_to_hide_browse_view_never_works(void)
{
    /* switch over from first to a non-browser view */
    mock_messages->expect_msg_info_formatted("Requested to activate view \"Second\"");
    all_mock_views[0]->expect_defocus();
    all_mock_views[1]->expect_focus();
    all_mock_views[1]->expect_serialize(*views_output);
    all_mock_views[1]->expect_write_xml_begin(true, true);
    vm->activate_view_by_name("Second");
    vm->serialization_result(DCP::Transaction::OK);
    check_and_clear_ostream("Second serialize\n", *views_output);

    /* hide request from active view, but view manager won't switch focus */
    mock_messages->expect_msg_info("Dispatching DRCP command %d%s");
    all_mock_views[1]->expect_input(ViewIface::InputResult::SHOULD_HIDE,
                                    DrcpCommand::PLAYBACK_START, false);
    vm->input(DrcpCommand::PLAYBACK_START, nullptr);
}

static bool check_equal_parameters_by_pointer_called;
static void check_equal_parameters_by_pointer(const UI::Parameters *expected_parameters,
                                              const std::unique_ptr<const UI::Parameters> &actual_parameters)
{
    check_equal_parameters_by_pointer_called = true;
    cppcut_assert_equal(expected_parameters, actual_parameters.get());
}

/*!\test
 * Passing data into the user interface.
 */
void test_input_command_with_data(void)
{
    ViewMock::View view("Play", false);
    cut_assert_true(view.init());
    cut_assert_true(vm->add_view(&view));

    auto speed_factor =
        std::unique_ptr<UI::ParamsFWSpeed>(new UI::ParamsFWSpeed(12.5));
    view.expect_input_with_callback(ViewIface::InputResult::OK,
                                    DrcpCommand::FAST_WIND_SET_SPEED,
                                    speed_factor.get(), check_equal_parameters_by_pointer);

    mock_messages->expect_msg_info("Dispatching DRCP command %d%s");

    check_equal_parameters_by_pointer_called = false;
    vm->input(DrcpCommand::FAST_WIND_SET_SPEED, std::move(speed_factor));
    cut_assert_true(check_equal_parameters_by_pointer_called);
}

class DummyTypeForTesting
{
  public:
    DummyTypeForTesting(const DummyTypeForTesting &) = delete;
    DummyTypeForTesting &operator=(const DummyTypeForTesting &) = delete;

    explicit DummyTypeForTesting() {}
};

/*!\test
 * In case we are messing up and pass the wrong kind of data to an input
 * command, the command handler is responsible for handling the situation.
 */
void test_input_command_with_unexpected_data(void)
{
    ViewMock::View view("Play", false);
    cut_assert_true(view.init());
    cut_assert_true(vm->add_view(&view));

    auto completely_unexpected_data =
        std::unique_ptr<UI::SpecificParameters<DummyTypeForTesting *>>(new UI::SpecificParameters<DummyTypeForTesting *>(nullptr));
    view.expect_input_with_callback(ViewIface::InputResult::OK,
                                    DrcpCommand::FAST_WIND_SET_SPEED,
                                    completely_unexpected_data.get(),
                                    check_equal_parameters_by_pointer);

    mock_messages->expect_msg_info("Dispatching DRCP command %d%s");

    check_equal_parameters_by_pointer_called = false;
    vm->input(DrcpCommand::FAST_WIND_SET_SPEED, std::move(completely_unexpected_data));
    cut_assert_true(check_equal_parameters_by_pointer_called);
}

/*!\test
 * In case an input command requires data, but we forgot to pass it, the
 * the command handler is responsible for handling the situation.
 */
void test_input_command_with_missing_data(void)
{
    ViewMock::View view("Play", false);
    cut_assert_true(view.init());
    cut_assert_true(vm->add_view(&view));

    const UI::SpecificParameters<DummyTypeForTesting *> completely_unexpected_data(nullptr);
    view.expect_input_with_callback(ViewIface::InputResult::OK,
                                    DrcpCommand::FAST_WIND_SET_SPEED,
                                    nullptr, check_equal_parameters_by_pointer);

    mock_messages->expect_msg_info("Dispatching DRCP command %d%s");

    check_equal_parameters_by_pointer_called = false;
    vm->input(DrcpCommand::FAST_WIND_SET_SPEED);
    cut_assert_true(check_equal_parameters_by_pointer_called);
}

/*!\test
 * Toggle between two named views with recognized, different names.
 */
void test_toggle_two_views(void)
{
    mock_messages->expect_msg_info_formatted("Requested to toggle between views \"Second\" and \"Third\"");
    all_mock_views[0]->expect_defocus();
    all_mock_views[1]->expect_focus();
    all_mock_views[1]->expect_serialize(*views_output);
    all_mock_views[1]->expect_write_xml_begin(true, true);
    vm->toggle_views_by_name("Second", "Third");
    vm->serialization_result(DCP::Transaction::OK);
    check_and_clear_ostream("Second serialize\n", *views_output);

    mock_messages->expect_msg_info_formatted("Requested to toggle between views \"Second\" and \"Third\"");
    all_mock_views[1]->expect_defocus();
    all_mock_views[2]->expect_focus();
    all_mock_views[2]->expect_serialize(*views_output);
    all_mock_views[2]->expect_write_xml_begin(true, true);
    vm->toggle_views_by_name("Second", "Third");
    vm->serialization_result(DCP::Transaction::OK);
    check_and_clear_ostream("Third serialize\n", *views_output);


    mock_messages->expect_msg_info_formatted("Requested to toggle between views \"Second\" and \"Third\"");
    all_mock_views[2]->expect_defocus();
    all_mock_views[1]->expect_focus();
    all_mock_views[1]->expect_serialize(*views_output);
    all_mock_views[1]->expect_write_xml_begin(true, true);
    vm->toggle_views_by_name("Second", "Third");
    vm->serialization_result(DCP::Transaction::OK);
    check_and_clear_ostream("Second serialize\n", *views_output);
}

/*!\test
 * Toggle requests between views with the same known name activates view each
 * time.
 */
void test_toggle_views_with_same_names_switches_each_time(void)
{
    mock_messages->expect_msg_info_formatted("Requested to toggle between views \"Fourth\" and \"Fourth\"");
    all_mock_views[0]->expect_defocus();
    all_mock_views[3]->expect_focus();
    all_mock_views[3]->expect_serialize(*views_output);
    all_mock_views[3]->expect_write_xml_begin(true, true);
    vm->toggle_views_by_name("Fourth", "Fourth");
    vm->serialization_result(DCP::Transaction::OK);
    check_and_clear_ostream("Fourth serialize\n", *views_output);

    mock_messages->expect_msg_info_formatted("Requested to toggle between views \"Fourth\" and \"Fourth\"");
    all_mock_views[3]->expect_defocus();
    all_mock_views[3]->expect_focus();
    all_mock_views[3]->expect_serialize(*views_output);
    all_mock_views[3]->expect_write_xml_begin(true, true);
    vm->toggle_views_by_name("Fourth", "Fourth");
    vm->serialization_result(DCP::Transaction::OK);
    check_and_clear_ostream("Fourth serialize\n", *views_output);
}

/*!\test
 * Toggle requests between two views with an unknown and a known name (unknown
 * name in the first position) switch to the known name, nothing more.
 */
void test_toggle_views_with_first_unknown_name_switches_to_the_known_name(void)
{
    mock_messages->expect_msg_info_formatted("Requested to toggle between views \"Foo\" and \"Third\"");
    all_mock_views[0]->expect_defocus();
    all_mock_views[2]->expect_focus();
    all_mock_views[2]->expect_serialize(*views_output);
    all_mock_views[2]->expect_write_xml_begin(true, true);
    vm->toggle_views_by_name("Foo", "Third");
    vm->serialization_result(DCP::Transaction::OK);
    check_and_clear_ostream("Third serialize\n", *views_output);

    mock_messages->expect_msg_info_formatted("Requested to toggle between views \"Foo\" and \"Third\"");
    all_mock_views[2]->expect_defocus();
    all_mock_views[2]->expect_focus();
    all_mock_views[2]->expect_serialize(*views_output);
    all_mock_views[2]->expect_write_xml_begin(true, true);
    vm->toggle_views_by_name("Foo", "Third");
    vm->serialization_result(DCP::Transaction::OK);
    check_and_clear_ostream("Third serialize\n", *views_output);

    mock_messages->expect_msg_info_formatted("Requested to toggle between views \"Foo\" and \"Third\"");
    all_mock_views[2]->expect_defocus();
    all_mock_views[2]->expect_focus();
    all_mock_views[2]->expect_serialize(*views_output);
    all_mock_views[2]->expect_write_xml_begin(true, true);
    vm->toggle_views_by_name("Foo", "Third");
    vm->serialization_result(DCP::Transaction::OK);
    check_and_clear_ostream("Third serialize\n", *views_output);
}

/*!\test
 * Toggle requests between two views with an unknown and a known name (unknown
 * name in second position) switch to the known name, nothing more.
 */
void test_toggle_views_with_second_unknown_name_switches_to_the_known_name(void)
{
    mock_messages->expect_msg_info_formatted("Requested to toggle between views \"Third\" and \"Foo\"");
    all_mock_views[0]->expect_defocus();
    all_mock_views[2]->expect_focus();
    all_mock_views[2]->expect_serialize(*views_output);
    all_mock_views[2]->expect_write_xml_begin(true, true);
    vm->toggle_views_by_name("Third", "Foo");
    vm->serialization_result(DCP::Transaction::OK);
    check_and_clear_ostream("Third serialize\n", *views_output);

    mock_messages->expect_msg_info_formatted("Requested to toggle between views \"Third\" and \"Foo\"");
    vm->toggle_views_by_name("Third", "Foo");

    mock_messages->expect_msg_info_formatted("Requested to toggle between views \"Third\" and \"Foo\"");
    vm->toggle_views_by_name("Third", "Foo");
}

/*!\test
 * Toggle requests between two views with unknown names have no effect.
 */
void test_toggle_views_with_two_unknown_names_does_nothing(void)
{
    mock_messages->expect_msg_info_formatted("Requested to toggle between views \"Foo\" and \"Bar\"");
    vm->toggle_views_by_name("Foo", "Bar");

    mock_messages->expect_msg_info_formatted("Requested to toggle between views \"Foo\" and \"Bar\"");
    vm->toggle_views_by_name("Foo", "Bar");
}

};

/*!
 * Tests concerning serialization to DCPD and handling the result.
 *
 * The tests in this section show that our error handling is---to keep a
 * positive tone---rather puristic. Errors are detected, but their handling is
 * mostly restricted to logging them. There should probably some retry after
 * failure, but we'll only add this if practice shows that it is really
 * necessary to do so.
 */
namespace view_manager_tests_serialization
{

static MockMessages *mock_messages;
static DCP::Queue *queue;
static ViewManager::Manager *vm;
static std::ostringstream *views_output;
static const char standard_mock_view_name[] = "Mock";
static ViewMock::View *mock_view;

void cut_setup(void)
{
    views_output = new std::ostringstream();
    cppcut_assert_not_null(views_output);

    mock_messages = new MockMessages();
    cppcut_assert_not_null(mock_messages);
    mock_messages->init();
    mock_messages_singleton = mock_messages;

    mock_view = new ViewMock::View(standard_mock_view_name, false);
    cppcut_assert_not_null(mock_view);
    cut_assert_true(mock_view->init());

    queue = new DCP::Queue(dcp_transaction_setup_timeout, dcp_deferred_tx);
    cppcut_assert_not_null(queue);

    vm = new ViewManager::Manager(*queue);
    cppcut_assert_not_null(vm);
    vm->set_output_stream(*views_output);
    cut_assert_true(vm->add_view(mock_view));

    cut_assert_false(queue->is_in_progress());
}

void cut_teardown(void)
{
    cppcut_assert_equal("", views_output->str().c_str());
    cut_assert_true(queue->is_idle());

    mock_messages->check();
    mock_view->check();

    delete vm;
    delete queue;
    delete mock_view;
    delete mock_messages;
    delete views_output;

    mock_messages = nullptr;
    mock_view = nullptr;
    vm =nullptr;
    queue = nullptr;
    views_output = nullptr;
}

/*!\test
 * Receiving a result from DCPD while there is no active transaction is
 * considered a bug and is logged as such.
 */
void test_serialization_result_for_idle_transaction_is_logged(void)
{
    mock_messages->expect_msg_error(0, LOG_CRIT, "BUG: Received result from DCPD for idle transaction");
    vm->serialization_result(DCP::Transaction::OK);

    mock_messages->expect_msg_error(0, LOG_CRIT, "BUG: Received result from DCPD for idle transaction");
    vm->serialization_result(DCP::Transaction::FAILED);

    mock_messages->expect_msg_error(0, LOG_CRIT, "BUG: Received result from DCPD for idle transaction");
    vm->serialization_result(DCP::Transaction::TIMEOUT);

    mock_messages->expect_msg_error(0, LOG_CRIT, "BUG: Received result from DCPD for idle transaction");
    vm->serialization_result(DCP::Transaction::INVALID_ANSWER);

    mock_messages->expect_msg_error(0, LOG_CRIT, "BUG: Received result from DCPD for idle transaction");
    vm->serialization_result(DCP::Transaction::IO_ERROR);
}

static void activate_view()
{
    mock_messages->expect_msg_info_formatted("Requested to activate view \"Mock\"");
    mock_view->expect_focus();
    mock_view->expect_serialize(*views_output);
    mock_view->expect_write_xml_begin(true, true);
    vm->activate_view_by_name(standard_mock_view_name);
    check_and_clear_ostream("Mock serialize\n", *views_output);
}

/*!\test
 * If DCPD failed to handle our DRCP transaction, then this incident is logged.
 */
void test_dcpd_failed(void)
{
    activate_view();

    mock_messages->expect_msg_error(EINVAL, LOG_CRIT, "DCPD failed to handle our transaction");
    vm->serialization_result(DCP::Transaction::FAILED);
}

/*!\test
 * If DCPD did not answer our DRCP transaction within a certain amount of time,
 * then the transaction is aborted and the incident is logged.
 *
 * We consider this case as a bug, either in DCPD, in DRCPD, or both. There
 * should never be a timeout over a named pipe between any two processes, even
 * on heavily loaded systems.
 */
void test_dcpd_timeout(void)
{
    activate_view();

    mock_messages->expect_msg_error(0, LOG_CRIT, "BUG: Got no answer from DCPD");
    vm->serialization_result(DCP::Transaction::TIMEOUT);
}

/*!\test
 * Reception of junk answers from DCPD during a transaction is considered a bug
 * and is logged as such.
 */
void test_dcpd_invalid_answer(void)
{
    activate_view();

    mock_messages->expect_msg_error(0, LOG_CRIT, "BUG: Got invalid response from DCPD");
    vm->serialization_result(DCP::Transaction::INVALID_ANSWER);
}

/*!\test
 * Failing hard to read a result back from DCPD during a transaction is logged.
 */
void test_hard_io_error(void)
{
    activate_view();

    mock_messages->expect_msg_error(EIO, LOG_CRIT, "I/O error while trying to get response from DCPD");
    vm->serialization_result(DCP::Transaction::IO_ERROR);
}

/*!\test
 * Serializing a view that is already in the progress of being serialized
 * causes a new element being inserted into the DCP queue.
 */
void test_view_update_does_not_affect_ongoing_transfer()
{
    cut_assert_false(queue->is_in_progress());
    cut_assert_true(queue->is_empty());
    cut_assert_true(queue->is_idle());

    activate_view();

    cut_assert_true(queue->is_in_progress());
    cut_assert_true(queue->is_empty());
    cut_assert_false(queue->is_idle());

    mock_view->expect_defocus();
    activate_view();

    cut_assert_true(queue->is_in_progress());
    cut_assert_false(queue->is_empty());
    cut_assert_false(queue->is_idle());

    vm->serialization_result(DCP::Transaction::OK);

    cut_assert_true(queue->is_in_progress());
    cut_assert_true(queue->is_empty());
    cut_assert_false(queue->is_idle());

    vm->serialization_result(DCP::Transaction::OK);
}

};

/*!@}*/
