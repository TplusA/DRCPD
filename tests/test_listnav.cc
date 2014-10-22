#include <cppcutter.h>

#include "listnav.hh"
#include "ramlist.hh"

/*!
 * \addtogroup list_navigation_tests Unit tests
 * \ingroup list_navigation
 *
 * List navigation unit tests with visibility and selectability of items.
 */
/*!@{*/

namespace list_navigation_tests
{

/*!\test
 * Navigation should start in first line, with first line displayed first.
 */
void test_simple_navigation_init(void);

/*!\test
 * Navigation in visible lines does not change number of first line.
 */
void test_move_down_and_up_within_displayed_lines(void);

/*!\test
 * Moving beyond displayed lines scrolls the list.
 */
void test_move_down_and_up_with_scrolling(void);

/*!\test
 * We cannot select negative lines.
 */
void test_cannot_move_before_first_line(void);

/*!\test
 * We cannot select lines beyond the last one.
 */
void test_cannot_move_beyond_last_line(void);

};

namespace list_navigation_tests_with_unselectable_items
{

/*!\test
 * Navigation should start in second line, with first line displayed first.
 */
void test_navigation_init_with_first_line_unselectable(void);

/*!\test
 * First line is unselectable, so we cannot select it.
*/
void test_cannot_select_unselectable_line(void);

/*!\test
 * First line is unselectable, but it must become visible when scrolling up.
*/
void test_scroll_to_unselectable_line(void);

};

/*!@}*/


static List::RamList *list;
static const char *list_texts[] =
    { "First", "Second", "Third", "Fourth", "Fifth", "Sixth", "Seventh", };

template <size_t N>
static void check_display(const List::RamList &l, const List::Nav &nav,
                          const std::array<unsigned int, N> &expected_indices)
{
    size_t i = 0;

    for(auto it : nav)
    {
        cppcut_assert_operator(i, <, N);
        cppcut_assert_equal(expected_indices[i], it);

        const List::TextItem *item = dynamic_cast<const List::TextItem *>(l.get_item(it));

        cppcut_assert_not_null(item);
        cppcut_assert_equal(list_texts[it], item->get_text());

        ++i;
    }

    cppcut_assert_equal(N, i);
}


namespace list_navigation_tests
{

void cut_setup(void)
{
    list = new List::RamList();
    cppcut_assert_not_null(list);
    for(auto t : list_texts)
        List::append(list, List::TextItem(t, false, 0));
}

void cut_teardown(void)
{
    delete list;
    list = nullptr;
}

void test_simple_navigation_init(void)
{
    List::Nav nav(0, 0, list->get_number_of_items(), 3);

    cppcut_assert_equal(0U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 3>({0, 1, 2}));
}

void test_move_down_and_up_within_displayed_lines(void)
{
    List::Nav nav(0, 0, list->get_number_of_items(), 3);

    cut_assert_true(nav.down());
    cut_assert_true(nav.down());
    cppcut_assert_equal(2U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 3>({0, 1, 2}));

    cut_assert_true(nav.up());
    cut_assert_true(nav.up());
    cppcut_assert_equal(0U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 3>({0, 1, 2}));
}

void test_move_down_and_up_with_scrolling(void)
{
    List::Nav nav(0, 0, list->get_number_of_items(), 2);

    cut_assert_true(nav.down());
    cppcut_assert_equal(1U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 2>({0, 1}));

    cut_assert_true(nav.down());
    cppcut_assert_equal(2U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 2>({1, 2}));

    cut_assert_true(nav.down());
    cppcut_assert_equal(3U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 2>({2, 3}));

    cut_assert_true(nav.up());
    cppcut_assert_equal(2U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 2>({2, 3}));

    cut_assert_true(nav.up());
    cppcut_assert_equal(1U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 2>({1, 2}));

    cut_assert_true(nav.up());
    cppcut_assert_equal(0U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 2>({0, 1}));
}

void test_cannot_move_before_first_line(void)
{
    List::Nav nav(0, 0, list->get_number_of_items(), 2);

    cut_assert_false(nav.up());
    cppcut_assert_equal(0U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 2>({0, 1}));

    /* down works as expected, no persistent internal underflows */
    cut_assert_true(nav.down());
    cppcut_assert_equal(1U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 2>({0, 1}));
}

void test_cannot_move_beyond_last_line(void)
{
    List::Nav nav(0, 0, list->get_number_of_items(), 3);

    const unsigned int N = list->get_number_of_items() - 1;

    for(unsigned int i = 0; i < N; ++i)
        cut_assert_true(nav.down());

    cppcut_assert_equal(N, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 3>({N - 2, N - 1, N}));

    cut_assert_false(nav.down());
    cppcut_assert_equal(N, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 3>({N - 2, N - 1, N}));

    /* up works as expected, no persistent internal overflows */
    cut_assert_true(nav.up());
    cppcut_assert_equal(N - 1, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 3>({N - 2, N - 1, N}));
}

void test_navigation_init_with_first_line_unselectable(void)
{
    List::Nav nav(1, 0, list->get_number_of_items(), 3);

    cppcut_assert_equal(1U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 3>({0, 1, 2}));
}

void test_cannot_select_unselectable_line(void)
{
    List::Nav nav(1, 0, list->get_number_of_items(), 3);

    cut_assert_false(nav.up());
    cppcut_assert_equal(1U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 3>({0, 1, 2}));

    /* down works as expected, no persistent internal underflows */
    cut_assert_true(nav.down());
    cppcut_assert_equal(2U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 3>({0, 1, 2}));
}

void test_scroll_to_unselectable_line(void)
{
    List::Nav nav(1, 0, list->get_number_of_items(), 3);

    cut_assert_true(nav.down());
    cut_assert_true(nav.down());
    cppcut_assert_equal(3U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 3>({1, 2, 3}));

    /* regular case... */
    cut_assert_true(nav.up());
    cppcut_assert_equal(2U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 3>({1, 2, 3}));

    /* ...but now show first line even though the second line is selected */
    cut_assert_true(nav.up());
    cppcut_assert_equal(1U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 3>({0, 1, 2}));
}

};
