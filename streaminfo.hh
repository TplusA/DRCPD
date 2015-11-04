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
#include <inttypes.h>

/*!
 * \addtogroup streaminfo Extra stream data
 *
 * Extra data for queued streams, indexed by ID.
 */
/*!@{*/

class StreamInfo
{
  private:
    std::map<uint16_t, std::string> stream_names_;
    uint16_t next_free_id_;
    uint16_t current_id_;

  public:
    static constexpr size_t MAX_ENTRIES = 20;
    static constexpr size_t MAX_ID      = 5 * MAX_ENTRIES;

    StreamInfo(const StreamInfo &) = delete;
    StreamInfo &operator=(const StreamInfo &) = delete;

    explicit StreamInfo():
        next_free_id_(0),
        current_id_(0)
    {}

    void clear();
    uint16_t insert(const char *fallback_title);
    void forget(uint16_t id);
    void forget() { forget(current_id_); }
    const std::string *lookup_and_activate(uint16_t id);
};

/*!@}*/

#endif /* !STREAMINFO_HH */
