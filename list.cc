#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "list.hh"
#include "i18n.h"

const char *List::TextItem::get_text() const
{
    return (!text_is_translatable_) ? text_.c_str() : _(text_.c_str());
}
