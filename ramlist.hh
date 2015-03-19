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
 */
class RamList: public ListIface
{
  private:
    const ListIface *parent_list_;
    std::vector<Item *> items_;

    Item *get_nonconst_item(unsigned int line);

  public:
    RamList(const RamList &) = delete;
    RamList &operator=(const RamList &) = delete;

    explicit RamList(): parent_list_(this) {}
    ~RamList();

    unsigned int get_number_of_items() const override;
    bool empty() const override { return get_number_of_items() == 0; }
    void clear() override;

    const Item *get_item(unsigned int line) const override;
    void set_parent_list(const ListIface *parent) override;
    bool set_child_list(unsigned int line,
                        const std::shared_ptr<ListIface> &list) override;

    const ListIface &up() const override;
    const ListIface *down(unsigned int line) const override;

    unsigned int append(Item *item);
};

template <typename T>
static unsigned int append(RamList *l, T &&item)
{
    return l->append(new T(std::move(item)));
}

};

/*!@}*/

#endif /* !RAMLIST_HH */
