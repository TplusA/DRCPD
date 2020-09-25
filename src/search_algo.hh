/*
 * Copyright (C) 2016, 2019, 2020  T+A elektroakustik GmbH & Co. KG
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

#ifndef SEARCH_ALGO_HH
#define SEARCH_ALGO_HH

#include "dbuslist.hh"

namespace Search
{

class UnsortedException
{
  public:
    UnsortedException &operator=(const UnsortedException &) = delete;

    explicit UnsortedException() {}
};

ssize_t binary_search_utf8(List::DBusList &list, const std::string &query);

}

#endif /* !SEARCH_ALGO_HH */
