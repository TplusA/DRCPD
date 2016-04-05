/*
 * Copyright (C) 2015, 2016  T+A elektroakustik GmbH & Co. KG
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
#include <algorithm>

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

static PreloadedMetaData *empty_preloaded;
static StreamInfo *sinfo;
static std::array<ID::List, StreamInfo::MAX_ENTRIES> referenced_lists;

void cut_setup()
{
    mock_messages = new MockMessages;
    cppcut_assert_not_null(mock_messages);
    mock_messages->init();
    mock_messages_singleton = mock_messages;

    empty_preloaded = new PreloadedMetaData();
    cut_assert_not_null(empty_preloaded);

    sinfo = new StreamInfo;
    cut_assert_not_null(sinfo);

    cppcut_assert_equal(size_t(0), sinfo->get_referenced_lists(referenced_lists));
    referenced_lists.fill(ID::List());
}

void cut_teardown()
{
    delete sinfo;
    sinfo = nullptr;

    delete empty_preloaded;
    empty_preloaded = nullptr;

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
    static constexpr const auto expected_id(ID::OurStream::make());
    ID::List expected_list(5);

    const ID::OurStream id = sinfo->insert(*empty_preloaded, expected_title.c_str(), expected_list, 10);
    cppcut_assert_equal(expected_id.get(), id.get());
    cppcut_assert_equal(size_t(1), sinfo->get_referenced_lists(referenced_lists));

    cppcut_assert_equal(expected_list.get_raw_id(), referenced_lists[0].get_raw_id());

    const StreamInfoItem *info = sinfo->lookup(expected_id);
    cppcut_assert_not_null(info);
    cppcut_assert_equal(expected_title, info->alt_name_);
    cppcut_assert_equal(expected_list.get_raw_id(), info->list_id_.get_raw_id());
    cppcut_assert_equal(10U, info->line_);

    sinfo->forget(expected_id);

    cppcut_assert_null(sinfo->lookup(expected_id));
    cppcut_assert_equal(size_t(0), sinfo->get_referenced_lists(referenced_lists));
}

template <size_t N>
static void insert_titles(const std::array<ID::OurStream, N> &expected_ids,
                          const std::array<const std::string, N> &expected_titles)
{
    for(size_t i = 0; i < N; ++i)
    {
        const auto id = sinfo->insert(*empty_preloaded, expected_titles[i].c_str(), ID::List(8), i);
        cppcut_assert_equal(expected_ids[i].get(), id.get());
    }
}

/*!\test
 * Simple insertion and use of a multiple fallback titles.
 */
void test_insert_lookup_forget_multiple_titles()
{
    static constexpr std::array<ID::OurStream, 4> expected_ids =
    {
        ID::OurStream::make(1), ID::OurStream::make(2),
        ID::OurStream::make(3), ID::OurStream::make(4),
    };
    static const std::array<const std::string, expected_ids.size()> expected_titles =
    {
        "First", "Second", "Third", "Fourth",
    };

    insert_titles(expected_ids, expected_titles);

    cppcut_assert_equal(size_t(1), sinfo->get_referenced_lists(referenced_lists));
    cppcut_assert_equal(8U, referenced_lists[0].get_raw_id());

    for(size_t i = 0; i < expected_ids.size(); ++i)
    {
        const auto *info = sinfo->lookup(expected_ids[i]);
        cppcut_assert_not_null(info);
        cppcut_assert_equal(expected_titles[i], info->alt_name_);

        sinfo->forget(expected_ids[i]);

        cppcut_assert_null(sinfo->lookup(expected_ids[i]));

        if(i < expected_ids.size() - 1)
        {
            cppcut_assert_equal(size_t(1), sinfo->get_referenced_lists(referenced_lists));
            cppcut_assert_equal(8U, referenced_lists[0].get_raw_id());
        }
        else
            cppcut_assert_equal(size_t(0), sinfo->get_referenced_lists(referenced_lists));
    }

    cppcut_assert_equal(size_t(0), sinfo->get_referenced_lists(referenced_lists));
}

/*!\test
 * More complicated scenarios work as expected.
 */
void test_forget_title_in_middle()
{
    static constexpr std::array<ID::OurStream, 4> expected_ids =
    {
        ID::OurStream::make(1), ID::OurStream::make(2),
        ID::OurStream::make(3), ID::OurStream::make(4),
    };
    static const std::array<const std::string, expected_ids.size()> expected_titles =
    {
        "First", "Second", "Third", "Fourth",
    };

    insert_titles(expected_ids, expected_titles);

    cppcut_assert_equal(size_t(1), sinfo->get_referenced_lists(referenced_lists));
    cppcut_assert_equal(8U, referenced_lists[0].get_raw_id());

    sinfo->forget(expected_ids[2]);

    const auto *info = sinfo->lookup(expected_ids[0]);
    cppcut_assert_not_null(info);
    cppcut_assert_equal(expected_titles[0], info->alt_name_);
    cppcut_assert_equal(size_t(1), sinfo->get_referenced_lists(referenced_lists));
    cppcut_assert_equal(8U, referenced_lists[0].get_raw_id());

    sinfo->forget(expected_ids[0]);

    info = sinfo->lookup(expected_ids[1]);
    cppcut_assert_not_null(info);
    cppcut_assert_equal(expected_titles[1], info->alt_name_);
    cppcut_assert_equal(size_t(1), sinfo->get_referenced_lists(referenced_lists));
    cppcut_assert_equal(8U, referenced_lists[0].get_raw_id());

    sinfo->forget(expected_ids[1]);

    info = sinfo->lookup(expected_ids[2]);
    cppcut_assert_null(info);

    info = sinfo->lookup(expected_ids[3]);
    cppcut_assert_not_null(info);
    cppcut_assert_equal(expected_titles[3], info->alt_name_);
    cppcut_assert_equal(size_t(1), sinfo->get_referenced_lists(referenced_lists));
    cppcut_assert_equal(8U, referenced_lists[0].get_raw_id());

    sinfo->forget(expected_ids[3]);

    cppcut_assert_equal(size_t(0), sinfo->get_referenced_lists(referenced_lists));
}

/*!\test
 * Clearing the container works as expected.
 */
void test_all_information_are_lost_on_clear()
{
    static constexpr std::array<ID::OurStream, 2> expected_ids =
    {
        ID::OurStream::make(1), ID::OurStream::make(2),
    };
    static const std::array<const std::string, expected_ids.size()> expected_titles = { "A", "B", };

    insert_titles(expected_ids, expected_titles);

    cppcut_assert_equal(size_t(1), sinfo->get_referenced_lists(referenced_lists));
    cppcut_assert_equal(8U, referenced_lists[0].get_raw_id());

    sinfo->clear();

    cppcut_assert_null(sinfo->lookup(expected_ids[0]));
    cppcut_assert_null(sinfo->lookup(expected_ids[1]));
    cppcut_assert_equal(size_t(0), sinfo->get_referenced_lists(referenced_lists));
}

/*!\test
 * Stream IDs are not reused after clear.
 */
void test_ids_are_not_reused()
{
    static constexpr std::array<ID::OurStream, 2> expected_ids_first =
    {
        ID::OurStream::make(1), ID::OurStream::make(2),
    };
    static constexpr std::array<ID::OurStream, 2> expected_ids_second =
    {
        ID::OurStream::make(3), ID::OurStream::make(4),
    };
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
        cut_assert_true(sinfo->insert(*empty_preloaded, "Testing", ID::List(23), 42).get().is_valid());

    mock_messages->expect_msg_error(0, LOG_CRIT, "BUG: Too many stream IDs");
    cut_assert_false(sinfo->insert(*empty_preloaded, "Too many", ID::List(23), 43).get().is_valid());
}

/*!\test
 * Stream IDs are not reused when overflowing.
 */
void test_ids_are_not_reused_on_overflow()
{
    static constexpr std::array<ID::OurStream, 10> expected_ids  =
    {
        ID::OurStream::make(1), ID::OurStream::make(2),
        ID::OurStream::make(3), ID::OurStream::make(4),
        ID::OurStream::make(5), ID::OurStream::make(6),
        ID::OurStream::make(7), ID::OurStream::make(8),
        ID::OurStream::make(9), ID::OurStream::make(10),
    };
    static const std::array<const std::string, expected_ids.size()> expected_titles =
    {
        "A", "B", "C", "D", "E", "F", "G", "H", "I", "J",
    };

    insert_titles(expected_ids, expected_titles);

    for(size_t i = expected_ids[expected_ids.size() - 1].get().get_cookie() + 1;
        i <= STREAM_ID_COOKIE_MAX;
        ++i)
    {
        const auto id = sinfo->insert(*empty_preloaded, "Dummy", ID::List(23), 42);
        cppcut_assert_equal(stream_id_t(i), id.get().get_cookie());
        sinfo->forget(id);
    }

    const auto id = sinfo->insert(*empty_preloaded, "Overflown", ID::List(23), 43);
    cppcut_assert_equal(stream_id_t(expected_ids[expected_ids.size() - 1].get().get_cookie() + 1),
                        id.get().get_cookie());
}

template <size_t N>
static void expect_referenced_lists(const std::array<ID::List, N> &expected_list_ids)
{
    referenced_lists.fill(ID::List());
    cppcut_assert_equal(N, sinfo->get_referenced_lists(referenced_lists));
    std::sort(referenced_lists.begin(), referenced_lists.begin() + N);
    cut_assert_equal_memory(expected_list_ids.data(), N,
                            referenced_lists.data(), N);
}

/*!\test
 * List IDs
 */
void test_referenced_list_ids_are_returned_uniquely()
{
    std::vector<ID::OurStream> stream_ids;

    stream_ids.push_back(sinfo->insert(*empty_preloaded, "Item 5 in list 5",  ID::List(5),  5));
    expect_referenced_lists(std::array<ID::List, 1>{ ID::List(5), });

    stream_ids.push_back(sinfo->insert(*empty_preloaded, "Item 1 in list 3",  ID::List(3),  1));
    expect_referenced_lists(std::array<ID::List, 2>{ ID::List(3), ID::List(5), });

    stream_ids.push_back(sinfo->insert(*empty_preloaded, "Item 2 in list 10", ID::List(10), 2));
    expect_referenced_lists(std::array<ID::List, 3>{ ID::List(3), ID::List(5), ID::List(10), });

    stream_ids.push_back(sinfo->insert(*empty_preloaded, "Item 7 in list 10", ID::List(10), 7));
    expect_referenced_lists(std::array<ID::List, 3>{ ID::List(3), ID::List(5), ID::List(10), });

    stream_ids.push_back(sinfo->insert(*empty_preloaded, "Item 6 in list 3",  ID::List(3),  6));
    expect_referenced_lists(std::array<ID::List, 3>{ ID::List(3), ID::List(5), ID::List(10), });

    stream_ids.push_back(sinfo->insert(*empty_preloaded, "Item 3 in list 5",  ID::List(5),  3));
    expect_referenced_lists(std::array<ID::List, 3>{ ID::List(3), ID::List(5), ID::List(10), });

    stream_ids.push_back(sinfo->insert(*empty_preloaded, "Item 4 in list 1",  ID::List(1),  4));
    expect_referenced_lists(std::array<ID::List, 4>{ ID::List(1), ID::List(3), ID::List(5), ID::List(10), });

    cppcut_assert_equal(size_t(7), stream_ids.size());

    /*
     * Now forget the streams again.
     *
     * \note
     *     The pattern of the sequence below does not match any real usecase.
     *     In case this test breaks because the implementation starts relying
     *     on usage patterns, then this test should be adapted as well to
     *     simulate the expected usage.
     */

    /* list 5 */
    sinfo->forget(stream_ids[0]);
    expect_referenced_lists(std::array<ID::List, 4>{ ID::List(1), ID::List(3), ID::List(5), ID::List(10), });

    /* list 5 */
    sinfo->forget(stream_ids[5]);
    expect_referenced_lists(std::array<ID::List, 3>{ ID::List(1), ID::List(3), ID::List(10), });

    /* list 10 */
    sinfo->forget(stream_ids[3]);
    expect_referenced_lists(std::array<ID::List, 3>{ ID::List(1), ID::List(3), ID::List(10), });

    /* list 3 */
    sinfo->forget(stream_ids[1]);
    expect_referenced_lists(std::array<ID::List, 3>{ ID::List(1), ID::List(3), ID::List(10), });

    /* list 3 */
    sinfo->forget(stream_ids[4]);
    expect_referenced_lists(std::array<ID::List, 2>{ ID::List(1), ID::List(10), });

    /* list 1 */
    sinfo->forget(stream_ids[6]);
    expect_referenced_lists(std::array<ID::List, 1>{ ID::List(10), });

    /* list 10 */
    sinfo->forget(stream_ids[2]);
    cppcut_assert_equal(size_t(0), sinfo->get_referenced_lists(referenced_lists));
}

}

/*!@}*/
