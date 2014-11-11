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
    debug_stream_ = nullptr;
}

static inline bool is_view_name_valid(const std::string &view_name)
{
    return view_name[0] != '#' && view_name[0] != '\0';
}

bool ViewManager::add_view(ViewIface *view)
{
    if(view == nullptr)
        return false;

    if(!is_view_name_valid(view->name_))
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

void ViewManager::set_debug_stream(std::ostream &os)
{
    debug_stream_ = &os;
}

static void handle_input_result(ViewIface::InputResult result, ViewIface &view,
                                std::ostream &output_stream,
                                std::ostream *debug_stream)
{
    switch(result)
    {
      case ViewIface::InputResult::OK:
        break;

      case ViewIface::InputResult::UPDATE_NEEDED:
        view.update(output_stream, debug_stream);
        break;

      case ViewIface::InputResult::SHOULD_HIDE:
        view.defocus();
        break;
    }
}

void ViewManager::input(DrcpCommand command)
{
    msg_info("Dispatching DRCP command %d", command);
    handle_input_result(active_view_->input(command), *active_view_,
                        *output_stream_, debug_stream_);
}

void ViewManager::input_set_fast_wind_factor(double factor)
{
    msg_info("Need to handle FastWindSetFactor %f", factor);
}

static void move_cursor_multiple_steps(int steps, DrcpCommand down_cmd,
                                       DrcpCommand up_cmd, ViewIface &view,
                                       std::ostream &output_stream,
                                       std::ostream *debug_stream)
{
    if(steps == 0)
        return;

    const DrcpCommand command = (steps > 0) ? down_cmd : up_cmd;

    if(steps < 0)
        steps = -steps;

    ViewIface::InputResult result = ViewIface::InputResult::OK;

    for(/* nothing */; steps > 0; --steps)
    {
        const ViewIface::InputResult temp = view.input(command);

        if(temp != ViewIface::InputResult::OK)
            result = temp;

        if(temp != ViewIface::InputResult::UPDATE_NEEDED)
           break;
    }

    handle_input_result(result, view, output_stream, debug_stream);
}

void ViewManager::input_move_cursor_by_line(int lines)
{
    move_cursor_multiple_steps(lines, DrcpCommand::SCROLL_DOWN_ONE,
                               DrcpCommand::SCROLL_UP_ONE,
                               *active_view_, *output_stream_, debug_stream_);
}

void ViewManager::input_move_cursor_by_page(int pages)
{
    move_cursor_multiple_steps(pages, DrcpCommand::SCROLL_PAGE_DOWN,
                               DrcpCommand::SCROLL_PAGE_UP,
                               *active_view_, *output_stream_, debug_stream_);
}

static ViewIface *lookup_view_by_name(ViewManager::views_container_t &container,
                                      const char *view_name)
{
    if(!is_view_name_valid(view_name))
        return nullptr;

    auto it = container.find(view_name);

    return (it != container.end()) ? it->second : nullptr;
}

void ViewManager::activate_view(ViewIface *view)
{
    if(view == nullptr)
        return;

    if(view == active_view_)
        return;

    active_view_->defocus();

    active_view_ = view;
    active_view_->focus();
    active_view_->serialize(*output_stream_, debug_stream_);
}

void ViewManager::activate_view_by_name(const char *view_name)
{
    msg_info("Requested to activate view \"%s\"", view_name);
    activate_view(lookup_view_by_name(all_views_, view_name));
}

void ViewManager::toggle_views_by_name(const char *view_name_a,
                                       const char *view_name_b)
{
    msg_info("Requested to toggle between views \"%s\" and \"%s\"",
             view_name_a, view_name_b );

    ViewIface *view_a = lookup_view_by_name(all_views_, view_name_a);
    ViewIface *view_b = lookup_view_by_name(all_views_, view_name_b);

    ViewIface *next_view;

    if(view_a == view_b)
        next_view = view_a;
    else if(view_a == nullptr)
        next_view = view_b;
    else if(view_a == active_view_)
        next_view = view_b;
    else
        next_view = view_a;

    activate_view(next_view);
}
