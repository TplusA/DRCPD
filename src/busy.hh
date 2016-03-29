/*
 * Copyright (C) 2016  T+A elektroakustik GmbH & Co. KG
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

#ifndef BUSY_HH
#define BUSY_HH

#include <inttypes.h>

namespace Busy
{

enum class Source
{
    WAITING_FOR_PLAYER,
    FILLING_PLAYER_QUEUE,
    BUFFERING_STREAM,
};

void init(const std::function<void(bool)> &state_changed_callback);
void set(Source src);
void clear(Source src);
bool is_busy();

}
#endif /* !BUSY_HH */