/*
 * Copyright (C) 2019, 2020  T+A elektroakustik GmbH & Co. KG
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

#ifndef DUMP_ENUM_VALUE_HH
#define DUMP_ENUM_VALUE_HH

#include <array>
#include <sstream>

template <typename T, size_t N>
static std::ostream &dump_enum_value(std::ostream &os,
                                     const std::array<const char *const, N> &names,
                                     const char *const prefix, const T val)
{
    static_assert(N == size_t(T::LAST_VALUE) + 1, "Wrong array size");
    os << prefix << "::"
       << (size_t(val) < names.size() ? names[size_t(val)] : "***INVALID***");
    return os;
}

#endif /* !DUMP_ENUM_VALUE_HH */
