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

#include <algorithm>

#include "streaminfo.hh"
#include "messages.h"

static ID::List get_list_id_for_stream_id(const StreamInfo &sinfo,
                                          ID::OurStream stream_id)
{
    const auto item = sinfo.lookup(stream_id);
    return (item != nullptr) ? item->list_id_ : ID::List();
}

static void ref_list_id(std::map<ID::List, size_t> &list_refcounts,
                        ID::List list_id)
{
    auto item = list_refcounts.find(list_id);

    if(item != list_refcounts.end())
        ++item->second;
    else
        list_refcounts[list_id] = 1;
}

static void unref_list_id(std::map<ID::List, size_t> &list_refcounts,
                          ID::List list_id)
{
    auto item = list_refcounts.find(list_id);

    if(item == list_refcounts.end())
        return;

    log_assert(item->second > 0);

    if(--item->second == 0)
        list_refcounts.erase(list_id);
}

void StreamInfo::clear()
{
    stream_names_.clear();
    referenced_lists_.clear();
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
        {
            ref_list_id(referenced_lists_, list_id);
            return id;
        }
    }
}

void StreamInfo::forget(ID::OurStream id)
{
    const ID::List list_id(get_list_id_for_stream_id(*this, id));

    if(stream_names_.erase(id) == 1)
        unref_list_id(referenced_lists_, list_id);
    else
        msg_error(EINVAL, LOG_ERR,
                  "Attempted to erase non-existent stream ID %u",
                  id.get().get_raw_id());
}

StreamInfoItem *StreamInfo::lookup_for_update(ID::OurStream id)
{
    auto result = stream_names_.find(id);
    return (result != stream_names_.end()) ? &result->second : nullptr;
}

size_t StreamInfo::get_referenced_lists(std::array<ID::List, MAX_ENTRIES> &list_ids) const
{
    if(referenced_lists_.empty())
        return 0;

    size_t number_of_list_ids = 0;

    for(const auto &it : referenced_lists_)
        list_ids[number_of_list_ids++] = it.first;

    std::sort(list_ids.begin(), list_ids.begin() + number_of_list_ids);

    return number_of_list_ids;
}
