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

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <cstring>

#include "playinfo.hh"

void PlayInfo::MetaData::clear()
{
    for(size_t i = 0; i < values_.size(); ++i)
        values_[i].clear();
}

void PlayInfo::MetaData::add(const char *key, const char *value,
                             const Reformatters &reformat)
{
    static const struct
    {
        /*!
         * A key a sent by Streamplayer, thus GStreamer.
         */
        const char *const key;

        /*!
         * Internally used ID for GStreamer keys.
         */
        PlayInfo::MetaData::ID id;
    }
    key_to_id[METADATA_ID_LAST + 1] =
    {
        { "title",           TITLE, },
        { "artist",          ARTIST, },
        { "album",           ALBUM, },
        { "audio-codec",     CODEC, },
        { "minimum-bitrate", BITRATE_MIN, },
        { "maximum-bitrate", BITRATE_MAX, },
        { "nominal-bitrate", BITRATE_NOM, },
    };

    for(auto entry : key_to_id)
    {
        if(strcmp(key, entry.key) == 0)
        {
            switch(entry.id)
            {
              case BITRATE_MIN:
              case BITRATE_MAX:
              case BITRATE_NOM:
                if(!reformat.bitrate)
                    break;

                values_[entry.id] = reformat.bitrate(value);
                return;

              case TITLE:
              case ARTIST:
              case ALBUM:
              case CODEC:
                break;
            }

            values_[entry.id] = value;
            return;
        }
    }
}

bool PlayInfo::MetaData::operator==(const MetaData &other) const
{
    for(size_t i = 0; i < values_.size(); ++i)
        if(values_[i] != other.values_[i])
            return false;

    return true;
}
