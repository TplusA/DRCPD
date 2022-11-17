/*
 * Copyright (C) 2015, 2016, 2017, 2019, 2022  T+A elektroakustik GmbH & Co. KG
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

#include <algorithm>

#include "context_map.hh"
#include "de_tahifi_lists_context.h"
#include "messages.h"

const List::ContextInfo List::ContextMap::default_context_("#INVALID#",
                                                           "Invalid list context",
                                                           List::ContextInfo::INTERNAL_INVALID,
                                                           nullptr);

static bool is_invalid_string_id(const List::ContextMap &map, const char *id)
{
    if(id[0] == '\0' || id[0] == '#')
    {
        MSG_BUG("Invalid context ID \"%s\"", id);
        return true;
    }

    if(map[id].is_valid())
    {
        MSG_BUG("Duplicate context ID \"%s\"", id);
        return true;
    }

    return false;
}

List::context_id_t
List::ContextMap::append(const char *id, const char *description,
                         uint32_t flags,
                         const Player::LocalPermissionsIface *permissions)
{
    msg_log_assert(id != nullptr);
    msg_log_assert(description != nullptr);

    if(is_invalid_string_id(*this, id))
    {
        contexts_.push_back(default_context_);
        return INVALID_ID;
    }

    flags &= ContextInfo::PUBLIC_FLAGS_MASK;
    contexts_.emplace_back(ContextInfo(id, description, flags, permissions));

    const context_id_t new_id(contexts_.size() - 1);

    if(new_id > DBUS_LISTS_CONTEXT_ID_MAX)
        MSG_BUG("Too many list contexts (ignored)");

    return new_id;
}

const List::ContextInfo &
List::ContextMap::get_context_info_by_string_id(const std::string &id,
                                                context_id_t &ctx_id) const
{
    const auto &found =
        std::find_if(contexts_.begin(), contexts_.end(),
                     [&id] (const ContextInfo &info)
                     {
                         return info.string_id_ == id;
                     });

    if(found != contexts_.end())
    {
        ctx_id = found - contexts_.begin();
        return *found;
    }
    else
    {
        ctx_id = INVALID_ID;
        return List::ContextMap::default_context_;
    }
}
