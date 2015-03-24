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

#include "ramlist.hh"

List::RamList::~RamList()
{
    for(auto i : items_)
        delete i;
}

unsigned int List::RamList::get_number_of_items() const
{
    return items_.size();
}

void List::RamList::clear()
{
    for(auto i : items_)
        delete i;

    items_.clear();
}

const List::Item *List::RamList::get_item(unsigned int line) const
{
    return const_cast<List::RamList *>(this)->get_nonconst_item(line);
}

List::Item *List::RamList::get_nonconst_item(unsigned int line)
{
    if(line >= get_number_of_items())
        return nullptr;
    else
        return items_[line];
}

unsigned int List::RamList::append(Item *item)
{
    items_.push_back(item);
    return items_.size() - 1;
}
