#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <vector>

#include "ramlist.hh"

unsigned int List::RamList::get_number_of_items() const
{
    return items_.size();
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

    Item *item = get_nonconst_item(line);

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
    const List::Item *item = get_item(line);

    if(item == nullptr)
        return nullptr;

    return item->down();
}

unsigned int List::RamList::append(Item *item)
{
    items_.push_back(item);
    return items_.size() - 1;
}
