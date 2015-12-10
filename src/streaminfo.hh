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

#ifndef STREAMINFO_HH
#define STREAMINFO_HH

#include <map>
#include <string>

#include "idtypes.hh"

/*!
 * \addtogroup streaminfo Extra stream data
 *
 * Extra data for queued streams, indexed by ID.
 */
/*!@{*/

class StreamInfoItem
{
  public:
    const std::string alt_name_;
    const ID::List list_id_;
    const unsigned int line_;

    StreamInfoItem(StreamInfoItem &&) = default;
    StreamInfoItem(const StreamInfoItem &) = delete;
    StreamInfoItem &operator=(const StreamInfoItem &) = delete;

    explicit StreamInfoItem(std::string &&alt_name,
                            ID::List list_id, unsigned int line):
        alt_name_(alt_name),
        list_id_(list_id),
        line_(line)
    {}
};

class StreamInfo
{
  private:
    std::map<uint16_t, StreamInfoItem> stream_names_;
    uint16_t next_free_id_;

  public:
    static constexpr size_t MAX_ENTRIES = 20;
    static constexpr size_t MAX_ID      = 5 * MAX_ENTRIES;

    StreamInfo(const StreamInfo &) = delete;
    StreamInfo &operator=(const StreamInfo &) = delete;

    explicit StreamInfo():
        next_free_id_(0)
    {}

    void clear();
    uint16_t insert(const char *fallback_title,
                    ID::List list_id, unsigned int line);
    void forget(uint16_t id);
    const StreamInfoItem *lookup(uint16_t id) const;
};

/*!@}*/

#endif /* !STREAMINFO_HH */
