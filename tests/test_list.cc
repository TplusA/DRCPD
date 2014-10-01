#include <cppcutter.h>

#include "ramlist.hh"

/*!
 * \addtogroup ram_list_tests Unit tests
 * \ingroup ram_list
 *
 * List interface unit tests.
 */
/*!@{*/

namespace ram_list_tests
{

/*!\test
 * After initialization, the list shall be empty.
 */
void test_list_is_empty_on_startup(void);

};

/*!@}*/


namespace ram_list_tests
{

static List::RamList *list;

void cut_setup(void)
{
    list = new List::RamList();
    cut_assert_not_null(list);
}

void cut_teardown(void)
{
    delete list;
}

void test_list_is_empty_on_startup(void)
{
    cut_assert_equal_uint(0, list->get_number_of_items());
}

};
