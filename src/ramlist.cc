/*
 * Copyright (C) 2015, 2019, 2020  T+A elektroakustik GmbH & Co. KG
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

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <algorithm>

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

void List::RamList::replace(unsigned int line, Item *item)
{
    if(items_[line] != nullptr)
        delete items_[line];

    items_[line] = item;
}

void List::RamList::shift_up(unsigned int count)
{
    if(count == 0)
        return;

    std::for_each(items_.begin(), items_.begin() + count, [] (Item *p) { delete p; });
    std::move(items_.begin() + count, items_.end(), items_.begin());
    std::for_each(items_.end() - count, items_.end(), [] (Item *&p) { p = nullptr; });
}

void List::RamList::shift_down(unsigned int count)
{
    if(count == 0)
        return;

    std::for_each(items_.rbegin(), items_.rbegin() + count, [] (Item *p) { delete p; });
    std::move(items_.rbegin() + count, items_.rend(), items_.rbegin());
    std::for_each(items_.rend() - count, items_.rend(), [] (Item *&p) { p = nullptr; });
}
