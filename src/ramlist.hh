/*
 * Copyright (C) 2015, 2017, 2019, 2020  T+A elektroakustik GmbH & Co. KG
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

#ifndef RAMLIST_HH
#define RAMLIST_HH

#include <string>

#include "list.hh"

/*!
 * \addtogroup ram_list Lists with contents held in RAM
 * \ingroup list
 */
/*!@{*/

namespace List
{

/*!
 * A list with all list items stored in RAM.
 *
 * Before this list can be navigated in a meaningful way, is must be filled
 * with content (see #List::append() and #List::RamList::append()).
 */
class RamList: public ListIface
{
  public:
    class Viewport: public ListViewportBase
    {
        unsigned int get_default_view_size() const final override { return 0; }
    };

  private:
    const std::string list_iface_name_;
    std::vector<Item *> items_;

    Item *get_nonconst_item(unsigned int line);

  public:
    RamList(const RamList &) = delete;
    RamList &operator=(const RamList &) = delete;
    RamList(RamList &&) = default;

    explicit RamList(std::string &&list_iface_name):
        list_iface_name_(std::move(list_iface_name))
    {}

    ~RamList();

    const std::string &get_list_iface_name() const override { return list_iface_name_; }

    unsigned int get_number_of_items() const override;
    bool empty() const override { return get_number_of_items() == 0; }

    void enter_list(ID::List list_id) override
    {
        clear();
    }

    const Item *get_item(std::shared_ptr<ListViewportBase> vp,
                         unsigned int line) override
    {
        return get_nonconst_item(line);
    }

    const Item *get_item(unsigned int line)
    {
        return get_nonconst_item(line);
    }

    const Item *get_item(unsigned int line) const
    {
        return const_cast<RamList *>(this)->get_nonconst_item(line);
    }

    ID::List get_list_id() const override { return ID::List(); }

    void clear();
    unsigned int append(Item *item);
    void replace(unsigned int line, Item *item);
    void shift_up(unsigned int count);
    void shift_down(unsigned int count);

    void move_from(RamList &other)
    {
        items_.swap(other.items_);
        other.items_.clear();
    }
};

/*!
 * Append some item to a #List::RamList.
 *
 * This little helper function creates a new object and moves the given object
 * into the allocated space. There should be no overhead in terms of temporary
 * objects.
 *
 * \todo
 *     This may be inefficient if the move semantics do not work as they should
 *     and suddenly a copy ctor gets invoked, plus the memory allocation should
 *     probably use a memory pool. Time (and profiling) will tell.
 */
template <typename T>
static unsigned int append(RamList &l, T &&item)
{
    return l.append(new T(std::move(item)));
}

}

/*!@}*/

#endif /* !RAMLIST_HH */
