/*
 * Copyright (C) 2017, 2019, 2022, 2023  T+A elektroakustik GmbH & Co. KG
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

    uint32_t get_rank() const { return rank_; }
    uint32_t get_bitrate() const { return bitrate_bits_per_second_; }
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
    size_t size() const { return backing_store_.size(); }

    void add(RankedLink &&link)
    {
        msg_log_assert(!is_finalized_);
        backing_store_.emplace_back(std::move(link));
    }

    void finalize(const std::function<bool(uint32_t)> &is_bitrate_in_range);

    class const_iterator
    {
      private:
        size_t idx_;
        size_t end_idx_;

        const std::vector<const RankedLink *> *playable_;
        const std::vector<const RankedLink *> *stuttering_;
        const std::vector<const RankedLink *> *current_vector_;
        size_t current_vector_index_;

      public:
        const_iterator(const const_iterator &) = default;
        const_iterator(const_iterator &&) = default;

        explicit const_iterator(const std::vector<const RankedLink *> *playable,
                                const std::vector<const RankedLink *> *stuttering):
            idx_(0),
            end_idx_(playable->size() + stuttering->size()),
            playable_(playable),
            stuttering_(stuttering),
            current_vector_(playable_->empty() ? stuttering_ : playable_),
            current_vector_index_(0)
        {}

        explicit const_iterator(size_t total_size):
            idx_(total_size),
            end_idx_(total_size),
            playable_(nullptr),
            stuttering_(nullptr),
            current_vector_(nullptr),
            current_vector_index_(0)
        {}

        const_iterator &operator++()
        {
            if(idx_ >= end_idx_)
                return *this;

            ++idx_;
            ++current_vector_index_;

            if(current_vector_ == playable_ &&
               current_vector_index_ == playable_->size())
            {
                current_vector_ = stuttering_;
                current_vector_index_ = 0;
            }

            return *this;
        }

        const_iterator &operator--()
        {
            if(idx_ == 0)
                return *this;

            --idx_;

            if(current_vector_index_ > 0)
                --current_vector_index_;
            else
            {
                current_vector_ = playable_;
                current_vector_index_ = playable_->size() - 1;
            }

            return *this;
        }

        bool operator!=(const const_iterator &other) const { return idx_ != other.idx_; }

        const RankedLink *operator*() const
        {
            return (*current_vector_)[current_vector_index_];
        }
    };

    const_iterator begin() const { return const_iterator(&playable_, &stuttering_); }
    const_iterator end() const { return const_iterator(playable_.size() + stuttering_.size()); }
};

}

#endif /* !AIRABLE_LINKS_HH */
