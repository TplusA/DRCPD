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

#include "streaminfo.hh"
#include "messages.h"

void StreamInfo::clear()
{
    stream_names_.clear();
}

ID::OurStream StreamInfo::insert(const char *fallback_title,
                                 ID::List list_id, unsigned int line)
{
    log_assert(fallback_title != nullptr);
    log_assert(list_id.is_valid());

    if(stream_names_.size() >= MAX_ENTRIES)
    {
        BUG("Too many stream IDs");
        return ID::OurStream::make_invalid();
    }

    while(1)
    {
        const ID::OurStream id = next_free_id_;
        ++next_free_id_;

        const auto result =
            stream_names_.emplace(id, StreamInfoItem(fallback_title,
                                                     list_id, line));

        if(result.first != stream_names_.end() && result.second)
            return id;
    }
}

void StreamInfo::forget(ID::OurStream id)
{
    if(stream_names_.erase(id) != 1)
        msg_error(EINVAL, LOG_ERR,
                  "Attempted to erase non-existent stream ID %u",
                  id.get().get_raw_id());
}

const StreamInfoItem *StreamInfo::lookup(ID::OurStream id) const
{
    const auto result = stream_names_.find(id);
    return (result != stream_names_.end()) ? &result->second : nullptr;
}
