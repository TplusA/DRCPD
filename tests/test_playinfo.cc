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

#include "playinfo.hh"
#include "view_play.hh"
#include "mock_messages.hh"

/*!
 * \addtogroup playinfo_tests Unit tests
 * \ingroup view_play_playinfo
 *
 * Played stream information unit tests.
 */
/*!@{*/

namespace playinfo_tests
{

static MockMessages *mock_messages;

static PlayInfo::Data *data;
static PlayInfo::Reformatters no_reformat;

void cut_setup()
{
    mock_messages = new MockMessages;
    cppcut_assert_not_null(mock_messages);
    mock_messages->init();
    mock_messages_singleton = mock_messages;

    data = new PlayInfo::Data;
    cut_assert_not_null(data);
}

void cut_teardown()
{
    mock_messages->check();
    mock_messages_singleton = nullptr;
    delete mock_messages;
    mock_messages = nullptr;

    delete data;
    data = nullptr;
}

/*!\test
 * Meta data is empty after allocation.
 */
void test_allocated_playinfo_data_is_all_empty()
{
    cppcut_assert_equal(PlayInfo::Data::STREAM_STOPPED, data->assumed_stream_state_);

    for(auto s : data->meta_data_.values_)
        cut_assert_true(s.empty());
}

/*!\test
 * Check if exactly one piece of meta data is set.
 *
 * All strings except the one with the given ID must be empty. The non-empty
 * one must contain the expected string.
 */
static void check_single_meta_data(const std::string &expected, PlayInfo::MetaData::ID id)
{
    cppcut_assert_equal(expected, data->meta_data_.values_[id]);

    for(size_t i = 0; i < data->meta_data_.values_.size(); ++i)
    {
        if(i != id)
            cut_assert_true(data->meta_data_.values_[i].empty());
        else
            cppcut_assert_equal(expected.empty(),
                                data->meta_data_.values_[i].empty());
    }
}

/*!\test
 * Set title information.
 */
void test_set_title()
{
    static const std::string expected = "Ich will brennen";

    data->meta_data_.add("title", expected.c_str(), no_reformat);
    check_single_meta_data(expected, PlayInfo::MetaData::TITLE);
}

/*!\test
 * Set artist information.
 */
void test_set_artist()
{
    static const std::string expected = "Deine Lakaien";

    data->meta_data_.add("artist", expected.c_str(), no_reformat);
    check_single_meta_data(expected, PlayInfo::MetaData::ARTIST);
}

/*!\test
 * Set album information.
 */
void test_set_album()
{
    static const std::string expected = "Zombieland";

    data->meta_data_.add("album", expected.c_str(), no_reformat);
    check_single_meta_data(expected, PlayInfo::MetaData::ALBUM);
}

/*!\test
 * Set audio codec information.
 */
void test_set_audio_codec()
{
    static const std::string expected = "MPEG 1 Audio, Layer 3 (MP3)";

    data->meta_data_.add("audio-codec", expected.c_str(), no_reformat);
    check_single_meta_data(expected, PlayInfo::MetaData::CODEC);
}

/*!\test
 * Set minimum bitrate information.
 */
void test_set_minimum_bitrate()
{
    static const std::string expected = "158315";

    data->meta_data_.add("minimum-bitrate", expected.c_str(), no_reformat);
    check_single_meta_data(expected, PlayInfo::MetaData::BITRATE_MIN);
}

/*!\test
 * Set maximum bitrate information.
 */
void test_set_maximum_bitrate()
{
    static const std::string expected = "159862";

    data->meta_data_.add("maximum-bitrate", expected.c_str(), no_reformat);
    check_single_meta_data(expected, PlayInfo::MetaData::BITRATE_MAX);
}

/*!\test
 * Set nominal bitrate information.
 */
void test_set_nominal_bitrate()
{
    static const std::string expected = "160000";

    data->meta_data_.add("nominal-bitrate", expected.c_str(), no_reformat);
    check_single_meta_data(expected, PlayInfo::MetaData::BITRATE_NOM);
}

/*!\test
 * Bitrate information should be rounded to kb/s.
 */
void test_set_nominal_bitrate_rounded_to_kbit_per_sec()
{
    using string_pair_t = std::pair<const char *, const char *>;

    static const std::array<string_pair_t, 12> test_data =
    {
        string_pair_t("160000", "160"),
        string_pair_t("159999", "160"),
        string_pair_t("159500", "160"),
        string_pair_t("159499", "159"),
        string_pair_t("128000", "128"),
        string_pair_t("128001", "128"),
        string_pair_t("128499", "128"),
        string_pair_t("128500", "129"),
        string_pair_t("500", "1"),
        string_pair_t("499", "0"),
        string_pair_t("0", "0"),
        string_pair_t("4294967295", "4294967"),
    };

    for(auto const &pair : test_data)
    {
        data->meta_data_.add("nominal-bitrate", pair.first,
                             ViewPlay::meta_data_reformatters);
        check_single_meta_data(pair.second, PlayInfo::MetaData::BITRATE_NOM);
    }
}

/*!\test
 * Invalid bitrate strings are left unchanged by reformatter.
 */
void test_set_maximum_bitrate_attempt_rounding_funny_values()
{
    static const std::array<const char *, 14> invalid_strings =
    {
        "a160000",
        "160000a",
        "a160000a",
        " 160000",
        "160000 ",
        " 160000 ",
        "160k",
        "abc",
        "-1",
        "-192000",
        "0-1",
        "0-192000",
        "",
        "4294967296",
    };

    for(auto const &s : invalid_strings)
    {
        mock_messages->expect_msg_error(EINVAL, LOG_NOTICE,
                                        "Invalid bitrate string: \"%s\", leaving as is");
        data->meta_data_.add("maximum-bitrate", s,
                             ViewPlay::meta_data_reformatters);
        check_single_meta_data(s, PlayInfo::MetaData::BITRATE_MAX);
    }
}

/*!\test
 * Clear meta data works as expected.
 */
void test_clear_meta_data()
{
    data->meta_data_.add("title",           "a", no_reformat);
    data->meta_data_.add("artist",          "b", no_reformat);
    data->meta_data_.add("album",           "c", no_reformat);
    data->meta_data_.add("audio-codec",     "d", no_reformat);
    data->meta_data_.add("minimum-bitrate", "e", no_reformat);
    data->meta_data_.add("maximum-bitrate", "f", no_reformat);
    data->meta_data_.add("nominal-bitrate", "g", no_reformat);

    /* all set */
    for(auto s : data->meta_data_.values_)
        cut_assert_false(s.empty());

    data->meta_data_.clear();

    /* none set */
    for(auto s : data->meta_data_.values_)
        cut_assert_true(s.empty());
}

};

/*!@}*/