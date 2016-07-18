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
#include <string>
#include <functional>

#include "idtypes.hh"

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
    void copy_from(const Set &src, CopyMode mode);

    bool operator==(const Set &other) const;

    void dump(const char *what) const;
};

};

#endif /* !METADATA_HH */
