#include <cppcutter.h>
#include <sstream>

#include "view_manager.hh"
#include "view_nop.hh"
#include "view_mock.hh"
#include "mock_messages.hh"

/*!
 * \addtogroup view_manager_tests Unit tests
 * \ingroup view_manager
 *
 * View manager unit tests.
 */
/*!@{*/

namespace view_manager_tests
{

/*!\test
 * Attempt to add nothingness to the views is handled and leads to failure.
 */
void test_add_nullptr_view_fails(void);

/*!\test
 * Attempt to add a NOP view is rejected and leads to failure.
 */
void test_add_nop_view_fails(void);

/*!\test
 * Adding a regular view to a fresh view manager works.
 */
void test_add_view(void);

/*!\test
 * Attempt to add views with the same name only works for the first attempt.
 */
void test_add_views_with_same_name_fails(void);

/*!\test
 * Adding a regular view to a fresh view manager and activating it works.
 */
void test_add_view_and_activate(void);

};

namespace view_manager_tests_multiple_views
{

/*!\test
 * Activating an active view does not disturb the view.
 */
void test_reactivate_active_view_does_nothing(void);

/*!\test
 * Activating a view with unknown name does not disturb the view.
 */
void test_activate_nonexistent_view_does_nothing(void);

/*!\test
 * Activating the NOP view does not disturb the view.
 */
void test_activate_nop_view_does_nothing(void);

/*!\test
 * Activating a view takes the focus from one view and gives it to the other.
 */
void test_activate_different_view(void);

/*!\test
 * Command sent to view manager is sent to the active view, the view tells that
 * there is nothing to do.
 */
void test_input_command_with_no_need_to_refresh(void);

/*!\test
 * Commands sent to view manager is sent to the active view, the view tells
 * that the display content needs be updated.
 */
void test_input_command_with_need_to_refresh(void);

/*!\test
 * Commands sent to view manager is sent to the active view, the view tells
 * that it should be removed from screen.
 */
void test_input_command_with_need_to_hide_view(void);

/*!\test
 * Toggle between two named views with recognized, different names.
 */
void test_toggle_two_views(void);

/*!\test
 * Toggle requests between views with the same known name have no effect,
 * except initial switching.
 */
void test_toggle_views_with_same_names_switches_once(void);

/*!\test
 * Toggle requests between two views with an unknown and a known name switch to
 * the known name, nothing more.
 */
void test_toggle_views_with_one_unknown_name_switches_to_the_known_name(void);

/*!\test
 * Toggle requests between two views with unknown names have no effect.
 */
void test_toggle_views_with_two_unknown_names_does_nothing(void);

};

/*!@}*/


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


namespace view_manager_tests
{

static MockMessages mock_messages;
static ViewManager *vm;
static std::ostringstream views_output;
static std::string standard_mock_view_name("Mock");

void cut_setup(void)
{
    clear_ostream(views_output);

    mock_messages_singleton = &mock_messages;
    mock_messages.init();

    vm = new ViewManager();
    vm->set_output_stream(views_output);
}

void cut_teardown(void)
{
    mock_messages.check();
    cppcut_assert_equal("", views_output.str().c_str());
    delete vm;
}

void test_add_nullptr_view_fails(void)
{
    cut_assert_false(vm->add_view(nullptr));
}

void test_add_nop_view_fails(void)
{
    ViewNop::View view;

    cut_assert_true(view.init());
    cut_assert_false(vm->add_view(&view));
}

void test_add_view(void)
{
    ViewMock::View view(standard_mock_view_name);

    cut_assert_true(view.init());
    cut_assert_true(vm->add_view(&view));
    view.check();
}

void test_add_views_with_same_name_fails(void)
{
    ViewMock::View view(standard_mock_view_name);

    cut_assert_true(view.init());
    cut_assert_true(vm->add_view(&view));
    cut_assert_false(vm->add_view(&view));
    view.check();
}

void test_add_view_and_activate(void)
{
    ViewMock::View view(standard_mock_view_name);

    cut_assert_true(view.init());
    cut_assert_true(vm->add_view(&view));
    view.check();

    mock_messages.expect_msg_info_formatted("Requested to activate view \"Mock\"");
    view.expect_focus();
    view.expect_serialize(views_output);
    vm->activate_view_by_name(standard_mock_view_name.c_str());
    view.check();

    check_and_clear_ostream("Mock serialize\n", views_output);
}

};

namespace view_manager_tests_multiple_views
{

static void populate_view_manager(ViewManager &vm,
                                  std::array<ViewMock::View *, 4> &all_views)
{
    static const char *names[] =
    {
        "First",
        "Second",
        "Third",
        "Fourth",
    };

    for(int i = 0; i < sizeof(names) / sizeof(names[0]); ++i)
    {
        ViewMock::View *view = new ViewMock::View(names[i]);

        cut_assert_true(view->init());
        cut_assert_true(vm.add_view(view));
        view->check();

        all_views[i] = view;
    }
}

static MockMessages mock_messages;
static std::array<ViewMock::View *, 4> all_mock_views;
static ViewManager *vm;
static std::ostringstream views_output;

void cut_setup(void)
{
    clear_ostream(views_output);

    mock_messages_singleton = &mock_messages;
    mock_messages.init();

    vm = new ViewManager();

    mock_messages.ignore_all_ = true;
    all_mock_views.fill(nullptr);
    populate_view_manager(*vm,  all_mock_views);
    all_mock_views[0]->ignore_all_ = true;
    vm->activate_view_by_name("First");
    all_mock_views[0]->ignore_all_ = false;
    mock_messages.ignore_all_ = false;

    vm->set_output_stream(views_output);
}

void cut_teardown(void)
{
    cppcut_assert_equal("", views_output.str().c_str());

    mock_messages.check();

    for(auto view: all_mock_views)
        view->check();

    delete vm;

    for(auto view: all_mock_views)
        delete view;
}

void test_reactivate_active_view_does_nothing(void)
{
    mock_messages.expect_msg_info_formatted("Requested to activate view \"First\"");
    vm->activate_view_by_name("First");
}

void test_activate_nonexistent_view_does_nothing(void)
{
    mock_messages.expect_msg_info_formatted("Requested to activate view \"DoesNotExist\"");
    vm->activate_view_by_name("DoesNotExist");
}

void test_activate_nop_view_does_nothing(void)
{
    mock_messages.expect_msg_info_formatted("Requested to activate view \"#NOP\"");
    vm->activate_view_by_name("#NOP");
}

void test_activate_different_view(void)
{
    mock_messages.expect_msg_info_formatted("Requested to activate view \"Second\"");

    all_mock_views[0]->expect_defocus();

    all_mock_views[1]->expect_focus();
    all_mock_views[1]->expect_serialize(views_output);

    vm->activate_view_by_name("Second");

    check_and_clear_ostream("Second serialize\n", views_output);
}

void test_input_command_with_no_need_to_refresh(void)
{
    mock_messages.expect_msg_info("Dispatching DRCP command %d");
    all_mock_views[0]->expect_input_return(DrcpCommand::PLAYBACK_START,
                                           ViewIface::InputResult::OK);
    vm->input(DrcpCommand::PLAYBACK_START);
}

void test_input_command_with_need_to_refresh(void)
{
    mock_messages.expect_msg_info("Dispatching DRCP command %d");
    all_mock_views[0]->expect_input_return(DrcpCommand::PLAYBACK_START,
                                           ViewIface::InputResult::UPDATE_NEEDED);
    all_mock_views[0]->expect_update(views_output);
    vm->input(DrcpCommand::PLAYBACK_START);

    check_and_clear_ostream("First update\n", views_output);
}

void test_input_command_with_need_to_hide_view(void)
{
    mock_messages.expect_msg_info("Dispatching DRCP command %d");
    all_mock_views[0]->expect_input_return(DrcpCommand::PLAYBACK_START,
                                           ViewIface::InputResult::SHOULD_HIDE);
    all_mock_views[0]->expect_defocus();
    vm->input(DrcpCommand::PLAYBACK_START);
}

void test_toggle_two_views(void)
{
    mock_messages.expect_msg_info_formatted("Requested to toggle between views \"Second\" and \"Third\"");
    all_mock_views[0]->expect_defocus();
    all_mock_views[1]->expect_focus();
    all_mock_views[1]->expect_serialize(views_output);
    vm->toggle_views_by_name("Second", "Third");
    check_and_clear_ostream("Second serialize\n", views_output);

    mock_messages.expect_msg_info_formatted("Requested to toggle between views \"Second\" and \"Third\"");
    all_mock_views[1]->expect_defocus();
    all_mock_views[2]->expect_focus();
    all_mock_views[2]->expect_serialize(views_output);
    vm->toggle_views_by_name("Second", "Third");
    check_and_clear_ostream("Third serialize\n", views_output);


    mock_messages.expect_msg_info_formatted("Requested to toggle between views \"Second\" and \"Third\"");
    all_mock_views[2]->expect_defocus();
    all_mock_views[1]->expect_focus();
    all_mock_views[1]->expect_serialize(views_output);
    vm->toggle_views_by_name("Second", "Third");
    check_and_clear_ostream("Second serialize\n", views_output);
}

void test_toggle_views_with_same_names_switches_once(void)
{
    mock_messages.expect_msg_info_formatted("Requested to toggle between views \"Fourth\" and \"Fourth\"");
    all_mock_views[0]->expect_defocus();
    all_mock_views[3]->expect_focus();
    all_mock_views[3]->expect_serialize(views_output);
    vm->toggle_views_by_name("Fourth", "Fourth");
    check_and_clear_ostream("Fourth serialize\n", views_output);

    mock_messages.expect_msg_info_formatted("Requested to toggle between views \"Fourth\" and \"Fourth\"");
    vm->toggle_views_by_name("Fourth", "Fourth");
}

void test_toggle_views_with_one_unknown_name_switches_to_the_known_name(void)
{
    mock_messages.expect_msg_info_formatted("Requested to toggle between views \"Third\" and \"Foo\"");
    all_mock_views[0]->expect_defocus();
    all_mock_views[2]->expect_focus();
    all_mock_views[2]->expect_serialize(views_output);
    vm->toggle_views_by_name("Third", "Foo");
    check_and_clear_ostream("Third serialize\n", views_output);

    mock_messages.expect_msg_info_formatted("Requested to toggle between views \"Third\" and \"Foo\"");
    vm->toggle_views_by_name("Third", "Foo");

    mock_messages.expect_msg_info_formatted("Requested to toggle between views \"Third\" and \"Foo\"");
    vm->toggle_views_by_name("Third", "Foo");
}

void test_toggle_views_with_two_unknown_names_does_nothing(void)
{
    mock_messages.expect_msg_info_formatted("Requested to toggle between views \"Foo\" and \"Bar\"");
    vm->toggle_views_by_name("Foo", "Bar");

    mock_messages.expect_msg_info_formatted("Requested to toggle between views \"Foo\" and \"Bar\"");
    vm->toggle_views_by_name("Foo", "Bar");
}

};
