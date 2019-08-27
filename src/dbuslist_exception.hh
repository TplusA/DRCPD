/*
 * Copyright (C) 2015, 2019  T+A elektroakustik GmbH & Co. KG
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

#ifndef DBUSLIST_EXCEPTION_HH
#define DBUSLIST_EXCEPTION_HH

#include "de_tahifi_lists_errors.hh"

/*!
 * \addtogroup dbus_list_exception Exception for reading lists from D-Bus
 * \ingroup dbus_list
 *
 * This exception is possibly thrown while reading lists from D-Bus.
 */
/*!@{*/

namespace List
{

class DBusListException
{
  private:
    const ListError error_;
    const bool is_dbus_error_;

  public:
    DBusListException(const DBusListException &) = delete;
    DBusListException &operator=(const DBusListException &) = delete;
    DBusListException(DBusListException &&) = default;

    constexpr explicit DBusListException(ListError error,
                                         bool dbus_error = false) noexcept:
        error_(error),
        is_dbus_error_(dbus_error)
    {}

    constexpr explicit DBusListException(ListError::Code error,
                                         bool dbus_error = false) noexcept:
        error_(error),
        is_dbus_error_(dbus_error)
    {}

    const bool is_dbus_error() const noexcept
    {
        return is_dbus_error_;
    }

    const ListError::Code get() const noexcept
    {
        return error_.get();
    }

    const char *what() const noexcept
    {
        return error_.to_string();
    }
};

};

/*!@}*/

#endif /* !DBUSLIST_EXCEPTION_HH */
