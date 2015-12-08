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

#include "streaminfo.hh"
#include "messages.h"

static uint16_t next_id(uint16_t &next_free_id)
{
    if(++next_free_id == 0)
        ++next_free_id;
    else if(next_free_id > StreamInfo::MAX_ID)
        next_free_id = 1;

    return next_free_id;
}

void StreamInfo::clear()
{
    current_id_ = 0;
    stream_names_.clear();
}

uint16_t StreamInfo::insert(const char *fallback_title)
{
    log_assert(fallback_title != NULL);

    if(stream_names_.size() >= MAX_ENTRIES)
    {
        BUG("Too many stream IDs");
        return 0;
    }

    while(1)
    {
        const uint16_t id = next_id(next_free_id_);
        const auto result = stream_names_.emplace(id, std::string(fallback_title));

        if(result.first != stream_names_.end() && result.second)
            return id;
    }
}

void StreamInfo::forget(uint16_t id)
{
    if(stream_names_.erase(id) != 1)
        msg_error(EINVAL, LOG_ERR,
                  "Attempted to erase non-existent stream ID %u", id);
    else if(id == current_id_)
        current_id_ = 0;
}

const std::string *StreamInfo::lookup_and_activate(uint16_t id)
{
    if(current_id_ != 0)
        forget();

    const auto result = stream_names_.find(id);

    if(result != stream_names_.end())
    {
        current_id_ = id;
        return &result->second;
    }
    else
    {
        current_id_ = 0;
        return NULL;
    }
}
