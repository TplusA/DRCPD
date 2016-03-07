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

#ifndef PLAYINFO_HH
#define PLAYINFO_HH

#include <array>
#include <string>
#include <chrono>
#include <functional>

/*!
 * \addtogroup view_play_playinfo Data for player view
 * \ingroup view_play
 *
 * Stored data for currently playing stream.
 */
/*!@{*/

namespace PlayInfo
{

struct Reformatters
{
    using fn_t =  std::function<const std::string(const char *in)>;

    const fn_t bitrate;
};


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
        BITRATE,
        BITRATE_MIN,
        BITRATE_MAX,
        BITRATE_NOM,

        /* internal tags */
        INTERNAL_DRCPD_TITLE,
        INTERNAL_DRCPD_URL,

        METADATA_ID_LAST_REGULAR = BITRATE_NOM,
        METADATA_ID_FIRST_INTERNAL = INTERNAL_DRCPD_TITLE,
        METADATA_ID_LAST = INTERNAL_DRCPD_URL,
    };

    MetaData(const MetaData &) = delete;
    MetaData &operator=(const MetaData &) = default;

    explicit MetaData() {}

    std::array<std::string, METADATA_ID_LAST + 1> values_;

    void clear(bool is_update);
    void add(const char *key, const char *value, const Reformatters &reformat);

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
        STREAM_BUFFERING,
        STREAM_PLAYING,
        STREAM_PAUSED,

        STREAM_STATE_LAST = STREAM_PAUSED,
    };

    StreamState assumed_stream_state_;
    MetaData meta_data_;
    std::chrono::milliseconds stream_position_;
    std::chrono::milliseconds stream_duration_;

    explicit Data(StreamState state = STREAM_STOPPED):
        assumed_stream_state_(state),
        stream_position_(-1),
        stream_duration_(-1)
    {
        meta_data_.clear(false);
    }

    void set_buffering()
    {
        assumed_stream_state_ = STREAM_BUFFERING;
        meta_data_.clear(false);
    }

    void set_playing()
    {
        assumed_stream_state_ = STREAM_PLAYING;
        meta_data_.clear(false);
    }

    void set_stopped()
    {
        assumed_stream_state_ = STREAM_STOPPED;
        meta_data_.clear(false);
        stream_position_ = std::chrono::milliseconds(-1);
        stream_duration_ = std::chrono::milliseconds(-1);
    }
};

};

/*!@}*/

#endif /* !PLAYINFO_HH */
