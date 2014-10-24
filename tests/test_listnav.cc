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

class NavItemFlags: public List::NavItemFilterIface
{
  public:
    static constexpr unsigned int item_is_on_top = 0x01;
    static constexpr unsigned int item_is_at_bottom = 0x02;

  private:
    bool are_cached_values_valid_;
    unsigned int cached_first_selectable_line_;
    unsigned int cached_last_selectable_line_;
    unsigned int cached_first_visible_line_;
    unsigned int cached_last_visible_line_;

    unsigned int visibility_flags_;
    unsigned int selectability_flags_;

  public:
    NavItemFlags(const NavItemFlags &) = delete;
    NavItemFlags &operator=(const NavItemFlags &) = delete;

    constexpr explicit NavItemFlags(const List::ListIface &list):
        NavItemFilterIface(&list),
        are_cached_values_valid_(false),
        cached_first_selectable_line_(0),
        cached_last_selectable_line_(0),
        cached_first_visible_line_(0),
        cached_last_visible_line_(0),
        visibility_flags_(0),
        selectability_flags_(0)
    {}

    void set_visible_mask(unsigned int flags)
    {
        if(visibility_flags_ == flags)
            return;

        visibility_flags_ = flags;
        are_cached_values_valid_ = false;
    }

    void set_selectable_mask(unsigned int flags)
    {
        if(selectability_flags_ == flags)
            return;

        selectability_flags_ = flags;
        are_cached_values_valid_ = false;
    }

    bool ensure_consistency() const override
    {
        if(are_cached_values_valid_)
            return false;

        const_cast<NavItemFlags *>(this)->update_cached_values();
        return true;
    }

    bool is_visible(unsigned int flags) const override
    {
        return !(flags & visibility_flags_);
    }

    bool is_selectable(unsigned int flags) const override
    {
        return !(flags & (selectability_flags_ | visibility_flags_));
    }

    unsigned int get_first_selectable_line() const override
    {
        if(!are_cached_values_valid_)
            const_cast<NavItemFlags *>(this)->update_cached_values();

        return cached_first_selectable_line_;
    }

    unsigned int get_last_selectable_line() const override
    {
        if(!are_cached_values_valid_)
            const_cast<NavItemFlags *>(this)->update_cached_values();

        return cached_last_selectable_line_;
    }

    unsigned int get_first_visible_line() const override
    {
        if(!are_cached_values_valid_)
            const_cast<NavItemFlags *>(this)->update_cached_values();

        return cached_first_visible_line_;
    }

    unsigned int get_last_visible_line() const override
    {
        if(!are_cached_values_valid_)
            const_cast<NavItemFlags *>(this)->update_cached_values();

        return cached_last_visible_line_;
    }

    unsigned int get_flags_for_line(unsigned int line) const override
    {
        return list_->get_item(line)->get_flags();
    }

  private:
    void update_cached_values()
    {
        const unsigned int n = list_->get_number_of_items();

        cached_first_selectable_line_ = 0;
        cached_first_visible_line_ = 0;
        cached_last_selectable_line_ = list_->get_number_of_items() - 1;
        cached_last_visible_line_ = cached_last_selectable_line_;

        bool is_first_selectable_set = false;
        bool is_first_visible_set = false;
        bool is_last_selectable_set = false;
        bool is_last_visible_set = false;

        for(unsigned int i = 0;
            i < n &&
            (!is_first_selectable_set || !is_last_selectable_set ||
             !is_first_visible_set    || !is_last_visible_set);
            ++i)
        {
            const unsigned int first_flags = list_->get_item(i)->get_flags();
            const unsigned int last_flags = list_->get_item(n - i - 1)->get_flags();

            if(!is_first_selectable_set && is_selectable(first_flags))
            {
                cached_first_selectable_line_ = i;
                is_first_selectable_set = true;
            }

            if(!is_first_visible_set && is_visible(first_flags))
            {
                cached_first_visible_line_ = i;
                is_first_visible_set = true;
            }

            if(!is_last_selectable_set && is_selectable(last_flags))
            {
                cached_last_selectable_line_ = n - i - 1;
                is_last_selectable_set = true;
            }

            if(!is_last_visible_set && is_visible(last_flags))
            {
                cached_last_visible_line_ = n - i - 1;
                is_last_visible_set = true;
            }
        }

        are_cached_values_valid_ = true;
    }
};

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

/*!\test
 * Navigation should start in first line, with first line displayed first.
 */
void test_simple_navigation_init(void)
{
    List::NavItemNoFilter no_filter(list->get_number_of_items());
    List::Nav nav(3, no_filter);

    cppcut_assert_equal(0U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 3>({0, 1, 2}));
}

/*!\test
 * Navigation in visible lines does not change number of first line.
 */
void test_move_down_and_up_within_displayed_lines(void)
{
    List::NavItemNoFilter no_filter(list->get_number_of_items());
    List::Nav nav(3, no_filter);

    cut_assert_true(nav.down());
    cut_assert_true(nav.down());
    cppcut_assert_equal(2U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 3>({0, 1, 2}));

    cut_assert_true(nav.up());
    cut_assert_true(nav.up());
    cppcut_assert_equal(0U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 3>({0, 1, 2}));
}

/*!\test
 * Moving beyond displayed lines scrolls the list.
 */
void test_move_down_and_up_with_scrolling(void)
{
    List::NavItemNoFilter no_filter(list->get_number_of_items());
    List::Nav nav(2, no_filter);

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

/*!\test
 * We cannot select negative lines.
 */
void test_cannot_move_before_first_line(void)
{
    List::NavItemNoFilter no_filter(list->get_number_of_items());
    List::Nav nav(2, no_filter);

    cut_assert_false(nav.up());
    cppcut_assert_equal(0U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 2>({0, 1}));

    /* down works as expected, no persistent internal underflows */
    cut_assert_true(nav.down());
    cppcut_assert_equal(1U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 2>({0, 1}));
}

/*!\test
 * We cannot select lines beyond the last one.
 */
void test_cannot_move_beyond_last_line(void)
{
    List::NavItemNoFilter no_filter(list->get_number_of_items());
    List::Nav nav(3, no_filter);

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

/*!\test
 * The iterator defined for unfiltered List::Nav iterates over the currently
 * shown lines, where the currently shown lines here are the first few items
 * stored in the list.
 */
void test_const_iterator_steps_through_visible_lines_from_first(void)
{
    List::NavItemNoFilter no_filter(list->get_number_of_items());
    List::Nav nav(3, no_filter);

    unsigned int expected_current_line = 0;

    for(auto it : nav)
    {
        cppcut_assert_equal(expected_current_line, it);
        ++expected_current_line;
    }

    cppcut_assert_equal(3U, expected_current_line);
}

/*!\test
 * The iterator defined for unfiltered List::Nav iterates over the currently
 * shown lines in a scrolled list.
 */
void test_const_iterator_steps_through_visible_lines_scrolled_down(void)
{
    List::NavItemNoFilter no_filter(list->get_number_of_items());
    List::Nav nav(3, no_filter);

    /* move some steps down */
    for(int i = 0; i < 4; ++i)
        cut_assert_true(nav.down());

    /* select middle item */
    cut_assert_true(nav.up());

    cppcut_assert_equal(3U, nav.get_cursor());

    unsigned int expected_current_line = 2;

    for(auto it : nav)
    {
        cppcut_assert_equal(expected_current_line, it);
        ++expected_current_line;
    }

    cppcut_assert_equal(5U, expected_current_line);
}

/*!\test
 * The iterator defined for unfiltered List::Nav iterates over the currently
 * shown lines in a list scrolled down all way down
 */
void test_const_iterator_steps_through_visible_lines_at_end_of_list(void)
{
    List::NavItemNoFilter no_filter(list->get_number_of_items());
    List::Nav nav(3, no_filter);

    while(nav.down())
    {
        /* nothing */
    }

    cppcut_assert_equal(list->get_number_of_items() - 1, nav.get_cursor());

    unsigned int expected_current_line = list->get_number_of_items() - 3;

    for(auto it : nav)
    {
        cppcut_assert_equal(expected_current_line, it);
        ++expected_current_line;
    }

    cppcut_assert_equal(list->get_number_of_items(), expected_current_line);
}

/*!\test
 * The iterator defined for unfiltered List::Nav does not get confused if there
 * are fewer visible items than the maximum number of lines on the display.
 */
void test_const_iterator_steps_through_visible_lines_on_big_display(void)
{
    List::NavItemNoFilter no_filter(list->get_number_of_items());
    List::Nav nav(50, no_filter);

    cppcut_assert_operator(list->get_number_of_items(), <=, 50U,
                           cut_message("This test cannot work with so many items. Please fix the test."));

    unsigned int expected_current_line = 0;

    for(auto it : nav)
    {
        cppcut_assert_equal(expected_current_line, it);
        ++expected_current_line;
    }

    cppcut_assert_equal(list->get_number_of_items(), expected_current_line);
}

};


namespace list_navigation_tests_with_unselectable_items
{

void cut_setup(void)
{
    list = new List::RamList();
    cppcut_assert_not_null(list);

    int count = 0;
    unsigned int item_flags;

    for(auto t : list_texts)
    {
        if(count == 0)
            item_flags = NavItemFlags::item_is_on_top;
        else if(count == 2)
            item_flags = 0;
        else if(count == sizeof(list_texts) / sizeof(list_texts[0]) - 2)
            item_flags = NavItemFlags::item_is_at_bottom;

        ++count;

        List::append(list, List::TextItem(t, false, item_flags));
    }
}

void cut_teardown(void)
{
    delete list;
    list = nullptr;
}

/*!\test
 * Navigation should start in third line, with first two lines displayed first.
 */
void test_navigation_init_with_first_lines_unselectable(void)
{
    NavItemFlags flags(*list);
    List::Nav nav(4, flags);

    flags.set_selectable_mask(NavItemFlags::item_is_on_top);

    cppcut_assert_equal(2U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 4>({0, 1, 2, 3}));
}

/*!\test
 * First two lines are unselectable, so we cannot select them.
 */
void test_cannot_select_unselectable_first_lines(void)
{
    NavItemFlags flags(*list);
    List::Nav nav(4, flags);

    flags.set_selectable_mask(NavItemFlags::item_is_on_top);

    cut_assert_false(nav.up());
    cppcut_assert_equal(2U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 4>({0, 1, 2, 3}));

    /* down works as expected, no persistent internal underflows */
    cut_assert_true(nav.down());
    cppcut_assert_equal(3U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 4>({0, 1, 2, 3}));
}

/*!\test
 * First two lines are unselectable, but they must become visible when
 * scrolling up.
 */
void test_scroll_to_unselectable_first_lines(void)
{
    NavItemFlags flags(*list);
    List::Nav nav(4, flags);

    flags.set_selectable_mask(NavItemFlags::item_is_on_top);

    cut_assert_true(nav.down());
    cut_assert_true(nav.down());
    cppcut_assert_equal(4U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 4>({1, 2, 3, 4}));

    cut_assert_true(nav.down());
    cppcut_assert_equal(5U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 4>({2, 3, 4, 5}));

    /* regular case... */
    cut_assert_true(nav.up());
    cut_assert_true(nav.up());
    cppcut_assert_equal(3U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 4>({2, 3, 4, 5}));

    /* ...but now show first two lines because they are not selectable and
     * could not be shown otherwise */
    cut_assert_true(nav.up());
    cppcut_assert_equal(2U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 4>({0, 1, 2, 3}));
}

/*!\test
 * Last line is unselectable, but it must become visible when scrolling down.
 */
void test_scroll_to_unselectable_last_line(void)
{
    NavItemFlags flags(*list);
    List::Nav nav(3, flags);

    flags.set_selectable_mask(NavItemFlags::item_is_at_bottom);
    cppcut_assert_equal(0U, nav.get_cursor());

    const unsigned int N = list->get_number_of_items() - 1;

    for(unsigned int i = 0; i < N - 4; ++i)
        cut_assert_true(nav.down());

    /* last regular case... */
    cut_assert_true(nav.down());
    cppcut_assert_equal(N - 3, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 3>({N - 5, N - 4, N - 3}));

    /* ...but now show last line because it is not selectable and could not be
     * shown otherwise */
    cut_assert_true(nav.down());
    cppcut_assert_equal(N - 2, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 3>({N - 2, N - 1, N - 0}));
}

};

/*!@}*/
