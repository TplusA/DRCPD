#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "view_config.hh"
#include "i18n.h"

bool ViewConfig::View::init()
{
    return true;
}

void ViewConfig::View::focus()
{
}

void ViewConfig::View::defocus()
{
}

ViewIface::InputResult ViewConfig::View::input(DrcpCommand command)
{
    return InputResult::OK;
}

void ViewConfig::View::serialize(std::ostream &os)
{
    os << _("Device name") << std::endl;
}

void ViewConfig::View::update(std::ostream &os)
{
    os << "Update world!" << std::endl;
}
