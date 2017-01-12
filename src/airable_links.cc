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

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <algorithm>

#include "airable_links.hh"

void Airable::SortedLinks::finalize(const std::function<bool(uint32_t)> &is_bitrate_in_range)
{
    log_assert(!is_finalized_);
    is_finalized_ = true;

    if(backing_store_.empty())
        return;

    for(const auto &l : backing_store_)
    {
        if(is_bitrate_in_range(l.get_bitrate()))
            playable_.push_back(&l);
        else
            stuttering_.push_back(&l);
    }

    std::sort(playable_.begin(), playable_.end(),
              [] (const RankedLink *a, const RankedLink *b)
              {
                  if(a->get_rank() != b->get_rank())
                      return a->get_rank() > b->get_rank();

                  if(a->get_bitrate() != b->get_bitrate())
                      return a->get_bitrate() > b->get_bitrate();

                  return false;
              });

    std::sort(stuttering_.begin(), stuttering_.end(),
              [] (const RankedLink *a, const RankedLink *b)
              {
                  if(a->get_bitrate() != b->get_bitrate())
                      return a->get_bitrate() < b->get_bitrate();

                  if(a->get_rank() != b->get_rank())
                      return a->get_rank() > b->get_rank();

                  return false;
              });

    for(const auto &l : playable_)
        msg_info("Sorted link: rank %u, bit rate %u, \"%s\"",
                 l->get_rank(), l->get_bitrate(), l->get_stream_link().c_str());

    for(const auto &l : stuttering_)
        msg_info("Sorted link: rank %u, bit rate %u (beyond bandwidth limit), \"%s\"",
                 l->get_rank(), l->get_bitrate(), l->get_stream_link().c_str());
}
