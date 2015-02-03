/*
 * Copyright (C) 2015  T+A elektroakustik GmbH & Co. KG
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

class TextTreeItem: public List::TreeItem, public List::TextItem
{
  private:
    TextTreeItem(const TextTreeItem &);
    TextTreeItem &operator=(const TextTreeItem &);

  public:
    explicit TextTreeItem(TextTreeItem &&) = default;

    explicit TextTreeItem(const char *text, bool text_is_translatable,
                          unsigned int flags):
        Item(flags),
        TreeItem(flags),
        TextItem(text, text_is_translatable, flags)
    {}
};

static List::RamList *list;

void cut_setup(void)
{
    list = new List::RamList();
    cut_assert_not_null(list);
}

void cut_teardown(void)
{
    delete list;
    list = nullptr;
}

/*!\test
 * After initialization, the list shall be empty.
 */
void test_list_is_empty_on_startup(void)
{
    cut_assert_equal_uint(0, list->get_number_of_items());
    cut_assert_true(list->empty());
}

/*!\test
 * Appending a single item to an empty RAM-based list works.
 */
void test_add_single_list_item(void)
{
    unsigned int line =
        List::append(list, TextTreeItem("Test entry", false, 0));

    cut_assert_equal_uint(0, line);
    cut_assert_equal_uint(1, list->get_number_of_items());
    cut_assert_false(list->empty());

    auto item = dynamic_cast<const TextTreeItem *>(list->get_item(line));

    cut_assert_not_null(item);
    cut_assert_equal_string("Test entry", item->get_text());
}

static void append_items_to_list(List::RamList *l, const char *strings[])
{
    unsigned int old_size = l->get_number_of_items();
    unsigned int expected_size = old_size;

    for(const char **s = strings; *s != nullptr; ++s)
    {
        (void)List::append(l, TextTreeItem(*s, false, 0));
        ++expected_size;
    }

    cut_assert_equal_uint(expected_size, l->get_number_of_items());
    cut_assert_false(l->empty());
}

/*!\test
 * Appending a few items to an empty RAM-based list works.
 */
void test_add_multiple_list_items(void)
{
    static const char *strings[] = { "first", "second", "foo", "bar", nullptr };

    append_items_to_list(list, strings);

    for(unsigned int i = 0; i < sizeof(strings) / sizeof(strings[0]) - 1; ++i)
    {
        auto item = dynamic_cast<const TextTreeItem *>(list->get_item(i));

        cut_assert_not_null(item);
        cut_assert_equal_string(strings[i], item->get_text());
    }
}

/*!\test
 * Trying to descend into a non-existent sub-list returns a null pointer.
 */
void test_move_down_hierarchy_without_child_list_returns_null_list(void)
{
    (void)List::append(list, TextTreeItem("Foo", false, 0));
    cut_assert_null(list->down(0));
}

/*!\test
 * Trying to ascend to non-existent parent list returns the current list.
 */
void test_move_up_hierarchy_without_parent_list_returns_self(void)
{
    cut_assert_equal_pointer(list, &list->up());
}

/*!\test
 * It is possible to move to a sub-list referenced by a list item of another
 * list.
 */
void test_up_and_down_one_level_of_hierarchy(void)
{
    static const char *strings[] = { "foo", "bar", nullptr };

    append_items_to_list(list, strings);

    static const char *more_strings_1[] = { "first", "second", "third", nullptr };
    std::shared_ptr<List::RamList> second_list(new List::RamList());
    cppcut_assert_not_null(second_list.get());
    append_items_to_list(second_list.get(), more_strings_1);

    static const char *more_strings_2[] = { "fourth", "fifth", "sixth", nullptr };
    std::shared_ptr<List::RamList> third_list(new List::RamList());
    cppcut_assert_not_null(third_list.get());
    append_items_to_list(third_list.get(), more_strings_2);

    cut_assert_true(list->set_child_list(0, second_list));
    cut_assert_true(list->set_child_list(1, third_list));

    cut_assert_equal_pointer(list, &second_list->up());
    cut_assert_equal_pointer(second_list.get(), list->down(0));
    cut_assert_equal_pointer(list, &list->down(0)->up());
    cut_assert_equal_string("second", dynamic_cast<const TextTreeItem *>(list->down(0)->get_item(1))->get_text());

    cut_assert_equal_pointer(list, &third_list->up());
    cut_assert_equal_pointer(third_list.get(), list->down(1));
    cut_assert_equal_pointer(list, &list->down(1)->up());
    cut_assert_equal_string("sixth", dynamic_cast<const TextTreeItem *>(list->down(1)->get_item(2))->get_text());
}

/*!\test
 * It is possible to move through a generic hierarchy of lists (i.e., a tree of
 * lists).
 */
void test_up_and_down_multiple_levels_of_hierarchy(void)
{
    static const char *levels[] = { "Level 0", "Level 1", "Level 2", "Level 3", nullptr };

    cut_assert_equal_uint(0, List::append(list, TextTreeItem(levels[0], false, 0)));

    size_t idx = 1;

    for(List::RamList *prev = list; levels[idx] != nullptr; ++idx)
    {
        std::shared_ptr<List::ListIface> l(new List::RamList());
        List::RamList *ram_list = static_cast<List::RamList *>(l.get());

        cut_assert_equal_uint(0, List::append(ram_list,
                                              TextTreeItem(levels[idx],
                                                           false, 0)));
        cut_assert_true(prev->set_child_list(0, l));

        prev = ram_list;
    }

    cut_assert_equal_uint(4, idx);

    const List::ListIface *const_prev = list;
    idx = 0;

    for(const List::ListIface *l = list; l != nullptr; l = l->down(0), ++idx)
    {
        cut_assert_equal_pointer(const_prev, &l->up());

        auto item = dynamic_cast<const TextTreeItem *>(l->get_item(0));

        cut_assert_not_null(item);
        cut_assert_equal_string(levels[idx], item->get_text());

        const_prev = l;
    }

    cut_assert_equal_uint(4, idx);
}

/*!\test
 * Clearing a list works.
 */
void test_clear_flat_list(void)
{
    test_add_multiple_list_items();
    cppcut_assert_operator(0U, <, list->get_number_of_items());

    list->clear();
    cppcut_assert_equal(0U, list->get_number_of_items());
    cut_assert_true(list->empty());
}

};

/*!@}*/
