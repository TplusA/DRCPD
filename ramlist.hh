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

#ifndef RAMLIST_HH
#define RAMLIST_HH

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
  private:
    std::vector<Item *> items_;

    Item *get_nonconst_item(unsigned int line);

  public:
    RamList(const RamList &) = delete;
    RamList &operator=(const RamList &) = delete;

    explicit RamList() {}
    ~RamList();

    unsigned int get_number_of_items() const override;
    bool empty() const override { return get_number_of_items() == 0; }
    void clear() override;

    const Item *get_item(unsigned int line) const override;

    unsigned int append(Item *item);
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
static unsigned int append(RamList *l, T &&item)
{
    return l->append(new T(std::move(item)));
}

};

/*!@}*/

#endif /* !RAMLIST_HH */
