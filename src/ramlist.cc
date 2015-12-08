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
