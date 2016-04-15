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

ssize_t binary_search_utf8(const List::DBusList &list,
                           const std::string &query)
    throw(Search::UnsortedException, List::DBusListException);

}

#endif /* !SEARCH_ALGO_HH */
