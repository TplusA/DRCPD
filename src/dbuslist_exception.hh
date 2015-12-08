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
