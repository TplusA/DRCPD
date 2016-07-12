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

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <cstring>
#include <algorithm>

#include "playinfo.hh"
#include "messages.h"

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
key_to_id[PlayInfo::MetaData::METADATA_ID_LAST + 1] =
{
    { "title",           PlayInfo::MetaData::TITLE, },
    { "artist",          PlayInfo::MetaData::ARTIST, },
    { "album",           PlayInfo::MetaData::ALBUM, },
    { "audio-codec",     PlayInfo::MetaData::CODEC, },
    { "bitrate",         PlayInfo::MetaData::BITRATE, },
    { "minimum-bitrate", PlayInfo::MetaData::BITRATE_MIN, },
    { "maximum-bitrate", PlayInfo::MetaData::BITRATE_MAX, },
    { "nominal-bitrate", PlayInfo::MetaData::BITRATE_NOM, },
    { "x-drcpd-title",   PlayInfo::MetaData::INTERNAL_DRCPD_TITLE, },
    { "x-drcpd-url",     PlayInfo::MetaData::INTERNAL_DRCPD_URL, },
};

void PlayInfo::MetaData::clear(bool keep_internals)
{
    const size_t last = keep_internals ? METADATA_ID_LAST_REGULAR : METADATA_ID_LAST;

    for(size_t i = 0; i <= last; ++i)
        this->values_[i].clear();
}

void PlayInfo::MetaData::add(const char *key, const char *value,
                             const Reformatters &reformat)
{
    for(const auto &entry : key_to_id)
    {
        if(strcmp(key, entry.key) == 0)
        {
            switch(entry.id)
            {
              case BITRATE:
              case BITRATE_MIN:
              case BITRATE_MAX:
              case BITRATE_NOM:
                if(!reformat.bitrate)
                    break;

                if(value != NULL)
                    this->values_[entry.id] = reformat.bitrate(value);
                else
                    this->values_[entry.id].clear();

                return;

              case TITLE:
              case ARTIST:
              case ALBUM:
              case CODEC:
              case INTERNAL_DRCPD_TITLE:
              case INTERNAL_DRCPD_URL:
                break;
            }

            if(value != NULL)
                this->values_[entry.id] = value;
            else
                this->values_[entry.id].clear();

            return;
        }
    }
}

void PlayInfo::MetaData::copy_from(const MetaData &src, CopyMode mode)
{
    switch(mode)
    {
      case CopyMode::ALL:
        std::copy(src.values_.begin(), src.values_.end(),
                  this->values_.begin());
        break;

      case CopyMode::NON_EMPTY:
        {
            auto dest(this->values_.begin());

            for(auto &it : src.values_)
            {
                if(!it.empty())
                    *dest = it;

                ++dest;
            }
        }

        break;
    }
}

bool PlayInfo::MetaData::operator==(const MetaData &other) const
{
    for(size_t i = 0; i < values_.size(); ++i)
        if(values_[i] != other.values_[i])
            return false;

    return true;
}

void PlayInfo::MetaData::dump(const char *what) const
{
    msg_info("Meta data \"%s\"", what);

    for(const auto &entry : key_to_id)
        msg_info("%18s: \"%s\"", entry.key, this->values_[entry.id].c_str());
}
