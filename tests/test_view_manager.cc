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

class DummyViewSignals: public ViewSignalsIface
{
  public:
    DummyViewSignals(const DummyViewSignals &) = delete;
    DummyViewSignals &operator=(const DummyViewSignals &) = delete;

    explicit DummyViewSignals() {}

    void request_display_update(ViewIface *view) override
    {
        cut_fail("Unexpected call of request_display_update()");
    }

    void request_hide_view(ViewIface *view) override
    {
        cut_fail("Unexpected call of request_hide_view()");
    }
};

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

static void dcp_transaction_observer(DcpTransaction::state)
{
    /* nothing */
}

static const std::function<void(DcpTransaction::state)> transaction_observer(dcp_transaction_observer);

namespace view_manager_tests_basics
{

static MockMessages *mock_messages;
static DcpTransaction *dcpd;
static ViewManager *vm;
static std::ostringstream *views_output;
static const char standard_mock_view_name[] = "Mock";
static DummyViewSignals dummy_view_signals;

void cut_setup(void)
{
    views_output = new std::ostringstream();
    cppcut_assert_not_null(views_output);

    mock_messages = new MockMessages();
    mock_messages->init();
    mock_messages_singleton = mock_messages;

    dcpd = new DcpTransaction(transaction_observer);
    cppcut_assert_not_null(dcpd);

    vm = new ViewManager(*dcpd);
    cppcut_assert_not_null(vm);
    vm->set_output_stream(*views_output);
}

void cut_teardown(void)
{
    mock_messages->check();
    cppcut_assert_equal("", views_output->str().c_str());

    delete mock_messages;
    delete vm;
    delete dcpd;
    delete views_output;

    mock_messages = nullptr;
    vm =nullptr;
    dcpd = nullptr;
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
    ViewNop::View view(&dummy_view_signals);

    cut_assert_true(view.init());
    cut_assert_false(vm->add_view(&view));
}

/*!\test
 * Adding a regular view to a fresh view manager works.
 */
void test_add_view(void)
{
    ViewMock::View view(standard_mock_view_name, false, &dummy_view_signals);

    cut_assert_true(view.init());
    cut_assert_true(vm->add_view(&view));
    view.check();
}

/*!\test
 * Attempt to add views with the same name only works for the first attempt.
 */
void test_add_views_with_same_name_fails(void)
{
    ViewMock::View view(standard_mock_view_name, false, &dummy_view_signals);

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
    ViewMock::View view(standard_mock_view_name, false, &dummy_view_signals);

    cut_assert_true(view.init());
    cut_assert_true(vm->add_view(&view));
    view.check();

    mock_messages->expect_msg_info_formatted("Requested to activate view \"Mock\"");
    view.expect_focus();
    view.expect_serialize(*views_output);
    vm->activate_view_by_name(standard_mock_view_name);
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
    ViewMock::View view(standard_mock_view_name, false, &dummy_view_signals);

    cut_assert_true(view.init());
    cut_assert_true(vm->add_view(&view));
    cut_assert_not_null(vm->get_view_by_name(standard_mock_view_name));
    view.check();
}

};

namespace view_manager_tests
{

static MockMessages *mock_messages;
static DcpTransaction *dcpd;
static ViewManager *vm;
static std::ostringstream *views_output;
static const char standard_mock_view_name[] = "Mock";
static ViewMock::View *mock_view;
static DummyViewSignals dummy_view_signals;

void cut_setup(void)
{
    views_output = new std::ostringstream();
    cppcut_assert_not_null(views_output);

    mock_messages = new MockMessages();
    cppcut_assert_not_null(mock_messages);
    mock_messages->init();
    mock_messages_singleton = mock_messages;

    mock_view = new ViewMock::View(standard_mock_view_name, false, &dummy_view_signals);
    cppcut_assert_not_null(mock_view);
    cut_assert_true(mock_view->init());

    dcpd = new DcpTransaction(transaction_observer);
    cppcut_assert_not_null(dcpd);

    vm = new ViewManager(*dcpd);
    cppcut_assert_not_null(vm);
    vm->set_output_stream(*views_output);
    cut_assert_true(vm->add_view(mock_view));

    mock_messages->ignore_all_ = true;
    mock_view->ignore_all_ = true;
    vm->activate_view_by_name(standard_mock_view_name);
    mock_view->ignore_all_ = false;
    mock_messages->ignore_all_ = false;
}

void cut_teardown(void)
{
    mock_messages->check();
    mock_view->check();
    cppcut_assert_equal("", views_output->str().c_str());

    delete vm;
    delete dcpd;
    delete mock_view;
    delete mock_messages;
    delete views_output;

    mock_messages = nullptr;
    mock_view = nullptr;
    vm =nullptr;
    dcpd = nullptr;
    views_output = nullptr;
}

/*!\test
 * Requests to move the cursor by zero lines have no effect.
 */
void test_move_cursor_by_zero_lines(void)
{
    vm->input_move_cursor_by_line(0);
}

/*!\test
 * Requests to move the cursor by multiple lines up are transformed into
 * multiple virtual key presses for the current view.
 *
 * There is only a single update call in the end.
 */
void test_move_cursor_up_by_multiple_lines(void)
{
    mock_view->expect_input_return(DrcpCommand::SCROLL_UP_ONE,
                                   ViewIface::InputResult::UPDATE_NEEDED);
    mock_view->expect_input_return(DrcpCommand::SCROLL_UP_ONE,
                                   ViewIface::InputResult::UPDATE_NEEDED);
    mock_view->expect_update(*views_output);

    vm->input_move_cursor_by_line(-2);

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
    mock_view->expect_input_return(DrcpCommand::SCROLL_DOWN_ONE,
                                   ViewIface::InputResult::UPDATE_NEEDED);
    mock_view->expect_input_return(DrcpCommand::SCROLL_DOWN_ONE,
                                   ViewIface::InputResult::UPDATE_NEEDED);
    mock_view->expect_input_return(DrcpCommand::SCROLL_DOWN_ONE,
                                   ViewIface::InputResult::UPDATE_NEEDED);
    mock_view->expect_update(*views_output);

    vm->input_move_cursor_by_line(3);

    check_and_clear_ostream("Mock update\n", *views_output);
}

/*!\test
 * If the view indicates that after an input nothing has changed, then upwards
 * cursor movement is stopped.
 */
void test_move_cursor_by_multiple_lines_up_stops_at_beginning_of_list(void)
{
    mock_view->expect_input_return(DrcpCommand::SCROLL_UP_ONE,
                                   ViewIface::InputResult::UPDATE_NEEDED);
    mock_view->expect_input_return(DrcpCommand::SCROLL_UP_ONE,
                                   ViewIface::InputResult::OK);
    mock_view->expect_update(*views_output);

    vm->input_move_cursor_by_line(-5);

    check_and_clear_ostream("Mock update\n", *views_output);
}

/*!\test
 * If the view indicates that after an input nothing has changed, then
 * downwards cursor movement is stopped.
 */
void test_move_cursor_by_multiple_lines_down_stops_at_end_of_list(void)
{
    mock_view->expect_input_return(DrcpCommand::SCROLL_DOWN_ONE,
                                   ViewIface::InputResult::UPDATE_NEEDED);
    mock_view->expect_input_return(DrcpCommand::SCROLL_DOWN_ONE,
                                   ViewIface::InputResult::OK);
    mock_view->expect_update(*views_output);

    vm->input_move_cursor_by_line(5);

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
    mock_view->expect_input_return(DrcpCommand::SCROLL_PAGE_UP,
                                   ViewIface::InputResult::UPDATE_NEEDED);
    mock_view->expect_input_return(DrcpCommand::SCROLL_PAGE_UP,
                                   ViewIface::InputResult::UPDATE_NEEDED);
    mock_view->expect_input_return(DrcpCommand::SCROLL_PAGE_UP,
                                   ViewIface::InputResult::UPDATE_NEEDED);
    mock_view->expect_input_return(DrcpCommand::SCROLL_PAGE_UP,
                                   ViewIface::InputResult::UPDATE_NEEDED);
    mock_view->expect_update(*views_output);

    vm->input_move_cursor_by_page(-4);

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
    mock_view->expect_input_return(DrcpCommand::SCROLL_PAGE_DOWN,
                                   ViewIface::InputResult::UPDATE_NEEDED);
    mock_view->expect_input_return(DrcpCommand::SCROLL_PAGE_DOWN,
                                   ViewIface::InputResult::UPDATE_NEEDED);
    mock_view->expect_update(*views_output);

    vm->input_move_cursor_by_page(2);

    check_and_clear_ostream("Mock update\n", *views_output);
}

};

namespace view_manager_tests_multiple_views
{

static DummyViewSignals dummy_view_signals;

static void populate_view_manager(ViewManager &vm,
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
            new ViewMock::View(names[i].name, names[i].is_browse_view,
                               &dummy_view_signals);

        cut_assert_true(view->init());
        cut_assert_true(vm.add_view(view));
        view->check();

        all_views[i] = view;
    }
}

static MockMessages *mock_messages;
static std::array<ViewMock::View *, 4> all_mock_views;
static DcpTransaction *dcpd;
static ViewManager *vm;
static std::ostringstream *views_output;

void cut_setup(void)
{
    views_output = new std::ostringstream();
    cppcut_assert_not_null(views_output);

    mock_messages = new MockMessages();
    cppcut_assert_not_null(mock_messages);
    mock_messages->init();
    mock_messages_singleton = mock_messages;

    dcpd = new DcpTransaction(transaction_observer);
    cppcut_assert_not_null(dcpd);

    vm = new ViewManager(*dcpd);
    cppcut_assert_not_null(vm);

    mock_messages->ignore_all_ = true;
    all_mock_views.fill(nullptr);
    populate_view_manager(*vm,  all_mock_views);
    all_mock_views[0]->ignore_all_ = true;
    vm->activate_view_by_name("First");
    all_mock_views[0]->ignore_all_ = false;
    mock_messages->ignore_all_ = false;

    vm->set_output_stream(*views_output);
}

void cut_teardown(void)
{
    cppcut_assert_equal("", views_output->str().c_str());

    mock_messages->check();

    for(auto view: all_mock_views)
        view->check();

    delete mock_messages;
    delete vm;
    delete dcpd;
    delete views_output;

    mock_messages = nullptr;
    vm = nullptr;
    dcpd = nullptr;
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
 * Activating an active view does not disturb the view.
 */
void test_reactivate_active_view_does_nothing(void)
{
    mock_messages->expect_msg_info_formatted("Requested to activate view \"First\"");
    vm->activate_view_by_name("First");
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
    vm->activate_view_by_name("#NOP");
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

    vm->activate_view_by_name("Second");

    check_and_clear_ostream("Second serialize\n", *views_output);
}

/*!\test
 * Command sent to view manager is sent to the active view, the view tells that
 * there is nothing to do.
 */
void test_input_command_with_no_need_to_refresh(void)
{
    mock_messages->expect_msg_info("Dispatching DRCP command %d");
    all_mock_views[0]->expect_input_return(DrcpCommand::PLAYBACK_START,
                                           ViewIface::InputResult::OK);
    vm->input(DrcpCommand::PLAYBACK_START);
}

/*!\test
 * Command sent to view manager is sent to the active view, the view tells
 * that the display content needs be updated.
 */
void test_input_command_with_need_to_refresh(void)
{
    mock_messages->expect_msg_info("Dispatching DRCP command %d");
    all_mock_views[0]->expect_input_return(DrcpCommand::PLAYBACK_START,
                                           ViewIface::InputResult::UPDATE_NEEDED);
    all_mock_views[0]->expect_update(*views_output);
    vm->input(DrcpCommand::PLAYBACK_START);

    check_and_clear_ostream("First update\n", *views_output);
}

/*!\test
 * Current view indicates it needs to be hidden, but the request is ignored
 * because there is no previous browse view.
 */
void test_input_command_with_need_to_hide_view_may_fail(void)
{
    mock_messages->expect_msg_info("Dispatching DRCP command %d");
    all_mock_views[0]->expect_input_return(DrcpCommand::PLAYBACK_START,
                                           ViewIface::InputResult::SHOULD_HIDE);
    vm->input(DrcpCommand::PLAYBACK_START);
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
    vm->activate_view_by_name("Third");
    check_and_clear_ostream("Third serialize\n", *views_output);
    vm->serialization_result(DcpTransaction::OK);

    /* hide request from active view, view manager switches back to previous
     * browse view in turn (view "First") */
    mock_messages->expect_msg_info("Dispatching DRCP command %d");
    all_mock_views[2]->expect_input_return(DrcpCommand::PLAYBACK_START,
                                           ViewIface::InputResult::SHOULD_HIDE);
    all_mock_views[2]->expect_defocus();
    all_mock_views[0]->expect_focus();
    all_mock_views[0]->expect_serialize(*views_output);
    vm->input(DrcpCommand::PLAYBACK_START);
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
    vm->activate_view_by_name("Second");
    check_and_clear_ostream("Second serialize\n", *views_output);
    vm->serialization_result(DcpTransaction::OK);

    /* hide request from active view, but view manager won't switch focus */
    mock_messages->expect_msg_info("Dispatching DRCP command %d");
    all_mock_views[1]->expect_input_return(DrcpCommand::PLAYBACK_START,
                                           ViewIface::InputResult::SHOULD_HIDE);
    vm->input(DrcpCommand::PLAYBACK_START);
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
    vm->toggle_views_by_name("Second", "Third");
    vm->serialization_result(DcpTransaction::OK);
    check_and_clear_ostream("Second serialize\n", *views_output);

    mock_messages->expect_msg_info_formatted("Requested to toggle between views \"Second\" and \"Third\"");
    all_mock_views[1]->expect_defocus();
    all_mock_views[2]->expect_focus();
    all_mock_views[2]->expect_serialize(*views_output);
    vm->toggle_views_by_name("Second", "Third");
    vm->serialization_result(DcpTransaction::OK);
    check_and_clear_ostream("Third serialize\n", *views_output);


    mock_messages->expect_msg_info_formatted("Requested to toggle between views \"Second\" and \"Third\"");
    all_mock_views[2]->expect_defocus();
    all_mock_views[1]->expect_focus();
    all_mock_views[1]->expect_serialize(*views_output);
    vm->toggle_views_by_name("Second", "Third");
    vm->serialization_result(DcpTransaction::OK);
    check_and_clear_ostream("Second serialize\n", *views_output);
}

/*!\test
 * Toggle requests between views with the same known name have no effect,
 * except initial switching.
 */
void test_toggle_views_with_same_names_switches_once(void)
{
    mock_messages->expect_msg_info_formatted("Requested to toggle between views \"Fourth\" and \"Fourth\"");
    all_mock_views[0]->expect_defocus();
    all_mock_views[3]->expect_focus();
    all_mock_views[3]->expect_serialize(*views_output);
    vm->toggle_views_by_name("Fourth", "Fourth");
    check_and_clear_ostream("Fourth serialize\n", *views_output);

    mock_messages->expect_msg_info_formatted("Requested to toggle between views \"Fourth\" and \"Fourth\"");
    vm->toggle_views_by_name("Fourth", "Fourth");
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
    vm->toggle_views_by_name("Foo", "Third");
    check_and_clear_ostream("Third serialize\n", *views_output);

    mock_messages->expect_msg_info_formatted("Requested to toggle between views \"Foo\" and \"Third\"");
    vm->toggle_views_by_name("Foo", "Third");

    mock_messages->expect_msg_info_formatted("Requested to toggle between views \"Foo\" and \"Third\"");
    vm->toggle_views_by_name("Foo", "Third");
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
    vm->toggle_views_by_name("Third", "Foo");
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
static DcpTransaction *dcpd;
static ViewManager *vm;
static std::ostringstream *views_output;
static const char standard_mock_view_name[] = "Mock";
static ViewMock::View *mock_view;
static DummyViewSignals dummy_view_signals;

void cut_setup(void)
{
    views_output = new std::ostringstream();
    cppcut_assert_not_null(views_output);

    mock_messages = new MockMessages();
    cppcut_assert_not_null(mock_messages);
    mock_messages->init();
    mock_messages_singleton = mock_messages;

    mock_view = new ViewMock::View(standard_mock_view_name, false, &dummy_view_signals);
    cppcut_assert_not_null(mock_view);
    cut_assert_true(mock_view->init());

    dcpd = new DcpTransaction(transaction_observer);
    cppcut_assert_not_null(dcpd);

    vm = new ViewManager(*dcpd);
    cppcut_assert_not_null(vm);
    vm->set_output_stream(*views_output);
    cut_assert_true(vm->add_view(mock_view));

    cut_assert_false(dcpd->is_in_progress());
}

void cut_teardown(void)
{
    mock_messages->check();
    mock_view->check();
    cppcut_assert_equal("", views_output->str().c_str());
    cut_assert_false(dcpd->is_in_progress());

    delete vm;
    delete dcpd;
    delete mock_view;
    delete mock_messages;
    delete views_output;

    mock_messages = nullptr;
    mock_view = nullptr;
    vm =nullptr;
    dcpd = nullptr;
    views_output = nullptr;
}

/*!\test
 * Receiving a result from DCPD while there is no active transaction is
 * considered a bug and is logged as such.
 */
void test_serialization_result_for_idle_transaction_is_logged(void)
{
    mock_messages->expect_msg_error(0, LOG_CRIT, "BUG: Received result from DCPD for idle transaction");
    vm->serialization_result(DcpTransaction::OK);

    mock_messages->expect_msg_error(0, LOG_CRIT, "BUG: Received result from DCPD for idle transaction");
    vm->serialization_result(DcpTransaction::FAILED);

    mock_messages->expect_msg_error(0, LOG_CRIT, "BUG: Received result from DCPD for idle transaction");
    vm->serialization_result(DcpTransaction::TIMEOUT);

    mock_messages->expect_msg_error(0, LOG_CRIT, "BUG: Received result from DCPD for idle transaction");
    vm->serialization_result(DcpTransaction::INVALID_ANSWER);

    mock_messages->expect_msg_error(0, LOG_CRIT, "BUG: Received result from DCPD for idle transaction");
    vm->serialization_result(DcpTransaction::IO_ERROR);
}

static void activate_view()
{
    mock_messages->expect_msg_info_formatted("Requested to activate view \"Mock\"");
    mock_view->expect_focus();
    mock_view->expect_serialize(*views_output);
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
    vm->serialization_result(DcpTransaction::FAILED);
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
    vm->serialization_result(DcpTransaction::TIMEOUT);
}

/*!\test
 * Reception of junk answers from DCPD during a transaction is considered a bug
 * and is logged as such.
 */
void test_dcpd_invalid_answer(void)
{
    activate_view();

    mock_messages->expect_msg_error(0, LOG_CRIT, "BUG: Got invalid response from DCPD");
    vm->serialization_result(DcpTransaction::INVALID_ANSWER);
}

/*!\test
 * Failing hard to read a result back from DCPD during a transaction is logged.
 */
void test_hard_io_error(void)
{
    activate_view();

    mock_messages->expect_msg_error(EIO, LOG_CRIT, "I/O error while trying to get response from DCPD");
    vm->serialization_result(DcpTransaction::IO_ERROR);
}

/*!\test
 * Failing hard to read a result back from DCPD during a transaction is logged.
 *
 * This would happen in case a view starts a transaction, but fails to commit
 * it. There will be a bug log message, and the transaction will be aborted by
 * the view manager.
 */
void test_unexpected_transaction_state(void)
{
    dcpd->start();

    mock_messages->expect_msg_error(0, LOG_CRIT, "BUG: Got OK from DCPD, but failed ending transaction");
    vm->serialization_result(DcpTransaction::OK);
}

};

/*!@}*/
