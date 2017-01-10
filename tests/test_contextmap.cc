/*
 * Copyright (C) 2016, 2017  T+A elektroakustik GmbH & Co. KG
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

#include "context_map.hh"
#include "de_tahifi_lists_context.h"

#include "mock_messages.hh"

constexpr const uint32_t List::ContextInfo::HAS_EXTERNAL_META_DATA;
constexpr const uint32_t List::ContextInfo::HAS_PROPER_SEARCH_FORM;
constexpr const uint32_t List::ContextInfo::SEARCH_NOT_POSSIBLE;
constexpr const uint32_t List::ContextInfo::INTERNAL_INVALID;
constexpr const uint32_t List::ContextInfo::INTERNAL_FLAGS_MASK;
constexpr const uint32_t List::ContextInfo::PUBLIC_FLAGS_MASK;
constexpr const List::context_id_t List::ContextMap::INVALID_ID;

/*!
 * \addtogroup context_map_tests Unit tests for list context management
 * \ingroup list
 *
 * List context management unit tests.
 */
/*!@{*/

namespace context_map_tests
{

static MockMessages *mock_messages;

static List::ContextMap *cmap;

void cut_setup()
{
    mock_messages = new MockMessages;
    cppcut_assert_not_null(mock_messages);
    mock_messages->init();
    mock_messages_singleton = mock_messages;

    cmap = new List::ContextMap();
    cut_assert_not_null(cmap);

    cppcut_assert_equal(List::context_id_t(0),
                        cmap->append("first",  "First list context"));
    cppcut_assert_equal(List::context_id_t(1),
                        cmap->append("second", "Second list context",
                                     List::ContextInfo::HAS_EXTERNAL_META_DATA));
}

void cut_teardown()
{
    delete cmap;
    cmap = nullptr;

    mock_messages->check();
    mock_messages_singleton = nullptr;
    delete mock_messages;
    mock_messages = nullptr;
}

template <typename T>
static void access_with_unknown_id_expect_invalid_ctx(const T &unknown_id)
{
    List::ContextMap &map(*cmap);

    cut_assert_false(map.exists(unknown_id));

    List::ContextInfo &info(map[unknown_id]);
    cppcut_assert_equal(0U, info.get_flags());
    cut_assert_false(info.check_flags(UINT32_MAX));
    cut_assert_false(info.is_valid());

    cppcut_assert_equal("#INVALID#", info.string_id_.c_str());
    cppcut_assert_equal("Invalid list context", info.description_.c_str());

    /* changing flags does not work for invalid context */
    info.set_flags(List::ContextInfo::HAS_EXTERNAL_META_DATA);
    cppcut_assert_equal(0U, info.get_flags());
    cut_assert_false(info.is_valid());
}

/*!\test
 * Accessing map with out-of-bounds numeric ID returns a default item.
 */
void test_array_access_with_out_of_bounds_id_yields_default_invalid_context()
{
    access_with_unknown_id_expect_invalid_ctx(List::context_id_t(5));
}

/*!\test
 * Accessing map with unknown string ID returns a default item.
 */
void test_array_access_with_unknown_string_id_yields_default_invalid_context()
{
    access_with_unknown_id_expect_invalid_ctx("does not exist");
}

template <typename T>
static void access_by_ids(const T &id1, const T &id2)
{
    List::ContextMap &map(*cmap);

    cut_assert_true(map.exists(id1));
    cut_assert_true(map.exists(id2));

    const List::ContextInfo &info1(map[id1]);
    cut_assert_true(info1.is_valid());
    cppcut_assert_equal("first", info1.string_id_.c_str());
    cppcut_assert_equal("First list context", info1.description_.c_str());
    cppcut_assert_equal(0U, info1.get_flags());

    const List::ContextInfo &info2(map[id2]);
    cut_assert_true(info2.is_valid());
    cppcut_assert_equal("second", info2.string_id_.c_str());
    cppcut_assert_equal("Second list context", info2.description_.c_str());
    cppcut_assert_equal(List::ContextInfo::HAS_EXTERNAL_META_DATA, info2.get_flags());
}

template <typename T>
static void detailed_access_by_ids(const T &id1, const T &id2)
{
    List::ContextMap &map(*cmap);

    cut_assert_true(map.exists(id1));
    cut_assert_true(map.exists(id2));

    List::context_id_t ctx_id1;
    const List::ContextInfo &info1(map.get_context_info_by_string_id(id1, ctx_id1));
    cut_assert_true(info1.is_valid());
    cppcut_assert_equal("first", info1.string_id_.c_str());
    cppcut_assert_equal("First list context", info1.description_.c_str());
    cppcut_assert_equal(0U, info1.get_flags());
    cppcut_assert_equal(&info1, &(map[ctx_id1]));

    List::context_id_t ctx_id2;
    const List::ContextInfo &info2(map.get_context_info_by_string_id(id2, ctx_id2));
    cut_assert_true(info2.is_valid());
    cppcut_assert_equal("second", info2.string_id_.c_str());
    cppcut_assert_equal("Second list context", info2.description_.c_str());
    cppcut_assert_equal(List::ContextInfo::HAS_EXTERNAL_META_DATA, info2.get_flags());
    cppcut_assert_equal(&info2, &(map[ctx_id2]));

    cppcut_assert_not_equal(ctx_id1, ctx_id2);
}

/*!\test
 * Context information can be retrieved by numeric ID.
 */
void test_access_by_numeric_id()
{
    access_by_ids(List::context_id_t(0),
                  List::context_id_t(1));
}

/*!\test
 * Context information can be retrieved by string ID as C string.
 */
void test_access_by_cstring_id()
{
    access_by_ids(static_cast<const char *>("first"),
                  static_cast<const char *>("second"));
}

/*!\test
 * Context information can be retrieved by string ID as C++ string.
 */
void test_access_by_cppstring_id()
{
    access_by_ids(std::string("first"),
                  std::string("second"));
}

/*!\test
 * Context information can be retrieved by string ID as C string.
 */
void test_detailed_access_by_cstring_id()
{
    detailed_access_by_ids(std::string("first"),
                           std::string("second"));
}

/*!\test
 * Context information can be retrieved by string ID as C++ string.
 */
void test_detailed_access_by_cppstring_id()
{
    detailed_access_by_ids(std::string("first"),
                           std::string("second"));
}

/*!\test
 * Context information flags can be set after construction.
 */
void test_set_context_information_flags()
{
    List::ContextMap &map(*cmap);

    List::ContextInfo &info(map[List::context_id_t(0)]);
    cut_assert_true(info.is_valid());
    cppcut_assert_equal(0U, info.get_flags());

    info.set_flags(List::ContextInfo::HAS_EXTERNAL_META_DATA);
    cppcut_assert_equal(List::ContextInfo::HAS_EXTERNAL_META_DATA, info.get_flags());
}

/*!\test
 * Some context information flags are reserved for internal purposes.
 */
void test_not_all_context_information_flags_can_be_set()
{
    List::ContextMap &map(*cmap);

    List::ContextInfo &info(map[List::context_id_t(0)]);
    cut_assert_true(info.is_valid());
    cppcut_assert_equal(0U, info.get_flags());
    cut_assert_false(info.check_flags(UINT32_MAX));

    info.set_flags(UINT32_MAX);
    cppcut_assert_equal(List::ContextInfo::PUBLIC_FLAGS_MASK, info.get_flags());
    cppcut_assert_not_equal(UINT32_MAX, info.get_flags());
    cppcut_assert_not_equal(0U, info.get_flags());
    cut_assert_true(info.check_flags(UINT32_MAX));
    cut_assert_true(info.check_flags(List::ContextInfo::PUBLIC_FLAGS_MASK));
    cut_assert_false(info.check_flags(List::ContextInfo::INTERNAL_FLAGS_MASK));
}

/*!\test
 * It is not possible to insert string contexts with duplicate string IDs.
 */
void test_context_string_ids_must_be_unique()
{
    mock_messages->expect_msg_error_formatted(0, LOG_CRIT, "BUG: Duplicate context ID \"first\"");
    cppcut_assert_equal(List::ContextMap::INVALID_ID, cmap->append("first", "foo"));
}

/*!\test
 * It is not possible to insert string contexts with empty string IDs.
 */
void test_context_string_ids_must_not_be_empty()
{
    mock_messages->expect_msg_error_formatted(0, LOG_CRIT, "BUG: Invalid context ID \"\"");
    cppcut_assert_equal(List::ContextMap::INVALID_ID, cmap->append("", "foo"));
}

/*!\test
 * It is not possible to insert string contexts with string IDs starting with
 * character '#'.
 */
void test_context_string_ids_must_not_start_with_hash_character()
{
    mock_messages->expect_msg_error_formatted(0, LOG_CRIT, "BUG: Invalid context ID \"#test\"");
    cppcut_assert_equal(List::ContextMap::INVALID_ID, cmap->append("#test", "foo"));
}

/*!\test
 * String contexts may have no description.
 */
void test_context_description_may_be_empty()
{
    cppcut_assert_not_equal(List::ContextMap::INVALID_ID, cmap->append("new", ""));

    List::ContextMap &map(*cmap);

    const List::ContextInfo &info(map["new"]);
    cut_assert_true(info.is_valid());
    cppcut_assert_equal("new", info.string_id_.c_str());
    cut_assert_true(info.description_.empty());
}

/*!\test
 * Due to restrictions imposed by the encoding of context IDs into list IDs,
 * the maximum number of contexts is not very high.
 */
void test_warning_is_emitted_when_adding_too_many_contexts()
{
    /* just to make sure we are operating within a sane range */
    cppcut_assert_operator(20U, >=, DBUS_LISTS_CONTEXT_ID_MAX);

    List::context_id_t expected_id(2);
    char string_id[] = "a";

    do
    {
        List::context_id_t id = cmap->append(string_id, "foo");

        cppcut_assert_equal(expected_id, id);
        ++string_id[0];
        ++expected_id;
    }
    while(expected_id <= DBUS_LISTS_CONTEXT_ID_MAX);

    mock_messages->expect_msg_error(0, LOG_CRIT, "BUG: Too many list contexts (ignored)");

    List::context_id_t id = cmap->append(string_id, "foo");
    cppcut_assert_equal(expected_id, id);
}

/*!\test
 * If a list broker sends 3 contexts, but the second is rejected by us, then
 * IDs 0 and 2 are still valid.
 */
void test_invalid_contexts_do_not_mess_up_numeric_context_ids()
{
    List::ContextMap &map(*cmap);

    cut_assert_true(map.exists(List::context_id_t(0)));
    cut_assert_true(map.exists(List::context_id_t(1)));
    cut_assert_false(map.exists(List::context_id_t(2)));

    mock_messages->expect_msg_error_formatted(0, LOG_CRIT, "BUG: Invalid context ID \"#rejected\"");
    cppcut_assert_equal(List::ContextMap::INVALID_ID, map.append("#rejected", "foo"));
    cut_assert_true(map.exists(List::context_id_t(2)));

    cppcut_assert_equal(List::context_id_t(3),
                        map.append("accepted", "Accepted list context"));

    cut_assert_true(map.exists(List::context_id_t(0)));
    cut_assert_true(map.exists(List::context_id_t(1)));
    cut_assert_true(map.exists(List::context_id_t(2)));
    cut_assert_true(map.exists(List::context_id_t(3)));
    cut_assert_false(map.exists(List::context_id_t(4)));

    cut_assert_true(map[List::context_id_t(0)].is_valid());
    cut_assert_true(map[List::context_id_t(1)].is_valid());
    cut_assert_false(map[List::context_id_t(2)].is_valid());
    cut_assert_true(map[List::context_id_t(3)].is_valid());
}

}

/*!@}*/
