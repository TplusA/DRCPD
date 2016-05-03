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

#include "busy.hh"

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

    enum class CopyMode
    {
        ALL,
        NON_EMPTY,
    };

    MetaData(const MetaData &) = delete;
    MetaData &operator=(const MetaData &) = delete;

    explicit MetaData() {}

    std::array<std::string, METADATA_ID_LAST + 1> values_;

    void clear(bool keep_internals);
    void add(const char *key, const char *value, const Reformatters &reformat);
    void copy_from(const MetaData &src, CopyMode mode);

    bool operator==(const MetaData &other) const;

    void dump(const char *what) const;
};

/*!
 * Stream playback information POD.
 */
class Data
{
  public:
    enum StreamState
    {
        STREAM_STOPPED,
        STREAM_BUFFERING,
        STREAM_PLAYING,
        STREAM_PAUSED,

        STREAM_STATE_LAST = STREAM_PAUSED,
    };

  private:
    StreamState assumed_stream_state_;

  public:
    MetaData meta_data_;
    std::chrono::milliseconds stream_position_;
    std::chrono::milliseconds stream_duration_;

    Data(const Data &) = delete;
    Data &operator=(const Data &) = delete;

    explicit Data(StreamState state = STREAM_STOPPED):
        assumed_stream_state_(state),
        stream_position_(-1),
        stream_duration_(-1)
    {
        meta_data_.clear(false);
    }

    void set_buffering()
    {
        Busy::set(Busy::Source::BUFFERING_STREAM);
        assumed_stream_state_ = STREAM_BUFFERING;
        meta_data_.clear(false);
    }

    void set_playing(bool clear_meta_data)
    {
        Busy::clear(Busy::Source::BUFFERING_STREAM);
        assumed_stream_state_ = STREAM_PLAYING;

        if(clear_meta_data)
            meta_data_.clear(false);
    }

    void set_paused() { assumed_stream_state_ = STREAM_PAUSED; }

    void set_stopped()
    {
        Busy::clear(Busy::Source::BUFFERING_STREAM);
        assumed_stream_state_ = STREAM_STOPPED;
        meta_data_.clear(false);
        stream_position_ = std::chrono::milliseconds(-1);
        stream_duration_ = std::chrono::milliseconds(-1);
    }

    StreamState get_assumed_state() const throw() { return assumed_stream_state_; }
};

};

/*!@}*/

#endif /* !PLAYINFO_HH */
