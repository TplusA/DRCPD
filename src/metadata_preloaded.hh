/*
 * Copyright (C) 2015, 2016, 2017  T+A elektroakustik GmbH & Co. KG
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

#ifndef METADATA_PRELOADED_HH
#define METADATA_PRELOADED_HH

#include <string>

/*!
 * \addtogroup metadata
 */
/*!@{*/

namespace MetaData
{

/*!
 * Minimalist version of #MetaData::Set.
 *
 * This structure is going to be embedded into each list item, so it better be
 * small. It represents the essential stream meta data in cases where the meta
 * data is extracted from an external source, not from the stream itself.
 *
 * This is often the case with streams from TIDAL or Deezer played over
 * Airable. In this setting, the streams frequently do not contain any useful
 * meta data, but these data can be extracted from the Airable directory.
 */
class PreloadedSet
{
  public:
    const std::string artist_;
    const std::string album_;
    const std::string title_;

    explicit PreloadedSet() {}

    explicit PreloadedSet(const char *artist, const char *album,
                          const char *title):
        artist_(artist != nullptr ? artist : ""),
        album_(album != nullptr ? album : ""),
        title_(title != nullptr ? title : "")
    {}

    explicit PreloadedSet(const std::string &artist, const std::string &album,
                          const std::string &title):
        artist_(artist),
        album_(album),
        title_(title)
    {}

    bool have_anything() const
    {
        return !artist_.empty() || !album_.empty() || !title_.empty();
    }

    /* For special purposes: Like operator=(), but more explicit. */
    void copy_from(const PreloadedSet &src)
    {
        const_cast<std::string &>(artist_) = src.artist_;
        const_cast<std::string &>(album_) = src.album_;
        const_cast<std::string &>(title_) = src.title_;
    }

    /* For special purposes: Clear all strings. */
    void clear_individual_copy()
    {
        const_cast<std::string &>(artist_).clear();
        const_cast<std::string &>(album_).clear();
        const_cast<std::string &>(title_).clear();
    }
};

}

/*!@}*/

#endif /* !METADATA_PRELOADED_HH */
