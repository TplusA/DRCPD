#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "view_manager.hh"
#include "messages.h"

void ViewManager::input(DrcpCommand command)
{
    msg_info("Need to handle DRCP command %d", command);
}

void ViewManager::input_set_fast_wind_factor(double factor)
{
    msg_info("Need to handle FastWindSetFactor %f", factor);
}

void ViewManager::activate_view_by_name(const char *view_name)
{
    msg_info("Requested to activate view \"%s\"", view_name);
}
