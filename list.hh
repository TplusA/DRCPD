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

#ifndef LIST_HH
#define LIST_HH

#include <string>
#include <vector>
#include <memory>

/*!
 * \addtogroup list List data model
 */
/*!@{*/

namespace List
{

class ListIface;

class Item
{
  protected:
    unsigned int flags_;

    explicit Item(unsigned int flags):
        flags_(flags)
    {}

  public:
    Item(const Item &) = delete;
    Item &operator=(const Item &) = delete;
    explicit Item(Item &&) = default;

    virtual ~Item() {}

    unsigned int get_flags() const
    {
        return flags_;
    }
};

class TreeItem: virtual public Item
{
  protected:
    std::shared_ptr<ListIface> child_list_;

  public:
    TreeItem(const TreeItem &) = delete;
    TreeItem &operator=(const TreeItem &) = delete;
    explicit TreeItem(TreeItem &&) = default;

    explicit TreeItem(unsigned int flags):
        Item(flags),
        child_list_(nullptr)
    {}

    void set_child_list(const std::shared_ptr<ListIface> &list)
    {
        child_list_ = list;
    }

    const List::ListIface *down() const
    {
        return child_list_.get();
    }
};

class TextItem: virtual public Item
{
  protected:
    std::string text_;
    bool text_is_translatable_;

  public:
    TextItem(const TextItem &) = delete;
    TextItem &operator=(const TextItem &) = delete;
    explicit TextItem(TextItem &&) = default;

    explicit TextItem(unsigned int flags):
        Item(flags),
        text_is_translatable_(false)
    {}

    explicit TextItem(const char *text, bool text_is_translatable,
                      unsigned int flags):
        Item(flags),
        text_(text),
        text_is_translatable_(text_is_translatable)
    {}

    const char *get_text() const;
};

class ListIface
{
  protected:
    explicit ListIface() {}

  public:
    ListIface(const ListIface &) = delete;
    ListIface &operator=(const ListIface &) = delete;

    virtual ~ListIface() {}

    virtual unsigned int get_number_of_items() const = 0;
    virtual bool empty() const = 0;
    virtual void clear() = 0;

    virtual const Item *get_item(unsigned int line) const = 0;
    virtual void set_parent_list(const ListIface *parent) = 0;
    virtual bool set_child_list(unsigned int line,
                                const std::shared_ptr<ListIface> &list) = 0;

    virtual const ListIface &up() const = 0;
    virtual const ListIface *down(unsigned int line) const = 0;
};

};

/*!@}*/

#endif /* !LIST_HH */
