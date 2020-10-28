/*
 * Copyright (C) 2020  T+A elektroakustik GmbH & Co. KG
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

#ifndef PLAYLIST_CURSOR_HH
#define PLAYLIST_CURSOR_HH

#include <memory>
#include <string>

namespace Playlist
{

namespace Crawler
{

enum class Direction
{
    NONE,
    FORWARD,
    BACKWARD,
    LAST_VALUE = BACKWARD,
};

/*!
 * Base class for cursors into a directory hierarchy.
 *
 * This class does not do much except to provide a common type for generic
 * code.
 */
class CursorBase
{
  protected:
    explicit CursorBase() = default;

  public:
    CursorBase(const CursorBase &) = default;
    CursorBase(CursorBase &&) = default;
    CursorBase &operator=(const CursorBase &) = default;
    CursorBase &operator=(CursorBase &&) = default;

    virtual ~CursorBase() = default;
    virtual void clear() = 0;
    virtual std::unique_ptr<CursorBase> clone() const = 0;
    virtual bool advance(Direction direction) = 0;
    virtual void sync_request_with_pos() = 0;
    virtual std::string get_description(bool full = true) const = 0;

    template <typename T>
    std::unique_ptr<T> clone_as() const
    {
        return std::make_unique<T>(*static_cast<const T *>(this));
    }
};

}

}

#endif /* !PLAYLIST_CURSOR_HH */
