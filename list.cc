#ifndef LIST_CC
#define LIST_CC

#include "list.hh"
#include "i18n.h"

const char *List::Item::get_text() const
{
    return (!text_is_translatable_) ? text_.c_str() : _(text_.c_str());
}

#endif /* !LIST_CC */
