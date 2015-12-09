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

#include "player.hh"
#include "playbackmode_state.hh"

bool Playback::Player::take(Playback::State &playback_state,
                            const List::DBusList &file_list, int line)
{
    if(current_state_ != nullptr)
        release();

    current_state_ = &playback_state;

    if(!current_state_->start(file_list, line))
        return false;

    current_state_->enqueue_next(stream_info_, true);

    return true;
}

void Playback::Player::release()
{
    stream_info_.clear();
}

void Playback::Player::enqueue_next()
{
    current_state_->enqueue_next(stream_info_, false);
}

const std::string *Playback::Player::get_original_stream_name(uint16_t id)
{
    return stream_info_.lookup_and_activate(id);
}
