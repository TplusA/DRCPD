/*
 * Copyright (C) 2015--2017, 2019--2021  T+A elektroakustik GmbH & Co. KG
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

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <cstring>
#include <algorithm>

#include "metadata.hh"
#include "messages.h"

struct KeyToID
{
    /*!
     * A key a sent by Streamplayer, thus GStreamer.
     */
    const char *const key;

    /*!
     * Internally used ID for GStreamer keys.
     */
    MetaData::Set::ID id;

    constexpr KeyToID(const char *k, const MetaData::Set::ID i):
        key(k),
        id(i)
    {}
};

static const std::array<const KeyToID, MetaData::Set::METADATA_ID_LAST + 1> key_to_id
{
    KeyToID("title",           MetaData::Set::TITLE),
    KeyToID("artist",          MetaData::Set::ARTIST),
    KeyToID("album",           MetaData::Set::ALBUM),
    KeyToID("audio-codec",     MetaData::Set::CODEC),
    KeyToID("bitrate",         MetaData::Set::BITRATE),
    KeyToID("minimum-bitrate", MetaData::Set::BITRATE_MIN),
    KeyToID("maximum-bitrate", MetaData::Set::BITRATE_MAX),
    KeyToID("nominal-bitrate", MetaData::Set::BITRATE_NOM),
    KeyToID("x-drcpd-title",   MetaData::Set::INTERNAL_DRCPD_TITLE),
    KeyToID("x-drcpd-line-1",  MetaData::Set::INTERNAL_DRCPD_OPAQUE_LINE_1),
    KeyToID("x-drcpd-line-2",  MetaData::Set::INTERNAL_DRCPD_OPAQUE_LINE_2),
    KeyToID("x-drcpd-line-3",  MetaData::Set::INTERNAL_DRCPD_OPAQUE_LINE_3),
    KeyToID("x-drcpd-url",     MetaData::Set::INTERNAL_DRCPD_URL),
};

void MetaData::Set::clear(bool keep_internals)
{
    const size_t last = keep_internals ? METADATA_ID_LAST_REGULAR : METADATA_ID_LAST;

    for(size_t i = 0; i <= last; ++i)
        this->values_[i].clear();
}

const char *MetaData::get_tag_name(MetaData::Set::ID id)
{
    return key_to_id[id].key;
}

void MetaData::Set::add(const char *key, const char *value)
{
    const auto &found(std::find_if(key_to_id.begin(), key_to_id.end(),
                                   [&key] (const auto &entry)
                                   { return strcmp(key, entry.key) == 0; }));

    if(found != key_to_id.end())
        add(found->id, value);
}

void MetaData::Set::add(const MetaData::Set::ID key_id, const char *value)
{
    switch(key_id)
    {
      case BITRATE:
      case BITRATE_MIN:
      case BITRATE_MAX:
      case BITRATE_NOM:
        if(value != nullptr)
            this->values_[key_id] = Reformatters::bitrate(value);
        else
            this->values_[key_id].clear();

        return;

      case TITLE:
      case ARTIST:
      case ALBUM:
      case CODEC:
      case INTERNAL_DRCPD_TITLE:
      case INTERNAL_DRCPD_OPAQUE_LINE_1:
      case INTERNAL_DRCPD_OPAQUE_LINE_2:
      case INTERNAL_DRCPD_OPAQUE_LINE_3:
      case INTERNAL_DRCPD_URL:
        break;
    }

    if(value != nullptr)
        this->values_[key_id] = value;
    else
        this->values_[key_id].clear();
}

void MetaData::Set::add(const MetaData::Set::ID key_id, std::string &&value)
{
    switch(key_id)
    {
      case BITRATE:
      case BITRATE_MIN:
      case BITRATE_MAX:
      case BITRATE_NOM:
        this->values_[key_id] = Reformatters::bitrate(value.c_str());
        return;

      case TITLE:
      case ARTIST:
      case ALBUM:
      case CODEC:
      case INTERNAL_DRCPD_TITLE:
      case INTERNAL_DRCPD_OPAQUE_LINE_1:
      case INTERNAL_DRCPD_OPAQUE_LINE_2:
      case INTERNAL_DRCPD_OPAQUE_LINE_3:
      case INTERNAL_DRCPD_URL:
        break;
    }

    this->values_[key_id] = std::move(value);
}

bool MetaData::Set::operator==(const Set &other) const
{
    for(size_t i = 0; i < values_.size(); ++i)
        if(values_[i] != other.values_[i])
            return false;

    return true;
}

void MetaData::Set::dump(const char *what) const
{
    msg_info("Meta data \"%s\"", what);

    for(const auto &entry : key_to_id)
        msg_info("%18s: \"%s\"", entry.key, this->values_[entry.id].c_str());
}
