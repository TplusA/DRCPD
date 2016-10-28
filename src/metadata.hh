/*
 * Copyright (C) 2016  T+A elektroakustik GmbH & Co. KG
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

struct Reformatters
{
    using fn_t =  std::function<const std::string(const char *in)>;

    const fn_t bitrate;
};

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

    /* FIXME: The default copy ctor should not be required */
    Set(const Set &) = default;
    Set &operator=(const Set &) = delete;
    Set(Set &&) = default;

    explicit Set() {}

    std::array<std::string, METADATA_ID_LAST + 1> values_;

    void clear(bool keep_internals);
    void add(const char *key, const char *value, const Reformatters &reformat);
    void add(const ID key_id, const char *value, const Reformatters &reformat);
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

    void insert(ID::Stream stream_id, const Set &src)
    {
        meta_data_sets_.emplace(stream_id, src);
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
