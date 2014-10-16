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

};

/*!@}*/


namespace view_manager_tests
{

static MockMessages mock_messages;
static ViewManager *vm;
static std::ostringstream views_output;
static std::string standard_mock_view_name("Mock");

void cut_setup(void)
{
    views_output.clear();

    mock_messages_singleton = &mock_messages;
    mock_messages.init();

    vm = new ViewManager();
    vm->set_output_stream(views_output);
}

void cut_teardown(void)
{
    mock_messages.check();
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

    cppcut_assert_equal("Mock serialize\n", views_output.str().c_str());
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
    views_output.clear();

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

};
