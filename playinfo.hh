#ifndef PLAYINFO_HH
#define PLAYINFO_HH

#include <array>
#include <string>
#include <chrono>

/*!
 * \addtogroup view_play_playinfo Data for player view
 * \ingroup view_play
 *
 * Stored data for currently playing stream.
 */
/*!@{*/

namespace PlayInfo
{

/*!
 * Stream meta data POD as obtained from Streamplayer.
 */
class MetaData
{
  public:
    enum ID
    {
        TITLE = 0,
        ARTIST,
        ALBUM,
        CODEC,
        BITRATE_MIN,
        BITRATE_MAX,
        BITRATE_NOM,

        METADATA_ID_LAST = BITRATE_NOM
    };

    MetaData(const MetaData &) = delete;
    MetaData &operator=(const MetaData &) = default;

    explicit MetaData() {}

    std::array<std::string, METADATA_ID_LAST + 1> values_;

    void clear();
    void add(const char *key, const char *value);

    bool operator==(const MetaData &other) const;
};

/*!
 * Stream playback information POD.
 */
class Data
{
  public:
    Data(const Data &) = delete;
    Data &operator=(const Data &) = delete;

    enum StreamState
    {
        STREAM_STOPPED,
        STREAM_PLAYING,
        STREAM_PAUSED,
    };

    StreamState assumed_stream_state_;
    std::string url_;
    MetaData meta_data_;
    std::chrono::milliseconds stream_position_;
    std::chrono::milliseconds stream_duration_;

    explicit Data():
        assumed_stream_state_(STREAM_STOPPED),
        stream_position_(-1),
        stream_duration_(-1)
    {
        meta_data_.clear();
    }
};

};

/*!@}*/

#endif /* !PLAYINFO_HH */
