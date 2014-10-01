#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "ramlist.hh"

unsigned int List::RamList::get_number_of_items() const
{
    return 0;
}

const List::Item *List::RamList::get_item(unsigned int line)
{
    return nullptr;
}

const List::ListIface *List::RamList::up() const
{
    return nullptr;
}

const List::ListIface *List::RamList::down(unsigned int line) const
{
    return nullptr;
}
