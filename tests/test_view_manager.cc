#include <cppcutter.h>

#include "view_manager.hh"
#include "view_nop.hh"

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

};

/*!@}*/


namespace view_manager_tests
{

static ViewManager *vm;

void cut_setup(void)
{
    vm = new ViewManager();
}

void cut_teardown(void)
{
    delete vm;
}

void test_add_views_with_same_name_fails(void)
{
    cut_assert_false(vm->add_view(nullptr));
}

void test_add_nop_view_fails(void)
{
    ViewNop::View view;
    cut_assert_false(vm->add_view(&view));
}

};
