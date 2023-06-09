/*
 * Copyright (C) 2015, 2016, 2017, 2019, 2020  T+A elektroakustik GmbH & Co. KG
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

static std::unique_ptr<List::RamList> list;

void cut_setup(void)
{
    list = std::make_unique<List::RamList>("ram_list_tests");
    cut_assert_not_null(list.get());
}

void cut_teardown(void)
{
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
        List::append(*list, TextTreeItem("Test entry", false, 0));

    cut_assert_equal_uint(0, line);
    cut_assert_equal_uint(1, list->get_number_of_items());
    cut_assert_false(list->empty());

    auto item = dynamic_cast<const TextTreeItem *>(list->get_item(line));

    cut_assert_not_null(item);
    cut_assert_equal_string("Test entry", item->get_text());
}

static void append_items_to_list(List::RamList &l, const char *strings[])
{
    unsigned int old_size = l.get_number_of_items();
    unsigned int expected_size = old_size;

    for(const char **s = strings; *s != nullptr; ++s)
    {
        (void)List::append(l, TextTreeItem(*s, false, 0));
        ++expected_size;
    }

    cut_assert_equal_uint(expected_size, l.get_number_of_items());
    cut_assert_false(l.empty());
}

/*!\test
 * Appending a few items to an empty RAM-based list works.
 */
void test_add_multiple_list_items(void)
{
    static const char *strings[] = { "first", "second", "foo", "bar", nullptr };

    append_items_to_list(*list, strings);

    for(unsigned int i = 0; i < sizeof(strings) / sizeof(strings[0]) - 1; ++i)
    {
        auto item = dynamic_cast<const TextTreeItem *>(list->get_item(i));

        cut_assert_not_null(item);
        cut_assert_equal_string(strings[i], item->get_text());
    }
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

}

/*!@}*/
