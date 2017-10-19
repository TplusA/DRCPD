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

#ifndef SCREEN_IDS_HH
#define SCREEN_IDS_HH

namespace ScreenID
{

using id_t = uint16_t;

static constexpr id_t INVALID_ID       = 0;
static constexpr id_t FIRST_REGULAR_ID = 1;
static constexpr id_t FIRST_ERROR_ID   = 1U << (sizeof(uint16_t) * 8 - 1);

enum class Error
{
    INVALID = INVALID_ID,

    ENTER_LIST_PERMISSION_DENIED = FIRST_ERROR_ID,
    ENTER_LIST_MEDIA_IO,
    ENTER_LIST_NET_IO,
    ENTER_LIST_PROTOCOL,
    ENTER_LIST_AUTHENTICATION,
    ENTER_CONTEXT_AUTHENTICATION,
};

}

#endif /* !SCREEN_IDS_HH */
