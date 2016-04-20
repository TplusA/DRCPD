/*
 * Copyright (C) 2015, 2016  T+A elektroakustik GmbH & Co. KG
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
#include "view_filebrowser.hh"
#include "view_nop.hh"
#include "ui_parameters_predefined.hh"
#include "messages.h"
#include "os.h"

static ViewNop::View nop_view;

ViewManager::Manager::Manager(DCP::Queue &queue):
    active_view_(&nop_view),
    last_browse_view_(nullptr),
    dcp_transaction_queue_(queue),
    debug_stream_(nullptr)
{}

static inline bool is_view_name_valid(const char *view_name)
{
    return view_name != nullptr && view_name[0] != '#' && view_name[0] != '\0';
}

bool ViewManager::Manager::add_view(ViewIface *view)
{
    if(view == nullptr)
        return false;

    if(dynamic_cast<ViewSerializeBase *>(view) == nullptr)
        return false;

    if(!is_view_name_valid(view->name_))
        return false;

    if(all_views_.find(view->name_) != all_views_.end())
        return false;

    all_views_.insert(ViewsContainer::value_type(view->name_, view));

    return true;
}

bool ViewManager::Manager::invoke_late_init_functions()
{
    bool ok = true;

    for(auto &v : all_views_)
    {
        if(!v.second->late_init())
            ok = false;
    }

    return ok;
}

void ViewManager::Manager::set_output_stream(std::ostream &os)
{
    dcp_transaction_queue_.set_output_stream(&os);
}

void ViewManager::Manager::set_debug_stream(std::ostream &os)
{
    debug_stream_ = &os;
}

void ViewManager::Manager::serialization_result(DCP::Transaction::Result result)
{
    if(dcp_transaction_queue_.finish_transaction(result))
    {
        /* just start the next transaction with not delay */
        (void)dcp_transaction_queue_.start_transaction(DCP::Queue::Mode::SYNC_IF_POSSIBLE);
        return;
    }

    switch(result)
    {
      case DCP::Transaction::OK:
        BUG("Got OK from DCPD, but failed ending transaction");
        break;

      case DCP::Transaction::FAILED:
        msg_error(EINVAL, LOG_CRIT, "DCPD failed to handle our transaction");
        break;

      case DCP::Transaction::TIMEOUT:
        BUG("Got no answer from DCPD");
        break;

      case DCP::Transaction::INVALID_ANSWER:
        BUG("Got invalid response from DCPD");
        break;

      case DCP::Transaction::IO_ERROR:
        msg_error(EIO, LOG_CRIT,
                  "I/O error while trying to get response from DCPD");
        break;
    }
}

void ViewManager::Manager::handle_input_result(ViewIface::InputResult result,
                                               ViewIface &view)
{
    switch(result)
    {
      case ViewIface::InputResult::OK:
        break;

      case ViewIface::InputResult::UPDATE_NEEDED:
        if(&view != active_view_)
            break;

        /* fall-through */

      case ViewIface::InputResult::FORCE_SERIALIZE:
        dynamic_cast<ViewSerializeBase &>(view).update(dcp_transaction_queue_,
                                                       DCP::Queue::Mode::SYNC_IF_POSSIBLE,
                                                       debug_stream_);
        break;

      case ViewIface::InputResult::SHOULD_HIDE:
        if(&view == active_view_ && !view.is_browse_view_)
            activate_view(last_browse_view_);

        break;
    }
}

void ViewManager::Manager::input(DrcpCommand command,
                                 std::unique_ptr<const UI::Parameters> parameters)
{
    msg_info("Dispatching DRCP command %d%s",
             static_cast<int>(command),
             (parameters != nullptr) ? " with parameters" : "");

    static constexpr const InputBouncer::Item global_bounce_table_data[] =
    {
        InputBouncer::Item(DrcpCommand::PLAYBACK_STOP, ViewNames::PLAYER),
        InputBouncer::Item(DrcpCommand::FAST_WIND_SET_SPEED, ViewNames::PLAYER),
        InputBouncer::Item(DrcpCommand::X_TA_SEARCH_PARAMETERS, ViewNames::SEARCH_OPTIONS),
        InputBouncer::Item(DrcpCommand::X_TA_SET_STREAM_INFO, ViewNames::PLAYER),
    };

    static constexpr const ViewManager::InputBouncer global_bounce_table(global_bounce_table_data);

    if(!do_input_bounce(global_bounce_table, command, parameters))
        handle_input_result(active_view_->input(command, std::move(parameters)),
                            *active_view_);
}

bool ViewManager::Manager::do_input_bounce(const ViewManager::InputBouncer &bouncer,
                                           DrcpCommand command,
                                           std::unique_ptr<const UI::Parameters> &parameters)
{
    const auto *item = bouncer.find(command);

    if(item == nullptr)
        return false;

    auto *const view = get_view_by_name(item->view_name_);

    if(view != nullptr)
    {
        handle_input_result(view->input(item->xform_command_, std::move(parameters)),
                            *view);
        return true;
    }

    BUG("Failed bouncing command %d, view \"%s\" unknown",
        static_cast<int>(command), item->view_name_);

    return false;
}

static ViewIface::InputResult move_cursor_multi(DrcpCommand command, int units,
                                                ViewIface &view)
{
    log_assert(command == DrcpCommand::SCROLL_DOWN_MANY ||
               command == DrcpCommand::SCROLL_UP_MANY);
    log_assert(units != 0);

    if(units < 0)
        units = -units;

    auto packaged_units =
        std::unique_ptr<UI::ParamsUpDownSteps>(new UI::ParamsUpDownSteps(units));

    return view.input(command, std::move(packaged_units));
}

static const DrcpCommand count_to_command(int count, DrcpCommand one_down_cmd,
                                          DrcpCommand one_up_cmd)
{
    return (count == 0
            ? DrcpCommand::UNDEFINED_COMMAND
            : (count > 0
               ? (count == 1
                  ? one_down_cmd
                  : DrcpCommand::SCROLL_DOWN_MANY
                 )
               : (count == -1
                  ? one_up_cmd
                  : DrcpCommand::SCROLL_UP_MANY)));
}

static ViewIface::InputResult move_cursor_generic(ViewIface &view,
                                                  int count, int multiplier,
                                                  DrcpCommand one_down_command,
                                                  DrcpCommand one_up_command)
{
    if(count == 0 || multiplier == 0)
        return ViewIface::InputResult::OK;

    const DrcpCommand command =
        count_to_command(count, one_down_command, one_up_command);

    if(count == -1 || count == 1)
        return view.input(command, nullptr);

    return move_cursor_multi(command, count * multiplier, view);
}

void ViewManager::Manager::input_move_cursor_by_line(int lines)
{
    const ViewIface::InputResult result =
        move_cursor_generic(*active_view_, lines, 1,
                            DrcpCommand::SCROLL_DOWN_ONE,
                            DrcpCommand::SCROLL_UP_ONE);

    handle_input_result(result, *active_view_);
}

void ViewManager::Manager::input_move_cursor_by_page(int pages)
{
    const ViewIface::InputResult result =
        move_cursor_generic(*active_view_, pages, NUMBER_OF_LINES_ON_DISPLAY,
                            DrcpCommand::SCROLL_PAGE_DOWN,
                            DrcpCommand::SCROLL_PAGE_UP);

    handle_input_result(result, *active_view_);
}

static ViewIface *lookup_view_by_name(ViewManager::Manager::ViewsContainer &container,
                                      const char *view_name)
{
    if(!is_view_name_valid(view_name))
        return nullptr;

    const auto &it = container.find(view_name);

    return (it != container.end()) ? it->second : nullptr;
}

static ViewIface *lookup_view_by_dbus_proxy(ViewManager::Manager::ViewsContainer &container,
                                            const void *dbus_proxy)
{
    if(dbus_proxy == nullptr)
        return nullptr;

    for(const auto &it : container)
    {
        auto *vfb = dynamic_cast<ViewFileBrowser::View *>(it.second);

        if(vfb != nullptr && vfb->owns_dbus_proxy(dbus_proxy))
            return vfb;
    }

    return nullptr;
}

void ViewManager::Manager::activate_view(ViewIface *view)
{
    if(view == nullptr)
        return;

    active_view_->defocus();

    active_view_ = view;
    active_view_->focus();

    dynamic_cast<ViewSerializeBase *>(active_view_)->serialize(dcp_transaction_queue_,
                                                               DCP::Queue::Mode::SYNC_IF_POSSIBLE,
                                                               debug_stream_);

    if(view->is_browse_view_)
        last_browse_view_ = view;
}

ViewIface *ViewManager::Manager::get_view_by_name(const char *view_name)
{
    return lookup_view_by_name(all_views_, view_name);
}

ViewIface *ViewManager::Manager::get_view_by_dbus_proxy(const void *dbus_proxy)
{
    return lookup_view_by_dbus_proxy(all_views_, dbus_proxy);
}

ViewIface *ViewManager::Manager::get_playback_initiator_view() const
{
    return last_browse_view_;
}

void ViewManager::Manager::activate_view_by_name(const char *view_name)
{
    msg_info("Requested to activate view \"%s\"", view_name);
    activate_view(lookup_view_by_name(all_views_, view_name));
}

void ViewManager::Manager::toggle_views_by_name(const char *view_name_a,
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

bool ViewManager::Manager::is_active_view(const ViewIface *view) const
{
    return view == active_view_;
}

void ViewManager::Manager::update_view_if_active(const ViewIface *view,
                                                 DCP::Queue::Mode mode) const
{
    if(is_active_view(view))
        dynamic_cast<ViewSerializeBase *>(active_view_)->update(
            dcp_transaction_queue_, mode, debug_stream_);
}

void ViewManager::Manager::serialize_view_if_active(const ViewIface *view,
                                                    DCP::Queue::Mode mode) const
{
    if(is_active_view(view))
        dynamic_cast<ViewSerializeBase *>(active_view_)->serialize(
            dcp_transaction_queue_, mode, debug_stream_);
}

void ViewManager::Manager::serialize_view_forced(const ViewIface *view,
                                                 DCP::Queue::Mode mode) const
{
    dynamic_cast<ViewSerializeBase *>(const_cast<ViewIface *>(view))->serialize(
        dcp_transaction_queue_, mode, debug_stream_);
}

void ViewManager::Manager::hide_view_if_active(const ViewIface *view)
{
    if(is_active_view(view))
        handle_input_result(ViewIface::InputResult::SHOULD_HIDE, *active_view_);
}

void ViewManager::Manager::busy_state_notification(bool is_busy)
{
    ViewSerializeBase *view = dynamic_cast<ViewSerializeBase *>(active_view_);
    log_assert(view != nullptr);

    view->add_base_update_flags(ViewSerializeBase::UPDATE_FLAGS_BASE_BUSY_FLAG);
    view->update(dcp_transaction_queue_, DCP::Queue::Mode::FORCE_ASYNC,
                 debug_stream_);
}
