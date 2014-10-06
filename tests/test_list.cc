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

/*!\test
 * Appending a single item to an empty RAM-based list works.
 */
void test_add_single_list_item(void);

/*!\test
 * Appending a few items to an empty RAM-based list works.
 */
void test_add_multiple_list_item(void);

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

void test_add_single_list_item(void)
{
    unsigned int line =
        list->append(List::Item("Test entry", false, 0));

    cut_assert_equal_uint(0, line);
    cut_assert_equal_uint(1, list->get_number_of_items());

    const List::Item *item = list->get_item(line);

    cut_assert_not_null(item);
    cut_assert_equal_string("Test entry", item->get_text());
}

void test_add_multiple_list_item(void)
{
    static const char *strings[] = { "first", "second", "foo", "bar" };

    for(unsigned int i = 0; i < sizeof(strings) / sizeof(strings[0]); ++i)
    {
        unsigned int line =
            list->append(List::Item(strings[i], false, 0));

        cut_assert_equal_uint(i, line);
        cut_assert_equal_uint(i + 1, list->get_number_of_items());
    }

    for(unsigned int i = 0; i < sizeof(strings) / sizeof(strings[0]); ++i)
    {
        const List::Item *item = list->get_item(i);

        cut_assert_not_null(item);
        cut_assert_equal_string(strings[i], item->get_text());
    }
}

};
