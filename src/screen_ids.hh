/*
 * Copyright (C) 2017, 2019, 2021  T+A elektroakustik GmbH & Co. KG
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

#ifndef SCREEN_IDS_HH
#define SCREEN_IDS_HH

namespace ScreenID
{

using id_t = uint16_t;

static constexpr id_t INVALID_ID             = 0;
static constexpr id_t FIRST_REGULAR_ID       = 1;
static constexpr id_t IS_REAL_ERROR_MASK     = 1U << (sizeof(uint16_t) * 8 - 1);
static constexpr id_t IS_SYSTEM_MESSAGE_MASK = 1U << (sizeof(uint16_t) * 8 - 2);

enum class Error
{
    INVALID = INVALID_ID,

    /* not quite errors */
    OAUTH_REQUEST = FIRST_REGULAR_ID,

    /* true errors */
    ENTER_LIST_PERMISSION_DENIED = IS_REAL_ERROR_MASK,
    ENTER_LIST_MEDIA_IO,
    ENTER_LIST_NET_IO,
    ENTER_LIST_PROTOCOL,
    ENTER_LIST_AUTHENTICATION,
    ENTER_CONTEXT_AUTHENTICATION,

    SYSTEM_ERROR_NETWORK = IS_REAL_ERROR_MASK | IS_SYSTEM_MESSAGE_MASK,
};

static inline bool is_real_error(Error code)
{
    return (id_t(code) & IS_REAL_ERROR_MASK) != 0;
}

}

#endif /* !SCREEN_IDS_HH */
