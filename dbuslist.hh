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

#ifndef DBUSLIST_HH
#define DBUSLIST_HH

#include "list.hh"
#include "lists_dbus.h"
#include "ramlist.hh"
#include "messages.h"

/*!
 * \addtogroup dbus_list Lists with contents filled directly from D-Bus
 * \ingroup list
 */
/*!@{*/

namespace List
{

/*!
 * A list filled from D-Bus, with only fractions of the list held in RAM.
 */
class DBusList: public ListIface
{
  public:
    typedef List::Item *(*const NewItemFn)(const char *name, bool is_directory);

  private:
    tdbuslistsNavigation *const dbus_proxy_;

    /*!
     * Window size.
     */
    const unsigned int number_of_prefetched_items_;

    /*!
     * Callback that constructs a #List::Item from raw list data.
     */
    const NewItemFn new_item_fn_;

    /*!
     * Total number of items as reported over D-Bus.
     *
     * This gets updated by #List::DBusList::enter_list().
     */
    unsigned int number_of_items_;

    /*!
     * Simple POD structure for storing a little window of the list.
     */
    struct CacheData
    {
      public:
        ID::List list_id_;
        unsigned int first_item_line_;
        RamList items_;

        CacheData(const CacheData &) = delete;
        CacheData &operator=(const CacheData &) = delete;

        explicit CacheData():
            first_item_line_(0)
        {}

        const List::Item *operator[](unsigned int line) const
        {
            log_assert(line >= first_item_line_);
            log_assert(line < first_item_line_ + items_.get_number_of_items());

            return items_.get_item(line - first_item_line_);
        }
    };

    CacheData window_;

  public:
    DBusList(const DBusList &) = delete;
    DBusList &operator=(const DBusList &) = delete;

    explicit DBusList(tdbuslistsNavigation *nav_proxy, unsigned int prefetch,
                      NewItemFn new_item_fn):
        dbus_proxy_(nav_proxy),
        number_of_prefetched_items_(prefetch),
        new_item_fn_(new_item_fn),
        number_of_items_(0)
    {}

    unsigned int get_number_of_items() const override;
    bool empty() const override;
    bool enter_list(ID::List list_id, unsigned int line) override;

    const Item *get_item(unsigned int line) const override;

    tdbuslistsNavigation *get_dbus_proxy() const { return dbus_proxy_; }

  private:
    bool is_line_cached(unsigned int line) const;
    bool scroll_to_line(unsigned int line);
    bool fill_cache_from_scratch(unsigned int line);
};

};

/*!@}*/

#endif /* !DBUSLIST_HH */
