#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <vector>

#include "ramlist.hh"

unsigned int List::RamList::get_number_of_items() const
{
    return items_.size();
}

const List::Item *List::RamList::get_item(unsigned int line)
{
    if(line >= get_number_of_items())
        return nullptr;
    else
        return &items_[line];
}

const List::ListIface *List::RamList::up() const
{
    return nullptr;
}

const List::ListIface *List::RamList::down(unsigned int line) const
{
    return nullptr;
}

unsigned int List::RamList::append(Item &&item)
{
    items_.push_back(std::move(item));
    return items_.size() - 1;
}
