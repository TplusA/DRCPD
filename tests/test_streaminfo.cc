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

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <cppcutter.h>

#include "streaminfo.hh"
#include "mock_messages.hh"

/*!
 * \addtogroup streaminfo_tests Unit tests
 * \ingroup streaminfo
 *
 * Stream information unit tests.
 */
/*!@{*/

namespace streaminfo_tests
{

static MockMessages *mock_messages;

static StreamInfo *sinfo;

void cut_setup()
{
    mock_messages = new MockMessages;
    cppcut_assert_not_null(mock_messages);
    mock_messages->init();
    mock_messages_singleton = mock_messages;

    sinfo = new StreamInfo;
    cut_assert_not_null(sinfo);
}

void cut_teardown()
{
    delete sinfo;
    sinfo = nullptr;

    mock_messages->check();
    mock_messages_singleton = nullptr;
    delete mock_messages;
    mock_messages = nullptr;
}

/*!\test
 * Simple insertion and use of a single fallback title.
 */
void test_insert_lookup_forget_one_title()
{
    static const std::string expected_title = "Testing";
    static constexpr uint16_t expected_id = 1;

    const uint16_t id = sinfo->insert(expected_title.c_str());
    cppcut_assert_equal(expected_id, id);

    const std::string *title = sinfo->lookup_and_activate(expected_id);
    cppcut_assert_not_null(title);
    cppcut_assert_equal(expected_title, *title);

    sinfo->forget();

    cppcut_assert_null(sinfo->lookup_and_activate(expected_id));
}

template <size_t N>
static void insert_titles(const std::array<uint16_t, N> &expected_ids,
                          const std::array<const std::string, N> &expected_titles)
{
    for(size_t i = 0; i < N; ++i)
    {
        const auto id = sinfo->insert(expected_titles[i].c_str());
        cppcut_assert_equal(expected_ids[i], id);
    }
}

/*!\test
 * Simple insertion and use of a multiple fallback titles.
 */
void test_insert_lookup_forget_multiple_titles()
{
    static constexpr std::array<uint16_t, 4> expected_ids = { 1, 2, 3, 4, };
    static const std::array<const std::string, expected_ids.size()> expected_titles =
    {
        "First", "Second", "Third", "Fourth",
    };

    insert_titles(expected_ids, expected_titles);

    for(size_t i = 0; i < expected_ids.size(); ++i)
    {
        auto title = sinfo->lookup_and_activate(expected_ids[i]);
        cppcut_assert_not_null(title);
        cppcut_assert_equal(expected_titles[i], *title);

        sinfo->forget();

        cppcut_assert_null(sinfo->lookup_and_activate(expected_ids[i]));
    }
}

/*!\test
 * API allows simplified use for the common use case.
 */
void test_explicit_forget_is_not_required_if_activating_in_insertion_order()
{
    static constexpr std::array<uint16_t, 4> expected_ids = { 1, 2, 3, 4, };
    static const std::array<const std::string, expected_ids.size()> expected_titles =
    {
        "First", "Second", "Third", "Fourth",
    };

    insert_titles(expected_ids, expected_titles);

    for(size_t i = 0; i < expected_ids.size(); ++i)
    {
        auto title = sinfo->lookup_and_activate(expected_ids[i]);
        cppcut_assert_not_null(title);
        cppcut_assert_equal(expected_titles[i], *title);
    }

    for(size_t i = 0; i < expected_ids.size(); ++i)
        cppcut_assert_null(sinfo->lookup_and_activate(expected_ids[i]));
}

/*!\test
 * More complicated scenarios work as expected.
 */
void test_forget_title_in_middle()
{
    static constexpr std::array<uint16_t, 4> expected_ids = { 1, 2, 3, 4, };
    static const std::array<const std::string, expected_ids.size()> expected_titles =
    {
        "First", "Second", "Third", "Fourth",
    };

    insert_titles(expected_ids, expected_titles);

    sinfo->forget(expected_ids[2]);

    auto title = sinfo->lookup_and_activate(expected_ids[0]);
    cppcut_assert_not_null(title);
    cppcut_assert_equal(expected_titles[0], *title);

    sinfo->forget();

    title = sinfo->lookup_and_activate(expected_ids[1]);
    cppcut_assert_not_null(title);
    cppcut_assert_equal(expected_titles[1], *title);

    sinfo->forget();

    title = sinfo->lookup_and_activate(expected_ids[2]);
    cppcut_assert_null(title);

    title = sinfo->lookup_and_activate(expected_ids[3]);
    cppcut_assert_not_null(title);
    cppcut_assert_equal(expected_titles[3], *title);

    sinfo->forget();
}

/*!\test
 * Clearing the container works as expected.
 */
void test_all_information_are_lost_on_clear()
{
    static constexpr std::array<uint16_t, 2> expected_ids = { 1, 2, };
    static const std::array<const std::string, expected_ids.size()> expected_titles = { "A", "B", };

    insert_titles(expected_ids, expected_titles);

    sinfo->clear();

    cppcut_assert_null(sinfo->lookup_and_activate(expected_ids[0]));
    cppcut_assert_null(sinfo->lookup_and_activate(expected_ids[1]));
}

/*!\test
 * Stream IDs are not reused after clear.
 */
void test_ids_are_not_reused()
{
    static constexpr std::array<uint16_t, 2> expected_ids_first  = { 1, 2, };
    static constexpr std::array<uint16_t, 2> expected_ids_second = { 3, 4, };
    static const std::array<const std::string, expected_ids_first.size()> expected_titles = { "A", "B", };

    insert_titles(expected_ids_first, expected_titles);

    sinfo->clear();

    insert_titles(expected_ids_second, expected_titles);
}

/*!\test
 * The maximum number of stream infos is restricted.
 */
void test_maximum_number_of_entries_is_enforced()
{
    for(size_t i = 0; i < StreamInfo::MAX_ENTRIES; ++i)
        cppcut_assert_not_equal(uint16_t(0), sinfo->insert("Testing"));

    mock_messages->expect_msg_error(0, LOG_CRIT, "BUG: Too many stream IDs");
    cppcut_assert_equal(uint16_t(0), sinfo->insert("Too many"));
}

/*!\test
 * Stream IDs are not reused when overflowing.
 */
void test_ids_are_not_reused_on_overflow()
{
    static constexpr std::array<uint16_t, 10> expected_ids  = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, };
    static const std::array<const std::string, expected_ids.size()> expected_titles =
    {
        "A", "B", "C", "D", "E", "F", "G", "H", "I", "J",
    };

    insert_titles(expected_ids, expected_titles);

    for(size_t i = expected_ids[expected_ids.size() - 1] + 1; i <= StreamInfo::MAX_ID; ++i)
    {
        const auto id = sinfo->insert("Dummy");
        cppcut_assert_equal(uint16_t(i), id);
        sinfo->forget(id);
    }

    const auto id = sinfo->insert("Overflown");
    cppcut_assert_equal(uint16_t(expected_ids[expected_ids.size() - 1] + 1), id);
}

}

/*!@}*/
