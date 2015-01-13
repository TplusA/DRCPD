#include <cppcutter.h>

#include "playinfo.hh"

/*!
 * \addtogroup playinfo_tests Unit tests
 * \ingroup view_play_playinfo
 *
 * Played stream information unit tests.
 */
/*!@{*/

namespace playinfo_tests
{

static PlayInfo::Data *data;

void cut_setup()
{
    data = new PlayInfo::Data;
    cut_assert_not_null(data);
}

void cut_teardown()
{
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
            cut_assert_false(data->meta_data_.values_[i].empty());
    }
}

/*!\test
 * Set title information.
 */
void test_set_title()
{
    static const std::string expected = "Ich will brennen";

    data->meta_data_.add("title", expected.c_str());
    check_single_meta_data(expected, PlayInfo::MetaData::TITLE);
}

/*!\test
 * Set artist information.
 */
void test_set_artist()
{
    static const std::string expected = "Deine Lakaien";

    data->meta_data_.add("artist", expected.c_str());
    check_single_meta_data(expected, PlayInfo::MetaData::ARTIST);
}

/*!\test
 * Set album information.
 */
void test_set_album()
{
    static const std::string expected = "Zombieland";

    data->meta_data_.add("album", expected.c_str());
    check_single_meta_data(expected, PlayInfo::MetaData::ALBUM);
}

/*!\test
 * Set audio codec information.
 */
void test_set_audio_codec()
{
    static const std::string expected = "MPEG 1 Audio, Layer 3 (MP3)";

    data->meta_data_.add("audio-codec", expected.c_str());
    check_single_meta_data(expected, PlayInfo::MetaData::CODEC);
}

/*!\test
 * Set minimum bitrate information.
 */
void test_set_minimum_bitrate()
{
    static const std::string expected = "158315";

    data->meta_data_.add("minimum-bitrate", expected.c_str());
    check_single_meta_data(expected, PlayInfo::MetaData::BITRATE_MIN);
}

/*!\test
 * Set maximum bitrate information.
 */
void test_set_maximum_bitrate()
{
    static const std::string expected = "159862";

    data->meta_data_.add("maximum-bitrate", expected.c_str());
    check_single_meta_data(expected, PlayInfo::MetaData::BITRATE_MAX);
}

/*!\test
 * Set nominal bitrate information.
 */
void test_set_nominal_bitrate()
{
    static const std::string expected = "160000";

    data->meta_data_.add("nominal-bitrate", expected.c_str());
    check_single_meta_data(expected, PlayInfo::MetaData::BITRATE_NOM);
}

/*!\test
 * Clear meta data works as expected.
 */
void test_clear_meta_data()
{
    data->meta_data_.add("title",           "a");
    data->meta_data_.add("artist",          "b");
    data->meta_data_.add("album",           "c");
    data->meta_data_.add("audio-codec",     "d");
    data->meta_data_.add("minimum-bitrate", "e");
    data->meta_data_.add("maximum-bitrate", "f");
    data->meta_data_.add("nominal-bitrate", "g");

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
