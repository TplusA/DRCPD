#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "view_manager.hh"
#include "view_nop.hh"
#include "messages.h"

static ViewNop::View nop_view;

class NopOutputStream: public std::ostream {};

static NopOutputStream nop_ostream;

ViewManager::ViewManager()
{
    active_view_ = &nop_view;
    output_stream_ = &nop_ostream;
}

bool ViewManager::add_view(ViewIface *view)
{
    if(view == nullptr)
        return false;

    if(view->name_[0] == '#')
        return false;

    if(all_views_.find(view->name_) != all_views_.end())
        return false;

    all_views_.insert(views_container_t::value_type(view->name_, view));

    return true;
}

void ViewManager::set_output_stream(std::ostream &os)
{
    output_stream_ = &os;
}

void ViewManager::input(DrcpCommand command)
{
    msg_info("Need to handle DRCP command %d", command);
}

void ViewManager::input_set_fast_wind_factor(double factor)
{
    msg_info("Need to handle FastWindSetFactor %f", factor);
}

static ViewIface *lookup_view_by_name(ViewManager::views_container_t &container,
                                      const char *view_name)
{
    if(view_name[0] == '#')
        return nullptr;

    auto it = container.find(view_name);

    return (it != container.end()) ? it->second : nullptr;
}

void ViewManager::activate_view_by_name(const char *view_name)
{
    msg_info("Requested to activate view \"%s\"", view_name);

    ViewIface *view = lookup_view_by_name(all_views_, view_name);

    if(view == nullptr)
        return;

    if(view == active_view_)
        return;

    active_view_->defocus();

    active_view_ = view;
    active_view_->focus();
    active_view_->serialize(*output_stream_);
}

void ViewManager::toggle_views_by_name(const char *view_name_a,
                                       const char *view_name_b)
{
    msg_info("Requested to toggle between views \"%s\" and \"%s\"",
             view_name_a, view_name_b );
}
