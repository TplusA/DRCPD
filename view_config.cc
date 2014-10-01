#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "view.hh"

void ViewConfig::input(DrcpCommand command)
{
}

void ViewConfig::serialize(std::ostream &os)
{
    os << "Hello world!" << std::endl;
}

void ViewConfig::update(std::ostream &os)
{
    os << "Update world!" << std::endl;
}
