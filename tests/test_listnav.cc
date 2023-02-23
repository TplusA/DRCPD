/*
 * Copyright (C) 2015, 2016, 2019, 2020  T+A elektroakustik GmbH & Co. KG
 * Copyright (C) 2022, 2023  T+A elektroakustik GmbH & Co. KG
 *
 * This file is part of DRCPD.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301, USA.
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <cppcutter.h>
#include <algorithm>
#include <climits>
#include <array>

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
    static constexpr unsigned int item_is_at_odd_position = 0x04;
    static constexpr unsigned int item_is_at_position_divisible_by_3 = 0x08;

  private:
    bool are_cached_values_valid_;
    unsigned int cached_first_selectable_item_;
    unsigned int cached_last_selectable_item_;
    unsigned int cached_first_visible_item_;
    unsigned int cached_last_visible_item_;
    unsigned int cached_total_number_of_visible_items_;

    unsigned int visibility_flags_;
    unsigned int selectability_flags_;

  public:
    NavItemFlags(const NavItemFlags &) = delete;
    NavItemFlags &operator=(const NavItemFlags &) = delete;

    explicit NavItemFlags(std::shared_ptr<List::ListViewportBase> vp,
                          const List::ListIface *list):
        NavItemFilterIface(std::move(vp), list),
        are_cached_values_valid_(false),
        cached_first_selectable_item_(0),
        cached_last_selectable_item_(0),
        cached_first_visible_item_(0),
        cached_last_visible_item_(0),
        cached_total_number_of_visible_items_(0),
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

    void list_content_changed() override
    {
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

    unsigned int get_first_selectable_item() const override
    {
        if(!are_cached_values_valid_)
            const_cast<NavItemFlags *>(this)->update_cached_values();

        return cached_first_selectable_item_;
    }

    unsigned int get_last_selectable_item() const override
    {
        if(!are_cached_values_valid_)
            const_cast<NavItemFlags *>(this)->update_cached_values();

        return cached_last_selectable_item_;
    }

    unsigned int get_first_visible_item() const override
    {
        if(!are_cached_values_valid_)
            const_cast<NavItemFlags *>(this)->update_cached_values();

        return cached_first_visible_item_;
    }

    unsigned int get_last_visible_item() const override
    {
        if(!are_cached_values_valid_)
            const_cast<NavItemFlags *>(this)->update_cached_values();

        return cached_last_visible_item_;
    }

    unsigned int get_total_number_of_visible_items() const override
    {
        if(!are_cached_values_valid_)
            const_cast<NavItemFlags *>(this)->update_cached_values();

        return cached_total_number_of_visible_items_;
    }

    unsigned int get_flags_for_item(unsigned int item) const override
    {
        msg_log_assert(list_ != nullptr);
        return const_cast<List::ListIface *>(list_)->get_item(viewport_, item)->get_flags();
    }

    bool map_line_number_to_item(unsigned int line_number,
                                 unsigned int &item) const override
    {
        const unsigned int n = list_->get_number_of_items();

        for(unsigned int i = 0; i < n; ++i)
        {
            const unsigned int flags =
                const_cast<List::ListIface *>(list_)->get_item(viewport_, i)->get_flags();

            if(!is_visible(flags))
                continue;

            if(line_number-- > 0)
                continue;

            item = i;
            return true;
        }

        return false;
    }

    bool map_item_to_line_number(unsigned int item,
                                 unsigned int &line_number) const override
    {
        const unsigned int n = list_->get_number_of_items();
        unsigned int line = 0;

        for(unsigned int i = 0; i < n; ++i)
        {
            const unsigned int flags =
                const_cast<List::ListIface *>(list_)->get_item(viewport_, i)->get_flags();

            if(i == item)
            {
                if(!is_visible(flags))
                    return false;

                line_number = line;
                return true;
            }

            if(is_visible(flags))
                ++line;
        }

        return false;
    }

  private:
    void update_cached_values()
    {
        if(!is_list_nonempty())
        {
            cached_first_selectable_item_ = 0;
            cached_first_visible_item_ = 0;
            cached_last_selectable_item_ = 0;
            cached_last_visible_item_ = 0;
            cached_total_number_of_visible_items_ = 0;
            are_cached_values_valid_ = true;
            return;
        }

        const unsigned int n = list_->get_number_of_items();
        cppcut_assert_operator(0U, <, n);

        cached_first_selectable_item_ = 0;
        cached_first_visible_item_ = 0;
        cached_last_selectable_item_ = n - 1;
        cached_last_visible_item_ = cached_last_selectable_item_;
        cached_total_number_of_visible_items_ = 0;

        bool is_first_selectable_set = false;
        bool is_first_visible_set = false;
        bool is_last_selectable_set = false;
        bool is_last_visible_set = false;

        for(unsigned int i = 0; i < n - i; ++i)
        {
            const unsigned int first_flags =
                const_cast<List::ListIface *>(list_)->get_item(viewport_, i)->get_flags();
            const unsigned int last_flags =
                const_cast<List::ListIface *>(list_)->get_item(viewport_, n - i - 1)->get_flags();

            if(!is_first_selectable_set && is_selectable(first_flags))
            {
                cached_first_selectable_item_ = i;
                is_first_selectable_set = true;
            }

            if(is_visible(first_flags))
            {
                ++cached_total_number_of_visible_items_;

                if(!is_first_visible_set)
                {
                    cached_first_visible_item_ = i;
                    is_first_visible_set = true;
                }
            }

            if(!is_last_selectable_set && is_selectable(last_flags))
            {
                cached_last_selectable_item_ = n - i - 1;
                is_last_selectable_set = true;
            }

            if(is_visible(last_flags))
            {
                if(i < n - i - 1)
                    ++cached_total_number_of_visible_items_;

                if(!is_last_visible_set)
                {
                    cached_last_visible_item_ = n - i - 1;
                    is_last_visible_set = true;
                }
            }
        }

        are_cached_values_valid_ = true;
    }
};

static std::unique_ptr<List::RamList> list;
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

static std::shared_ptr<List::ListViewportBase> viewport;

void cut_setup()
{
    list = std::make_unique<List::RamList>("list_navigation_tests");
    cppcut_assert_not_null(list.get());

    viewport = std::make_shared<List::RamList::Viewport>();
    cppcut_assert_not_null(viewport.get());

    for(auto t : list_texts)
        List::append(*list, List::TextItem(t, false, 0));
}

void cut_teardown()
{
    list = nullptr;
    viewport = nullptr;
}

/*!\test
 * Navigation should start in first line, with first line displayed first.
 */
void test_simple_navigation_init()
{
    List::NavItemNoFilter no_filter(viewport, list.get());
    List::Nav nav(3, List::Nav::WrapMode::NO_WRAP, no_filter);

    cppcut_assert_equal(0U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 3>({0, 1, 2}));
}

/*!\test
 * Navigation in visible lines does not change number of first line.
 */
void test_move_down_and_up_within_displayed_lines()
{
    List::NavItemNoFilter no_filter(viewport, list.get());
    List::Nav nav(3, List::Nav::WrapMode::NO_WRAP, no_filter);

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
 * Attempting to not move the selection up fails.
 */
void test_move_up_by_zero_fails()
{
    List::NavItemNoFilter no_filter(viewport, list.get());
    List::Nav nav(3, List::Nav::WrapMode::NO_WRAP, no_filter);

    cut_assert_true(nav.down());
    cut_assert_false(nav.up(0));
    cut_assert_true(nav.up(1));
}

/*!\test
 * Attempting to not move the selection down fails.
 */
void test_move_down_by_zero_fails()
{
    List::NavItemNoFilter no_filter(viewport, list.get());
    List::Nav nav(3, List::Nav::WrapMode::NO_WRAP, no_filter);

    cut_assert_false(nav.down(0));
    cut_assert_true(nav.down(1));
}

/*!\test
 * Moving beyond displayed lines scrolls the list.
 */
void test_move_down_and_up_with_scrolling()
{
    List::NavItemNoFilter no_filter(viewport, list.get());
    List::Nav nav(2, List::Nav::WrapMode::NO_WRAP, no_filter);

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
 * We cannot select negative lines in non-wrapping lists.
 */
void test_cannot_move_before_first_line()
{
    List::NavItemNoFilter no_filter(viewport, list.get());
    List::Nav nav(2, List::Nav::WrapMode::NO_WRAP, no_filter);

    cut_assert_false(nav.up());
    cppcut_assert_equal(0U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 2>({0, 1}));

    /* down works as expected, no persistent internal underflows */
    cut_assert_true(nav.down());
    cppcut_assert_equal(1U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 2>({0, 1}));
}

/*!\test
 * We cannot select lines beyond the last one in non-wrapping lists.
 */
void test_cannot_move_beyond_last_line()
{
    List::NavItemNoFilter no_filter(viewport, list.get());
    List::Nav nav(3, List::Nav::WrapMode::NO_WRAP, no_filter);

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

static void move_multiple_lines_with_no_attempt_to_cross_boundaries(List::Nav &nav)
{
    const unsigned int N = list->get_number_of_items() - 1;

    cut_assert_true(nav.down(N));
    cppcut_assert_equal(N, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 3>({N - 2, N - 1, N}));

    cut_assert_true(nav.up(N));
    cppcut_assert_equal(0U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 3>({0, 1, 2}));

    cut_assert_true(nav.down(N + 1));
    cppcut_assert_equal(N, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 3>({N - 2, N - 1, N}));

    cut_assert_true(nav.up(N + 1));
    cppcut_assert_equal(0U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 3>({0, 1, 2}));

    cut_assert_true(nav.down(N + 2));
    cppcut_assert_equal(N, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 3>({N - 2, N - 1, N}));

    cut_assert_true(nav.up(N + 2));
    cppcut_assert_equal(0U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 3>({0, 1, 2}));

    cut_assert_true(nav.down(UINT_MAX));
    cppcut_assert_equal(N, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 3>({N - 2, N - 1, N}));

    cut_assert_true(nav.up(UINT_MAX));
    cppcut_assert_equal(0U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 3>({0, 1, 2}));
}

/*!\test
 * Moving by multiple lines in non-wrapping list, cursor never crosses list
 * boundaries.
 */
void test_move_multiple_lines_in_nonwrapping_list()
{
    List::NavItemNoFilter no_filter(viewport, list.get());
    List::Nav nav(3, List::Nav::WrapMode::NO_WRAP, no_filter);

    move_multiple_lines_with_no_attempt_to_cross_boundaries(nav);
}

static void move_multiple_lines_down_with_crossing_boundaries(List::Nav &nav)
{
    /* behavior is the same as for non-wrapping list as long as the boundaries
     * are not knowingly crossed */
    move_multiple_lines_with_no_attempt_to_cross_boundaries(nav);

    nav.set_cursor_by_line_number(0);

    const unsigned int N = list->get_number_of_items() - 1;

    check_display(*list, nav, std::array<unsigned int, 3>({0, 1, 2}));
    cut_assert_true(nav.down(N));
    cppcut_assert_equal(N, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 3>({N - 2, N - 1, N}));

    /* always wraps around exactly one line */
    cut_assert_true(nav.down(2));
    cppcut_assert_equal(0U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 3>({0, 1, 2}));

    /* snaps to bottom */
    cut_assert_true(nav.down(2));
    cppcut_assert_equal(2U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 3>({0, 1, 2}));
    cut_assert_true(nav.down(N));
    cppcut_assert_equal(N, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 3>({N - 2, N - 1, N}));

    /* wraps around by one line also for big numbers */
    cut_assert_true(nav.down(6 * N + 4));
    cppcut_assert_equal(0U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 3>({0, 1, 2}));

    /* and also works for very big numbers */
    cut_assert_true(nav.down(UINT_MAX));
    cppcut_assert_equal(N, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 3>({N - 2, N - 1, N}));
    cut_assert_true(nav.down(UINT_MAX));
    cppcut_assert_equal(0U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 3>({0, 1, 2}));
}

static void move_multiple_lines_up_with_crossing_boundaries(List::Nav &nav)
{
    /* behavior is the same as for non-wrapping list as long as the boundaries
     * are not knowingly crossed */
    move_multiple_lines_with_no_attempt_to_cross_boundaries(nav);

    nav.set_cursor_by_line_number(0);

    const unsigned int N = list->get_number_of_items() - 1;

    check_display(*list, nav, std::array<unsigned int, 3>({0, 1, 2}));

    /* always wraps around exactly one line */
    cut_assert_true(nav.up(2));
    cppcut_assert_equal(N, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 3>({N - 2, N - 1, N}));

    /* snaps to top */
    cut_assert_true(nav.up(2));
    cppcut_assert_equal(N - 2, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 3>({N - 2, N - 1, N}));
    cut_assert_true(nav.up(N));
    cppcut_assert_equal(0U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 3>({0, 1, 2}));

    /* wraps around by one line also for big numbers */
    cut_assert_true(nav.up(6 * N + 4));
    cppcut_assert_equal(N, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 3>({N - 2, N - 1, N}));

    /* and also works for very big numbers */
    cut_assert_true(nav.up(UINT_MAX));
    cppcut_assert_equal(0U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 3>({0, 1, 2}));
    cut_assert_true(nav.up(UINT_MAX));
    cppcut_assert_equal(N, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 3>({N - 2, N - 1, N}));
}

/*!\test
 * Moving by multiple lines down in fully wrapping list, cursor crosses list
 * boundaries in a predictable way.
 */
void test_move_multiple_lines_down_in_fully_wrapping_list()
{
    List::NavItemNoFilter no_filter(viewport, list.get());
    List::Nav nav(3, List::Nav::WrapMode::FULL_WRAP, no_filter);

    move_multiple_lines_down_with_crossing_boundaries(nav);
}

/*!\test
 * Moving by multiple lines in wrap-to-top list, cursor crosses list boundaries
 * in a predictable way.
 */
void test_move_multiple_lines_down_in_wrap_to_top_list()
{
    List::NavItemNoFilter no_filter(viewport, list.get());
    List::Nav nav(3, List::Nav::WrapMode::WRAP_TO_TOP, no_filter);

    move_multiple_lines_down_with_crossing_boundaries(nav);
}

/*!\test
 * Moving by multiple lines up in fully wrapping list, cursor crosses list
 * boundaries in a predictable way.
 */
void test_move_multiple_lines_up_in_fully_wrapping_list()
{
    List::NavItemNoFilter no_filter(viewport, list.get());
    List::Nav nav(3, List::Nav::WrapMode::FULL_WRAP, no_filter);

    move_multiple_lines_up_with_crossing_boundaries(nav);
}

/*!\test
 * Moving by multiple lines in wrap-to-bottom list, cursor crosses list
 * boundaries in a predictable way.
 */
void test_move_multiple_lines_up_in_wrap_to_bottom_list()
{
    List::NavItemNoFilter no_filter(viewport, list.get());
    List::Nav nav(3, List::Nav::WrapMode::WRAP_TO_BOTTOM, no_filter);

    move_multiple_lines_up_with_crossing_boundaries(nav);
}

static void can_wrap_from_top_to_bottom(List::Nav::WrapMode wrap_mode)
{
    List::NavItemNoFilter no_filter(viewport, list.get());
    List::Nav nav(3, wrap_mode, no_filter);

    const unsigned int N = list->get_number_of_items() - 1;

    /* do it twice to catch funny overflows or other mistakes */
    for(int passes = 0; passes < 2; ++passes)
    {
        cut_assert_true(nav.up());
        cppcut_assert_equal(N, nav.get_cursor());
        check_display(*list, nav, std::array<unsigned int, 3>({N - 2, N - 1, N}));

        for(unsigned int i = 0; i < N; ++i)
            cut_assert_true(nav.up());

        cppcut_assert_equal(0U, nav.get_cursor());
        check_display(*list, nav, std::array<unsigned int, 3>({0, 1, 2}));
    }
}

static void can_wrap_from_bottom_to_top(List::Nav::WrapMode wrap_mode)
{
    List::NavItemNoFilter no_filter(viewport, list.get());
    List::Nav nav(3, wrap_mode, no_filter);

    const unsigned int N = list->get_number_of_items() - 1;

    /* do it twice to catch funny overflows or other mistakes */
    for(int passes = 0; passes < 2; ++passes)
    {
        for(unsigned int i = 0; i < N; ++i)
            cut_assert_true(nav.down());

        cppcut_assert_equal(N, nav.get_cursor());
        check_display(*list, nav, std::array<unsigned int, 3>({N - 2, N - 1, N}));

        cut_assert_true(nav.down());
        cppcut_assert_equal(0U, nav.get_cursor());
        check_display(*list, nav, std::array<unsigned int, 3>({0, 1, 2}));
    }
}

/*!\test
 * We can wrap from top to bottom in non-wrapping lists.
 */
void test_move_before_first_line_in_fully_wrapped_list()
{
    can_wrap_from_top_to_bottom(List::Nav::WrapMode::FULL_WRAP);
}

/*!\test
 * We can wrap from bottom to top in non-wrapping lists.
 */
void test_move_beyond_last_line_in_fully_wrapped_list()
{
    can_wrap_from_bottom_to_top(List::Nav::WrapMode::FULL_WRAP);
}

/*!\test
 * We cannot wrap from top to bottom in wrap-to-top lists.
 */
void test_cannot_move_before_first_line_in_wrap_to_top_list()
{
    List::NavItemNoFilter no_filter(viewport, list.get());
    List::Nav nav(5, List::Nav::WrapMode::WRAP_TO_TOP, no_filter);

    cppcut_assert_equal(0U, nav.get_cursor());

    cut_assert_false(nav.up());

    cppcut_assert_equal(0U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 5>({0, 1, 2, 3, 4}));
}

/*!\test
 * We can wrap from bottom to top in wrap-to-top lists.
 */
void test_move_beyond_last_line_in_wrap_to_top_list()
{
    can_wrap_from_bottom_to_top(List::Nav::WrapMode::WRAP_TO_TOP);
}

/*!\test
 * We can wrap from top to bottom in wrap-to-bottom lists.
 */
void test_move_before_first_line_in_wrap_to_bottom_list()
{
    can_wrap_from_top_to_bottom(List::Nav::WrapMode::WRAP_TO_BOTTOM);
}

/*!\test
 * We cannot wrap from bottom to top in wrap-to-bottom lists.
 */
void test_cannot_move_beyond_last_line_in_wrap_to_bottom_list()
{
    List::NavItemNoFilter no_filter(viewport, list.get());
    List::Nav nav(5, List::Nav::WrapMode::WRAP_TO_BOTTOM, no_filter);

    cppcut_assert_equal(0U, nav.get_cursor());

    const unsigned int N = list->get_number_of_items() - 1;

    for(unsigned int i = 0; i < N; ++i)
        cut_assert_true(nav.down());

    cppcut_assert_equal(N, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 5>({N - 4, N - 3, N - 2, N - 1, N}));

    cut_assert_false(nav.down());
    cppcut_assert_equal(N, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 5>({N - 4, N - 3, N - 2, N - 1, N}));
}

/*!\test
 * The iterator defined for unfiltered List::Nav iterates over the currently
 * shown lines, where the currently shown lines here are the first few items
 * stored in the list.
 */
void test_const_iterator_steps_through_visible_lines_from_first()
{
    List::NavItemNoFilter no_filter(viewport, list.get());
    List::Nav nav(3, List::Nav::WrapMode::NO_WRAP, no_filter);

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
void test_const_iterator_steps_through_visible_lines_scrolled_down()
{
    List::NavItemNoFilter no_filter(viewport, list.get());
    List::Nav nav(3, List::Nav::WrapMode::NO_WRAP, no_filter);

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
void test_const_iterator_steps_through_visible_lines_at_end_of_list()
{
    List::NavItemNoFilter no_filter(viewport, list.get());
    List::Nav nav(3, List::Nav::WrapMode::NO_WRAP, no_filter);

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
void test_const_iterator_steps_through_visible_lines_on_big_display()
{
    List::NavItemNoFilter no_filter(viewport, list.get());
    List::Nav nav(50, List::Nav::WrapMode::NO_WRAP, no_filter);

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

void test_const_iterator_on_empty_list()
{
    List::RamList empty_list(__func__);

    cppcut_assert_equal(0U, empty_list.get_number_of_items());

    List::NavItemNoFilter no_filter(viewport, &empty_list);
    List::Nav nav(10, List::Nav::WrapMode::NO_WRAP, no_filter);

    unsigned int count = 0;
    std::for_each(nav.begin(), nav.end(), [&count](unsigned int dummy){ ++count; });

    cppcut_assert_equal(0U, count);
}

/*!\test
 * Tying of list to filter can be done after construction of the filter object.
 */
void test_late_binding_of_navigation_and_filter()
{
    List::NavItemNoFilter no_filter(viewport, nullptr);
    List::Nav nav(4, List::Nav::WrapMode::NO_WRAP, no_filter);

    unsigned int expected_current_line = 0;
    std::for_each(nav.begin(), nav.end(),
                  [&expected_current_line](unsigned int dummy){ ++expected_current_line; });

    /* no list associated with filter, so there is nothing to show */
    cppcut_assert_equal(0U, expected_current_line);

    /* associate list and do it again */
    no_filter.tie(viewport, list.get());
    expected_current_line = 0;

    for(auto it : nav)
    {
        cppcut_assert_equal(expected_current_line, it);
        ++expected_current_line;
    }

    /* first four entries were shown */
    cppcut_assert_equal(4U, expected_current_line);
}

/*!\test
 * Selection of a line by line number, not item identifier.
 */
void test_set_cursor_by_line_number()
{
    List::NavItemNoFilter no_filter(viewport, list.get());
    List::Nav nav(4, List::Nav::WrapMode::NO_WRAP, no_filter);

    cppcut_assert_equal(0U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 4>({0, 1, 2, 3}));

    nav.set_cursor_by_line_number(1);
    cppcut_assert_equal(1U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 4>({0, 1, 2, 3}));

    nav.set_cursor_by_line_number(2);
    cppcut_assert_equal(2U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 4>({1, 2, 3, 4}));

    nav.set_cursor_by_line_number(list->get_number_of_items() - 1);
    cppcut_assert_equal(6U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 4>({3, 4, 5, 6}));

    nav.set_cursor_by_line_number(0);
    cppcut_assert_equal(0U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 4>({0, 1, 2, 3}));
}

/*!\test
 * Selection of a non-existent (out of range) line changes nothing.
 */
void test_set_cursor_by_invalid_line_number()
{
    List::NavItemNoFilter no_filter(viewport, list.get());
    List::Nav nav(4, List::Nav::WrapMode::NO_WRAP, no_filter);

    cppcut_assert_equal(0U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 4>({0, 1, 2, 3}));

    nav.set_cursor_by_line_number(list->get_number_of_items() + 1);
    cppcut_assert_equal(0U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 4>({0, 1, 2, 3}));

    nav.set_cursor_by_line_number(UINT_MAX);
    cppcut_assert_equal(0U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 4>({0, 1, 2, 3}));
}

/*!\test
 * Selection of line in empty list changes nothing.
 */
void test_set_cursor_in_empty_list()
{
    List::RamList empty_list(__func__);
    List::NavItemNoFilter no_filter(viewport, &empty_list);
    List::Nav nav(4, List::Nav::WrapMode::NO_WRAP, no_filter);

    cppcut_assert_equal(0U, nav.get_cursor());

    nav.set_cursor_by_line_number(0);
    cppcut_assert_equal(0U, nav.get_cursor());

    nav.set_cursor_by_line_number(1);
    cppcut_assert_equal(0U, nav.get_cursor());

    nav.set_cursor_by_line_number(UINT_MAX);
    cppcut_assert_equal(0U, nav.get_cursor());
}

/*!\test
 * Selection of line in very short lists does not move the list.
 */
void test_set_cursor_in_half_filled_screen()
{
    List::NavItemNoFilter no_filter(viewport, list.get());
    List::Nav nav(50, List::Nav::WrapMode::NO_WRAP, no_filter);

    cppcut_assert_operator(list->get_number_of_items(), <, 50U,
                           cut_message("This test cannot work with so many items. Please fix the test."));

    cppcut_assert_equal(0U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 7>({0, 1, 2, 3, 4, 5, 6}));

    nav.set_cursor_by_line_number(1);
    cppcut_assert_equal(1U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 7>({0, 1, 2, 3, 4, 5, 6}));

    nav.set_cursor_by_line_number(5);
    cppcut_assert_equal(5U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 7>({0, 1, 2, 3, 4, 5, 6}));

    nav.set_cursor_by_line_number(6);
    cppcut_assert_equal(6U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 7>({0, 1, 2, 3, 4, 5, 6}));
}

/*!\test
 * Selection of line in list with as many items as there are lines on display.
 *
 * This test may catch bugs in some corner cases.
 */
void test_set_cursor_in_exactly_fitting_list()
{
    List::NavItemNoFilter no_filter(viewport, list.get());
    List::Nav nav(7, List::Nav::WrapMode::NO_WRAP, no_filter);

    cppcut_assert_equal(7U, list->get_number_of_items());

    cppcut_assert_equal(0U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 7>({0, 1, 2, 3, 4, 5, 6}));

    nav.set_cursor_by_line_number(1);
    cppcut_assert_equal(1U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 7>({0, 1, 2, 3, 4, 5, 6}));

    nav.set_cursor_by_line_number(5);
    cppcut_assert_equal(5U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 7>({0, 1, 2, 3, 4, 5, 6}));

    nav.set_cursor_by_line_number(6);
    cppcut_assert_equal(6U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 7>({0, 1, 2, 3, 4, 5, 6}));
}

/*!\test
 * Get absolute line number for a list item.
 */
void test_get_line_number_by_item()
{
    List::NavItemNoFilter no_filter(viewport, list.get());
    List::Nav nav(2, List::Nav::WrapMode::NO_WRAP, no_filter);

    cppcut_assert_equal(0, nav.get_line_number_by_item(0));
    cppcut_assert_equal(1, nav.get_line_number_by_item(1));
    cppcut_assert_equal(6, nav.get_line_number_by_item(6));
}

/*!\test
 * Getting the absolute line number for a non-existent list item fails.
 */
void test_get_line_number_by_item_fails_for_invalid_item()
{
    List::NavItemNoFilter no_filter(viewport, list.get());
    List::Nav nav(2, List::Nav::WrapMode::NO_WRAP, no_filter);

    cppcut_assert_equal(-1, nav.get_line_number_by_item(7));
    cppcut_assert_equal(-1, nav.get_line_number_by_item(INT_MAX));
    cppcut_assert_equal(-1, nav.get_line_number_by_item(INT_MAX + 1U));
    cppcut_assert_equal(-1, nav.get_line_number_by_item(UINT_MAX));
}

/*!\test
 * Get absolute line number for the currently selected item.
 */
void test_get_line_number_by_cursor()
{
    List::NavItemNoFilter no_filter(viewport, list.get());
    List::Nav nav(2, List::Nav::WrapMode::NO_WRAP, no_filter);

    cppcut_assert_equal(0, nav.get_line_number_by_cursor());
    cut_assert_true(nav.down());
    cppcut_assert_equal(1, nav.get_line_number_by_cursor());
    cut_assert_true(nav.up());
    cppcut_assert_equal(0, nav.get_line_number_by_cursor());
}

/*!\test
 * Getting absolute line numbers for items in an empty list fails.
 */
void test_get_line_number_in_empty_list()
{
    List::RamList empty_list(__func__);
    List::NavItemNoFilter no_filter(viewport, &empty_list);
    List::Nav nav(2, List::Nav::WrapMode::NO_WRAP, no_filter);

    cppcut_assert_equal(-1, nav.get_line_number_by_item(0));
    cppcut_assert_equal(-1, nav.get_line_number_by_cursor());
}

/*!\test
 * It is possible to query the distance of the selection from the top and
 * bottom of the display.
 *
 * Only the screen size in lines determines the outcome in this case.
 */
void test_distance_from_top_and_bottom_in_filled_screen()
{
    List::NavItemNoFilter no_filter(viewport, list.get());
    List::Nav nav(3, List::Nav::WrapMode::NO_WRAP, no_filter);

    cppcut_assert_operator(list->get_number_of_items(), >=, 3U,
                           cut_message("This test cannot work with so few items. Please fix the test."));

    cppcut_assert_equal(0U, nav.distance_to_top());
    cppcut_assert_equal(2U, nav.distance_to_bottom());
    cut_assert_true(nav.down());

    cppcut_assert_equal(1U, nav.distance_to_top());
    cppcut_assert_equal(1U, nav.distance_to_bottom());
    cut_assert_true(nav.down());

    cppcut_assert_equal(2U, nav.distance_to_top());
    cppcut_assert_equal(0U, nav.distance_to_bottom());
    cut_assert_true(nav.down());
}

/*!\test
 * Querying the distance works with very short lists.
 *
 * It must be possible to determine the total number of visible items to get
 * this case right.
 */
void test_distance_from_top_and_bottom_in_half_filled_screen()
{
    List::NavItemNoFilter no_filter(viewport, list.get());
    List::Nav nav(50, List::Nav::WrapMode::NO_WRAP, no_filter);

    cppcut_assert_operator(list->get_number_of_items(), <, 50U,
                           cut_message("This test cannot work with so many items. Please fix the test."));

    cppcut_assert_equal(0U, nav.distance_to_top());
    cppcut_assert_equal(list->get_number_of_items() - 1, nav.distance_to_bottom());
    cut_assert_true(nav.down());

    cppcut_assert_equal(1U, nav.distance_to_top());
    cppcut_assert_equal(list->get_number_of_items() - 2, nav.distance_to_bottom());
    cut_assert_true(nav.down());

    cppcut_assert_equal(2U, nav.distance_to_top());
    cppcut_assert_equal(list->get_number_of_items() - 3, nav.distance_to_bottom());
    cut_assert_true(nav.down());
}

/*!\test
 * Distance functions return 0 for empty lists.
 */
void test_distance_from_top_and_bottom_in_empty_list()
{
    List::RamList empty_list(__func__);

    cppcut_assert_equal(0U, empty_list.get_number_of_items());

    List::NavItemNoFilter no_filter(viewport, &empty_list);
    List::Nav nav(5, List::Nav::WrapMode::NO_WRAP, no_filter);

    cppcut_assert_equal(0U, nav.distance_to_top());
    cppcut_assert_equal(0U, nav.distance_to_bottom());
}

}


namespace list_navigation_tests_with_unselectable_items
{

static std::shared_ptr<List::ListViewportBase> viewport;

void cut_setup()
{
    list = std::make_unique<List::RamList>("list_navigation_tests_with_unselectable_items");
    cppcut_assert_not_null(list.get());

    viewport = std::make_shared<List::RamList::Viewport>();
    cppcut_assert_not_null(viewport.get());

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

        List::append(*list, List::TextItem(t, false, item_flags));
    }
}

void cut_teardown()
{
    list = nullptr;
    viewport = nullptr;
}

/*!\test
 * Navigation should start in third line, with first two lines displayed first.
 */
void test_navigation_init_with_first_lines_unselectable()
{
    NavItemFlags flags(viewport, list.get());
    List::Nav nav(4, List::Nav::WrapMode::NO_WRAP, flags);

    flags.set_selectable_mask(NavItemFlags::item_is_on_top);

    cppcut_assert_equal(2U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 4>({0, 1, 2, 3}));
}

/*!\test
 * Navigation should start in second line, with first line displayed first.
 */
void test_navigation_init_with_first_lines_unselectable_with_late_list_population()
{
    List::RamList local_list(__func__);
    NavItemFlags flags(viewport, &local_list);
    List::Nav nav(10, List::Nav::WrapMode::NO_WRAP, flags);

    cut_assert_false(flags.is_list_nonempty());

    List::append(local_list, List::TextItem(list_texts[0], false, NavItemFlags::item_is_on_top));
    List::append(local_list, List::TextItem(list_texts[1], false, 0));
    List::append(local_list, List::TextItem(list_texts[2], false, 0));
    List::append(local_list, List::TextItem(list_texts[3], false, 0));

    flags.list_content_changed();
    cut_assert_true(flags.is_list_nonempty());

    flags.set_selectable_mask(NavItemFlags::item_is_on_top);

    cppcut_assert_equal(1U, nav.get_cursor());
    check_display(local_list, nav, std::array<unsigned int, 4>({0, 1, 2, 3}));
}

/*!\test
 * Navigation should start in first (nonexistent) line, with no lines displayed.
 */
void test_navigation_init_with_empty_list()
{
    List::RamList empty_list(__func__);
    NavItemFlags flags(viewport, &empty_list);
    List::Nav nav(5, List::Nav::WrapMode::NO_WRAP, flags);

    cut_assert_false(flags.is_list_nonempty());
    flags.set_selectable_mask(NavItemFlags::item_is_on_top);

    cppcut_assert_equal(0U, nav.get_cursor());
    check_display(empty_list, nav, std::array<unsigned int, 0>());
}

/*!\test
 * First two lines are unselectable, so we cannot select them.
 */
void test_cannot_select_unselectable_first_lines()
{
    NavItemFlags flags(viewport, list.get());
    List::Nav nav(4, List::Nav::WrapMode::NO_WRAP, flags);

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
void test_scroll_to_unselectable_first_lines()
{
    NavItemFlags flags(viewport, list.get());
    List::Nav nav(4, List::Nav::WrapMode::NO_WRAP, flags);

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
void test_scroll_to_unselectable_last_line()
{
    NavItemFlags flags(viewport, list.get());
    List::Nav nav(3, List::Nav::WrapMode::NO_WRAP, flags);

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

/*!\test
 * Tying of list to filter can be done after construction of the filter object.
 */
void test_late_binding_of_navigation_and_filter()
{
    NavItemFlags flags(viewport, nullptr);
    List::Nav nav(4, List::Nav::WrapMode::NO_WRAP, flags);

    unsigned int expected_current_line = 0;
    std::for_each(nav.begin(), nav.end(),
                  [&expected_current_line](unsigned int dummy){ ++expected_current_line; });

    /* no list associated with filter, so there is nothing to show */
    cppcut_assert_equal(0U, expected_current_line);

    /* associate list and do it again */
    flags.tie(viewport, list.get());
    expected_current_line = 0;

    for(auto it : nav)
    {
        cppcut_assert_equal(expected_current_line, it);
        ++expected_current_line;
    }

    /* first four entries were shown */
    cppcut_assert_equal(4U, expected_current_line);
}

}


namespace list_navigation_tests_with_invisible_items
{

static std::shared_ptr<List::ListViewportBase> viewport;

void cut_setup()
{
    list = std::make_unique<List::RamList>("list_navigation_tests_with_invisible_items");
    cppcut_assert_not_null(list.get());

    viewport = std::make_shared<List::RamList::Viewport>();
    cppcut_assert_not_null(viewport.get());

    int count = 0;

    for(auto t : list_texts)
    {
        unsigned int item_flags = 0;

        if((count % 2)!= 0)
            item_flags |= NavItemFlags::item_is_at_odd_position;

        if((count % 3) == 0)
            item_flags |= NavItemFlags::item_is_at_position_divisible_by_3;

        if(count == 0)
            item_flags |= NavItemFlags::item_is_on_top;

        if(count == sizeof(list_texts) / sizeof(list_texts[0]) - 1)
            item_flags |= NavItemFlags::item_is_at_bottom;

        ++count;

        List::append(*list, List::TextItem(t, false, item_flags));
    }
}

void cut_teardown()
{
    list = nullptr;
    viewport = nullptr;
}

/*!\test
 * Navigation should start in first visible line, corresponding to the second
 * item in the list.
 */
void test_navigation_with_first_line_invisible()
{
    NavItemFlags flags(viewport, list.get());
    List::Nav nav(4, List::Nav::WrapMode::NO_WRAP, flags);

    cppcut_assert_equal(0U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 4>({0, 1, 2, 3}));

    flags.set_visible_mask(NavItemFlags::item_is_on_top);

    cppcut_assert_equal(1U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 4>({1, 2, 3, 4}));
}

/*!\test
 * Last list item is invisible and therefore neither be seen nor selected.
 * item in the list.
 */
void test_navigation_with_last_line_invisible()
{
    NavItemFlags flags(viewport, list.get());
    List::Nav nav(4, List::Nav::WrapMode::NO_WRAP, flags);

    flags.set_visible_mask(NavItemFlags::item_is_at_bottom);

    cppcut_assert_equal(0U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 4>({0, 1, 2, 3}));

    cut_assert_true(nav.down());
    cut_assert_true(nav.down());
    cut_assert_true(nav.down());
    cut_assert_true(nav.down());
    cut_assert_true(nav.down());
    cppcut_assert_equal(5U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 4>({2, 3, 4, 5}));

    cut_assert_false(nav.down());
    cppcut_assert_equal(5U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 4>({2, 3, 4, 5}));
}

/*!\test
 * Every other list item is invisible.
 */
void test_navigation_with_odd_lines_invisible()
{
    NavItemFlags flags(viewport, list.get());
    List::Nav nav(4, List::Nav::WrapMode::NO_WRAP, flags);

    flags.set_visible_mask(NavItemFlags::item_is_at_odd_position);

    cppcut_assert_equal(0U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 4>({0, 2, 4, 6}));

    cut_assert_true(nav.down());
    cut_assert_true(nav.down());
    cut_assert_true(nav.down());
    cppcut_assert_equal(6U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 4>({0, 2, 4, 6}));

    cut_assert_false(nav.down());
    cppcut_assert_equal(6U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 4>({0, 2, 4, 6}));
}

/*!\test
 * Every third list item is invisible.
 */
void test_navigation_with_every_third_line_invisible()
{
    NavItemFlags flags(viewport, list.get());
    List::Nav nav(4, List::Nav::WrapMode::NO_WRAP, flags);

    flags.set_visible_mask(NavItemFlags::item_is_at_position_divisible_by_3);

    cppcut_assert_equal(1U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 4>({1, 2, 4, 5}));

    cut_assert_true(nav.down());
    cut_assert_true(nav.down());
    cut_assert_true(nav.down());
    cppcut_assert_equal(5U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 4>({1, 2, 4, 5}));

    cut_assert_false(nav.down());
    cppcut_assert_equal(5U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 4>({1, 2, 4, 5}));
}

/*!\test
 * Union of #test_navigation_with_odd_lines_invisible() and
 * #test_navigation_with_every_third_line_invisible().
 */
void test_navigation_with_odd_and_every_third_line_invisible()
{
    NavItemFlags flags(viewport, list.get());
    List::Nav nav(4, List::Nav::WrapMode::NO_WRAP, flags);

    flags.set_visible_mask(NavItemFlags::item_is_at_odd_position |
                           NavItemFlags::item_is_at_position_divisible_by_3);

    cppcut_assert_equal(2U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 2>({2, 4}));

    cut_assert_true(nav.down());
    cppcut_assert_equal(4U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 2>({2, 4}));

    cut_assert_false(nav.down());
    cppcut_assert_equal(4U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 2>({2, 4}));

    cut_assert_true(nav.up());
    cppcut_assert_equal(2U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 2>({2, 4}));

    cut_assert_false(nav.up());
    cppcut_assert_equal(2U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 2>({2, 4}));
}

/*!\test
 * Total number of visible items changes when applying the filter.
 */
void test_get_number_of_visible_items()
{
    NavItemFlags flags(viewport, list.get());
    List::Nav nav(2, List::Nav::WrapMode::NO_WRAP, flags);

    cppcut_assert_equal(list->get_number_of_items(), nav.get_total_number_of_visible_items());

    flags.set_visible_mask(NavItemFlags::item_is_at_odd_position);
    cppcut_assert_equal((list->get_number_of_items() + 1U) / 2U,
                        nav.get_total_number_of_visible_items());

    flags.set_visible_mask(NavItemFlags::item_is_at_position_divisible_by_3);
    cppcut_assert_equal((list->get_number_of_items() * 2U) / 3U,
                        nav.get_total_number_of_visible_items());

    flags.set_visible_mask(NavItemFlags::item_is_at_odd_position |
                           NavItemFlags::item_is_at_position_divisible_by_3);
    cppcut_assert_equal(2U, nav.get_total_number_of_visible_items());

    flags.set_visible_mask(0);
    cppcut_assert_equal(list->get_number_of_items(), nav.get_total_number_of_visible_items());
}

/*!\test
 * Selection of a line by line number in filtered list, not item identifier.
 */
void test_set_cursor_by_line_number()
{
    NavItemFlags flags(viewport, list.get());
    List::Nav nav(3, List::Nav::WrapMode::NO_WRAP, flags);

    flags.set_visible_mask(NavItemFlags::item_is_at_odd_position);

    cppcut_assert_equal(0U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 3>({0, 2, 4}));

    nav.set_cursor_by_line_number(1);
    cppcut_assert_equal(2U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 3>({0, 2, 4}));

    nav.set_cursor_by_line_number(2);
    cppcut_assert_equal(4U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 3>({2, 4, 6}));

    nav.set_cursor_by_line_number(nav.get_total_number_of_visible_items() - 1);
    cppcut_assert_equal(6U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 3>({2, 4, 6}));

    nav.set_cursor_by_line_number(0);
    cppcut_assert_equal(0U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 3>({0, 2, 4}));
}

/*!\test
 * Selection of a non-existent (out of range) line changes nothing.
 */
void test_set_cursor_by_invalid_line_number()
{
    NavItemFlags flags(viewport, list.get());
    List::Nav nav(4, List::Nav::WrapMode::NO_WRAP, flags);

    flags.set_visible_mask(NavItemFlags::item_is_at_odd_position);

    cppcut_assert_equal(0U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 4>({0, 2, 4, 6}));

    nav.set_cursor_by_line_number(nav.get_total_number_of_visible_items());
    cppcut_assert_equal(0U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 4>({0, 2, 4, 6}));

    nav.set_cursor_by_line_number(list->get_number_of_items() - 1);
    cppcut_assert_equal(0U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 4>({0, 2, 4, 6}));

    nav.set_cursor_by_line_number(UINT_MAX);
    cppcut_assert_equal(0U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 4>({0, 2, 4, 6}));
}

/*!\test
 * Selection of line in list with all items filtered out changes nothing.
 */
void test_set_cursor_in_filtered_list()
{
    List::RamList short_list(__func__);

    List::append(short_list, List::TextItem(list_texts[0], false, NavItemFlags::item_is_on_top));
    List::append(short_list, List::TextItem(list_texts[1], false, NavItemFlags::item_is_on_top));
    List::append(short_list, List::TextItem(list_texts[2], false, NavItemFlags::item_is_on_top));

    NavItemFlags flags(viewport, &short_list);
    List::Nav nav(5, List::Nav::WrapMode::NO_WRAP, flags);

    cut_assert_true(nav.down());
    cppcut_assert_equal(1U, nav.get_cursor());

    flags.set_visible_mask(NavItemFlags::item_is_on_top);

    cppcut_assert_equal(0U, nav.get_cursor());

    nav.set_cursor_by_line_number(0);
    cppcut_assert_equal(0U, nav.get_cursor());

    nav.set_cursor_by_line_number(1);
    cppcut_assert_equal(0U, nav.get_cursor());

    nav.set_cursor_by_line_number(UINT_MAX);
    cppcut_assert_equal(0U, nav.get_cursor());
}

/*!\test
 * Selection of line in heavily filtered lists does not move the list.
 *
 * There are more items in the list than there are lines on the display, but
 * filtering makes the displayed list shorter so that it fits entirely to
 * screen.
 */
void test_set_cursor_in_half_filled_screen()
{
    NavItemFlags flags(viewport, list.get());
    List::Nav nav(6, List::Nav::WrapMode::NO_WRAP, flags);

    cppcut_assert_operator(list->get_number_of_items(), >, 6U,
                           cut_message("This test cannot work with so few items. Please fix the test."));

    flags.set_visible_mask(NavItemFlags::item_is_at_odd_position);
    cppcut_assert_equal(0U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 4>({0, 2, 4, 6}));

    nav.set_cursor_by_line_number(1);
    cppcut_assert_equal(2U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 4>({0, 2, 4, 6}));

    nav.set_cursor_by_line_number(2);
    cppcut_assert_equal(4U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 4>({0, 2, 4, 6}));

    nav.set_cursor_by_line_number(3);
    cppcut_assert_equal(6U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 4>({0, 2, 4, 6}));
}

/*!\test
 * Selection of line in filtered list with as many items as there are lines on
 * display.
 *
 * This test may catch bugs in some corner cases.
 */
void test_set_cursor_in_exactly_fitting_list()
{
    NavItemFlags flags(viewport, list.get());
    List::Nav nav(4, List::Nav::WrapMode::NO_WRAP, flags);

    cppcut_assert_operator(list->get_number_of_items(), >, 4U,
                           cut_message("This test cannot work with so few items. Please fix the test."));

    flags.set_visible_mask(NavItemFlags::item_is_at_odd_position);
    cppcut_assert_equal(4U, nav.get_total_number_of_visible_items());
    cppcut_assert_equal(0U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 4>({0, 2, 4, 6}));

    nav.set_cursor_by_line_number(1);
    cppcut_assert_equal(2U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 4>({0, 2, 4, 6}));

    nav.set_cursor_by_line_number(2);
    cppcut_assert_equal(4U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 4>({0, 2, 4, 6}));

    nav.set_cursor_by_line_number(3);
    cppcut_assert_equal(6U, nav.get_cursor());
    check_display(*list, nav, std::array<unsigned int, 4>({0, 2, 4, 6}));
}

/*!\test
 * Get absolute line number for a list item in filtered list.
 */
void test_get_line_number_by_item()
{
    NavItemFlags flags(viewport, list.get());
    List::Nav nav(10, List::Nav::WrapMode::NO_WRAP, flags);

    flags.set_visible_mask(NavItemFlags::item_is_at_odd_position);
    check_display(*list, nav, std::array<unsigned int, 4>({0, 2, 4, 6}));

    cppcut_assert_equal(0, nav.get_line_number_by_item(0));
    cppcut_assert_equal(1, nav.get_line_number_by_item(2));
    cppcut_assert_equal(2, nav.get_line_number_by_item(4));
    cppcut_assert_equal(3, nav.get_line_number_by_item(6));
}

/*!\test
 * The absolute line number of list items may be different for different
 * filters.
 */
void test_get_line_number_by_item_changes_with_different_filters()
{
    NavItemFlags flags(viewport, list.get());
    List::Nav nav(10, List::Nav::WrapMode::NO_WRAP, flags);

    flags.set_visible_mask(NavItemFlags::item_is_at_odd_position);
    check_display(*list, nav, std::array<unsigned int, 4>({0, 2, 4, 6}));

    cppcut_assert_equal(1, nav.get_line_number_by_item(2));
    cppcut_assert_equal(2, nav.get_line_number_by_item(4));

    flags.set_visible_mask(NavItemFlags::item_is_at_odd_position |
                           NavItemFlags::item_is_at_position_divisible_by_3);
    check_display(*list, nav, std::array<unsigned int, 2>({2, 4}));

    cppcut_assert_equal(0, nav.get_line_number_by_item(2));
    cppcut_assert_equal(1, nav.get_line_number_by_item(4));

    flags.set_visible_mask(0);
    check_display(*list, nav, std::array<unsigned int, 7>({0, 1, 2, 3, 4, 5, 6}));

    cppcut_assert_equal(2, nav.get_line_number_by_item(2));
    cppcut_assert_equal(4, nav.get_line_number_by_item(4));
}

/*!\test
 * Getting the absolute line number of a list item works if the item was not
 * filtered out, and fails if the item was filtered out.
 */
void test_get_line_number_by_item_fails_or_succeeds_for_different_filters()
{
    NavItemFlags flags(viewport, list.get());
    List::Nav nav(10, List::Nav::WrapMode::NO_WRAP, flags);

    flags.set_visible_mask(NavItemFlags::item_is_at_odd_position);
    check_display(*list, nav, std::array<unsigned int, 4>({0, 2, 4, 6}));

    cppcut_assert_equal(0, nav.get_line_number_by_item(0));
    cppcut_assert_equal(3, nav.get_line_number_by_item(6));

    flags.set_visible_mask(NavItemFlags::item_is_at_position_divisible_by_3);
    check_display(*list, nav, std::array<unsigned int, 4>({1, 2, 4, 5}));

    cppcut_assert_equal(-1, nav.get_line_number_by_item(0));
    cppcut_assert_equal(-1, nav.get_line_number_by_item(6));
}

/*!\test
 * Getting the absolute line number for a non-existent list item fails.
 */
void test_get_line_number_by_item_fails_for_invalid_item()
{
    NavItemFlags flags(viewport, list.get());
    List::Nav nav(10, List::Nav::WrapMode::NO_WRAP, flags);

    flags.set_visible_mask(NavItemFlags::item_is_at_odd_position);

    cppcut_assert_equal(-1, nav.get_line_number_by_item(7));
    cppcut_assert_equal(-1, nav.get_line_number_by_item(INT_MAX));
    cppcut_assert_equal(-1, nav.get_line_number_by_item(INT_MAX + 1U));
    cppcut_assert_equal(-1, nav.get_line_number_by_item(UINT_MAX));
}

/*!\test
 * Get absolute line number for the currently selected item.
 */
void test_get_line_number_by_cursor()
{
    NavItemFlags flags(viewport, list.get());
    List::Nav nav(2, List::Nav::WrapMode::NO_WRAP, flags);

    flags.set_visible_mask(NavItemFlags::item_is_at_odd_position);

    cppcut_assert_equal(0, nav.get_line_number_by_cursor());

    cut_assert_true(nav.down());
    cppcut_assert_equal(2U, nav.get_cursor());
    cppcut_assert_equal(1, nav.get_line_number_by_cursor());

    cut_assert_true(nav.up());
    cppcut_assert_equal(0U, nav.get_cursor());
    cppcut_assert_equal(0, nav.get_line_number_by_cursor());
}

/*!\test
 * Getting absolute line numbers for items in an list with completely filtered
 * content fails.
 */
void test_get_line_number_in_filtered_list()
{
    List::RamList short_list(__func__);

    List::append(short_list, List::TextItem(list_texts[0], false, NavItemFlags::item_is_on_top));
    List::append(short_list, List::TextItem(list_texts[1], false, NavItemFlags::item_is_on_top));
    List::append(short_list, List::TextItem(list_texts[2], false, NavItemFlags::item_is_on_top));

    NavItemFlags flags(viewport, &short_list);
    List::Nav nav(2, List::Nav::WrapMode::NO_WRAP, flags);

    flags.set_visible_mask(NavItemFlags::item_is_on_top);

    cppcut_assert_equal(-1, nav.get_line_number_by_cursor());
}

/*!\test
 * It is possible to query the distance of the selection from the top and
 * bottom of the display.
 *
 * Only the screen size in lines determines the outcome in this case.
 */
void test_distance_from_top_and_bottom_in_filled_screen()
{
    NavItemFlags flags(viewport, list.get());
    List::Nav nav(3, List::Nav::WrapMode::NO_WRAP, flags);

    flags.set_visible_mask(NavItemFlags::item_is_at_odd_position);

    cppcut_assert_operator(nav.get_total_number_of_visible_items(), >=, 3U,
                           cut_message("This test cannot work with so few items. Please fix the test."));

    cppcut_assert_equal(0U, nav.distance_to_top());
    cppcut_assert_equal(2U, nav.distance_to_bottom());
    cut_assert_true(nav.down());

    cppcut_assert_equal(1U, nav.distance_to_top());
    cppcut_assert_equal(1U, nav.distance_to_bottom());
    cut_assert_true(nav.down());

    cppcut_assert_equal(2U, nav.distance_to_top());
    cppcut_assert_equal(0U, nav.distance_to_bottom());
    cut_assert_true(nav.down());
}

/*!\test
 * Querying the distance works with very short lists.
 *
 * It must be possible to determine the total number of visible items to get
 * this case right.
 */
void test_distance_from_top_and_bottom_in_half_filled_screen()
{
    NavItemFlags flags(viewport, list.get());
    List::Nav nav(10, List::Nav::WrapMode::NO_WRAP, flags);

    flags.set_visible_mask(NavItemFlags::item_is_at_odd_position);

    cppcut_assert_equal(4U, nav.get_total_number_of_visible_items());

    cppcut_assert_equal(0U, nav.distance_to_top());
    cppcut_assert_equal(3U, nav.distance_to_bottom());
    cut_assert_true(nav.down());

    cppcut_assert_equal(1U, nav.distance_to_top());
    cppcut_assert_equal(2U, nav.distance_to_bottom());
    cut_assert_true(nav.down());

    cppcut_assert_equal(2U, nav.distance_to_top());
    cppcut_assert_equal(1U, nav.distance_to_bottom());
    cut_assert_true(nav.down());
}

/*!\test
 * Distance functions return 0 for lists with all items filtered out.
 */
void test_distance_from_top_and_bottom_in_filtered_list()
{
    List::RamList short_list(__func__);

    List::append(short_list, List::TextItem(list_texts[0], false, NavItemFlags::item_is_on_top));
    List::append(short_list, List::TextItem(list_texts[1], false, NavItemFlags::item_is_on_top));
    List::append(short_list, List::TextItem(list_texts[2], false, NavItemFlags::item_is_on_top));

    NavItemFlags flags(viewport, &short_list);
    List::Nav nav(5, List::Nav::WrapMode::NO_WRAP, flags);

    cut_assert_true(nav.down());

    cppcut_assert_equal(3U, short_list.get_number_of_items());
    cppcut_assert_equal(3U, nav.get_total_number_of_visible_items());
    cppcut_assert_equal(1U, nav.distance_to_top());
    cppcut_assert_equal(1U, nav.distance_to_bottom());

    flags.set_visible_mask(NavItemFlags::item_is_on_top);

    cppcut_assert_equal(3U, short_list.get_number_of_items());
    cppcut_assert_equal(0U, nav.get_total_number_of_visible_items());
    cppcut_assert_equal(0U, nav.distance_to_top());
    cppcut_assert_equal(0U, nav.distance_to_bottom());
}

}

/*!@}*/
