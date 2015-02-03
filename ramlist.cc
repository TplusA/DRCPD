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

void List::RamList::set_parent_list(const List::ListIface *parent)
{
    parent_list_ = parent;
}

bool List::RamList::set_child_list(unsigned int line,
                                   const std::shared_ptr<List::ListIface> &list)
{
    if(list == nullptr)
        return false;

    auto item = dynamic_cast<TreeItem *>(get_nonconst_item(line));

    if(item == nullptr)
        return false;

    list->set_parent_list(this);
    item->set_child_list(list);

    return true;
}

const List::ListIface &List::RamList::up() const
{
    return *parent_list_;
}

const List::ListIface *List::RamList::down(unsigned int line) const
{
    auto item = dynamic_cast<const List::TreeItem *>(get_item(line));

    if(item == nullptr)
        return nullptr;

    return item->down();
}

unsigned int List::RamList::append(Item *item)
{
    items_.push_back(item);
    return items_.size() - 1;
}
