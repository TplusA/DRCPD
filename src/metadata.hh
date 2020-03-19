/*
 * Copyright (C) 2016, 2017, 2019, 2020  T+A elektroakustik GmbH & Co. KG
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

#ifndef METADATA_HH
#define METADATA_HH

#include <array>
#include <map>
#include <string>
#include <functional>

#include "idtypes.hh"

/*!
 * \addtogroup metadata Stream meta data
 */
/*!@{*/

namespace MetaData
{

namespace Reformatters
{
    std::string bitrate(const char *in);
}

/*!
 * Stream meta data POD as obtained from Streamplayer.
 */
class Set
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
        INTERNAL_DRCPD_OPAQUE_LINE_1,
        INTERNAL_DRCPD_OPAQUE_LINE_2,
        INTERNAL_DRCPD_OPAQUE_LINE_3,
        INTERNAL_DRCPD_URL,

        METADATA_ID_LAST_REGULAR = BITRATE_NOM,
        METADATA_ID_FIRST_INTERNAL = INTERNAL_DRCPD_TITLE,
        METADATA_ID_LAST = INTERNAL_DRCPD_URL,
    };

    /* XXX: Remove this */
    enum class CopyMode
    {
        ALL,
        NON_EMPTY,
    };

    std::array<std::string, METADATA_ID_LAST + 1> values_;

    Set(const Set &) = delete;
    Set(Set &&) = default;
    Set &operator=(const Set &) = delete;
    Set &operator=(Set &&) = default;

    explicit Set() {}

    void clear(bool keep_internals);
    void add(const char *key, const char *value);
    void add(const ID key_id, const char *value);
    void add(const ID key_id, std::string &&value);
    void copy_from(const Set &src, CopyMode mode);

    bool operator==(const Set &other) const;

    void dump(const char *what) const;
};

class Collection
{
  private:
    static constexpr const size_t MAX_ENTRIES = 30;
    std::map<ID::Stream, Set> meta_data_sets_;

  public:
    Collection(const Collection &) = delete;
    Collection &operator=(const Collection &) = delete;

    explicit Collection() {}

    bool is_full() const
    {
        return meta_data_sets_.size() >= MAX_ENTRIES;
    }

    void emplace(ID::Stream stream_id, Set &&src)
    {
        meta_data_sets_.emplace(stream_id, std::move(src));
    }

    bool forget_stream(ID::Stream stream_id)
    {
        const auto result(meta_data_sets_.erase(stream_id));
        return result > 0;
    }

    Set *get_meta_data_for_update(ID::Stream stream_id)
    {
        auto result(meta_data_sets_.find(stream_id));
        return (result != meta_data_sets_.end() ? &result->second : nullptr);
    }

    void clear()
    {
        meta_data_sets_.clear();
    }
};

};

/*!@}*/

#endif /* !METADATA_HH */
