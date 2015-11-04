/*
 * Copyright (C) 2015  T+A elektroakustik GmbH & Co. KG
 *
 * This file is part of DRCPD.
 *
 * DRCPD is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 3 as
 * published by the Free Software Foundation.
 *
 * DRCPD is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with DRCPD.  If not, see <http://www.gnu.org/licenses/>.
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "view_manager.hh"
#include "view_nop.hh"
#include "messages.h"
#include "os.h"

static ViewNop::View nop_view(nullptr);

ViewManager::ViewManager(DcpTransaction &dcpd):
    active_view_(&nop_view),
    last_browse_view_(nullptr),
    dcp_transaction_(dcpd),
    debug_stream_(nullptr),
    stream_info_(std::make_shared<StreamInfo>())
{}

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
    dcp_transaction_.set_output_stream(&os);
}

void ViewManager::set_debug_stream(std::ostream &os)
{
    debug_stream_ = &os;
}

static void abort_transaction_or_fail_hard(DcpTransaction &t)
{
    if(t.abort())
        return;

    BUG("Failed aborting DCPD transaction, aborting program.");
    os_abort();
}

void ViewManager::serialization_result(DcpTransaction::Result result)
{
    if(!dcp_transaction_.is_in_progress())
    {
        BUG("Received result from DCPD for idle transaction");
        return;
    }

    switch(result)
    {
      case DcpTransaction::OK:
        if(dcp_transaction_.done())
            return;

        BUG("Got OK from DCPD, but failed ending transaction");
        break;

      case DcpTransaction::FAILED:
        msg_error(EINVAL, LOG_CRIT, "DCPD failed to handle our transaction");
        break;

      case DcpTransaction::TIMEOUT:
        BUG("Got no answer from DCPD");
        break;

      case DcpTransaction::INVALID_ANSWER:
        BUG("Got invalid response from DCPD");
        break;

      case DcpTransaction::IO_ERROR:
        msg_error(EIO, LOG_CRIT,
                  "I/O error while trying to get response from DCPD");
        break;
    }

    abort_transaction_or_fail_hard(dcp_transaction_);
}

void ViewManager::handle_input_result(ViewIface::InputResult result,
                                      ViewIface &view)
{
    switch(result)
    {
      case ViewIface::InputResult::OK:
        break;

      case ViewIface::InputResult::UPDATE_NEEDED:
        view.update(dcp_transaction_, debug_stream_);
        break;

      case ViewIface::InputResult::SHOULD_HIDE:
        if(!view.is_browse_view_)
            activate_view(last_browse_view_);

        break;
    }
}

void ViewManager::input(DrcpCommand command)
{
    msg_info("Dispatching DRCP command %d", command);
    handle_input_result(active_view_->input(command), *active_view_);
}

void ViewManager::input_set_fast_wind_factor(double factor)
{
    msg_info("Need to handle FastWindSetFactor %f", factor);
}

static bool move_cursor_multiple_steps(int steps, DrcpCommand down_cmd,
                                       DrcpCommand up_cmd, ViewIface &view,
                                       DcpTransaction &dcpd,
                                       ViewIface::InputResult &result,
                                       std::ostream *debug_stream)
{
    if(steps == 0)
        return false;

    const DrcpCommand command = (steps > 0) ? down_cmd : up_cmd;

    if(steps < 0)
        steps = -steps;

    result = ViewIface::InputResult::OK;

    for(/* nothing */; steps > 0; --steps)
    {
        const ViewIface::InputResult temp = view.input(command);

        if(temp != ViewIface::InputResult::OK)
            result = temp;

        if(temp != ViewIface::InputResult::UPDATE_NEEDED)
           break;
    }

    return true;
}

void ViewManager::input_move_cursor_by_line(int lines)
{
    ViewIface::InputResult result;

    if(move_cursor_multiple_steps(lines, DrcpCommand::SCROLL_DOWN_ONE,
                                  DrcpCommand::SCROLL_UP_ONE,
                                  *active_view_, dcp_transaction_, result,
                                  debug_stream_))
        handle_input_result(result, *active_view_);
}

void ViewManager::input_move_cursor_by_page(int pages)
{
    ViewIface::InputResult result;

    if(move_cursor_multiple_steps(pages, DrcpCommand::SCROLL_PAGE_DOWN,
                                  DrcpCommand::SCROLL_PAGE_UP,
                                  *active_view_, dcp_transaction_, result,
                                  debug_stream_))
        handle_input_result(result, *active_view_);
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
    active_view_->serialize(dcp_transaction_, debug_stream_);

    if(view->is_browse_view_)
        last_browse_view_ = view;
}

ViewIface *ViewManager::get_view_by_name(const char *view_name)
{
    return lookup_view_by_name(all_views_, view_name);
}

ViewIface *ViewManager::get_playback_initiator_view() const
{
    return last_browse_view_;
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

bool ViewManager::is_active_view(const ViewIface *view) const
{
    return view == active_view_;
}

bool ViewManager::update_view_if_active(const ViewIface *view) const
{
    if(is_active_view(view))
        return active_view_->update(dcp_transaction_, debug_stream_);
    else
        return true;
}

bool ViewManager::serialize_view_if_active(const ViewIface *view) const
{
    if(is_active_view(view))
        return active_view_->serialize(dcp_transaction_, debug_stream_);
    else
        return true;
}

void ViewManager::hide_view_if_active(const ViewIface *view)
{
    if(is_active_view(view))
        handle_input_result(ViewIface::InputResult::SHOULD_HIDE, *active_view_);
}

std::shared_ptr<StreamInfo> ViewManager::get_stream_info()
{
    return stream_info_;
}
