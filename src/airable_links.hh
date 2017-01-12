/*
 * Copyright (C) 2017  T+A elektroakustik GmbH & Co. KG
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

#ifndef AIRABLE_LINKS_HH
#define AIRABLE_LINKS_HH

#include <string>
#include <vector>
#include <functional>

#include "messages.h"

namespace Airable
{

class RankedLink
{
  private:
    uint32_t rank_;
    uint32_t bitrate_bits_per_second_;
    std::string link_;

  public:
    RankedLink(const RankedLink &) = delete;
    RankedLink(RankedLink &&) = default;
    RankedLink &operator=(const RankedLink &) = delete;

    explicit RankedLink(uint32_t rank, uint32_t rate,
                               const std::string &uri):
        rank_(rank),
        bitrate_bits_per_second_(rate),
        link_(uri)
    {}

    explicit RankedLink(uint32_t rank, uint32_t rate,
                               const char *const uri):
        rank_(rank),
        bitrate_bits_per_second_(rate),
        link_(uri)
    {}

    const uint32_t get_rank() const { return rank_; }
    const uint32_t get_bitrate() const { return bitrate_bits_per_second_; }
    const std::string &get_stream_link() const { return link_; }
};

class SortedLinks
{
  private:
    bool is_finalized_;
    std::vector<RankedLink> backing_store_;
    std::vector<const RankedLink *> playable_;
    std::vector<const RankedLink *> stuttering_;

  public:
    SortedLinks(const SortedLinks &) = delete;
    SortedLinks(SortedLinks &&) = default;
    SortedLinks &operator=(const SortedLinks &) = delete;

    explicit SortedLinks():
        is_finalized_(false)
    {}

    void clear()
    {
        is_finalized_ = false;
        backing_store_.clear();
        playable_.clear();
        stuttering_.clear();
    }

    bool empty() const { return backing_store_.empty(); }

    void add(RankedLink &&link)
    {
        log_assert(!is_finalized_);
        backing_store_.emplace_back(std::move(link));
    }

    void finalize(const std::function<bool(uint32_t)> &is_bitrate_in_range);
};

}

#endif /* !AIRABLE_LINKS_HH */
