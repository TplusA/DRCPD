/*
 * Copyright (C) 2015, 2016, 2017  T+A elektroakustik GmbH & Co. KG
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

static ViewNop::View nop_view;

ViewManager::Manager::Manager(UI::EventQueue &event_queue, DCP::Queue &dcp_queue,
                              ViewManager::Manager::ConfigMgr &config_manager):
    ui_events_(event_queue),
    config_manager_(config_manager),
    active_view_(&nop_view),
    last_browse_view_(nullptr),
    dcp_transaction_queue_(dcp_queue),
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

    config_manager_.set_updated_notification_callback(
        std::bind(&ViewManager::Manager::configuration_changed_notification,
                  this, std::placeholders::_1, std::placeholders::_2));

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
            activate_view(last_browse_view_, true);

        break;
    }
}

void ViewManager::Manager::store_event(UI::EventID event_id,
                                       std::unique_ptr<const UI::Parameters> parameters)
{
    std::unique_ptr<UI::Events::BaseEvent> ev;

    switch(UI::get_event_type_id(event_id))
    {
      case UI::EventTypeID::INPUT_EVENT:
        ev.reset(new UI::Events::ViewInput(event_id, std::move(parameters)));
        break;

      case UI::EventTypeID::BROADCAST_EVENT:
        ev.reset(new UI::Events::Broadcast(event_id, std::move(parameters)));
        break;

      case UI::EventTypeID::VIEW_MANAGER_EVENT:
        ev.reset(new UI::Events::ViewMan(event_id, std::move(parameters)));
        break;
    }

    ui_events_.post(std::move(ev));
}

static void log_event_dispatch(const UI::ViewEventID event_id,
                               const char *view_name, bool was_bounced)
{
    static constexpr std::array<const char *const,
                                static_cast<unsigned int>(UI::ViewEventID::LAST_VIEW_EVENT_ID) + 1>
        events
    {
        "NOP",
        "PLAYBACK_COMMAND_START",
        "PLAYBACK_COMMAND_STOP",
        "PLAYBACK_COMMAND_PAUSE",
        "PLAYBACK_PREVIOUS",
        "PLAYBACK_NEXT",
        "PLAYBACK_FAST_WIND_SET_SPEED",
        "PLAYBACK_SEEK_STREAM_POS",
        "PLAYBACK_MODE_REPEAT_TOGGLE",
        "PLAYBACK_MODE_SHUFFLE_TOGGLE",
        "NAV_SELECT_ITEM",
        "NAV_SCROLL_LINES",
        "NAV_SCROLL_PAGES",
        "NAV_GO_BACK_ONE_LEVEL",
        "SEARCH_COMMENCE",
        "SEARCH_STORE_PARAMETERS",
        "STORE_STREAM_META_DATA",
        "STORE_PRELOADED_META_DATA",
        "NOTIFY_AIRABLE_SERVICE_LOGIN_STATUS_UPDATE",
        "NOTIFY_NOW_PLAYING",
        "NOTIFY_STREAM_STOPPED",
        "NOTIFY_STREAM_PAUSED",
        "NOTIFY_STREAM_POSITION",
        "NOTIFY_SPEED_CHANGED",
    };

    static_assert(events[events.size() - 1] != nullptr, "Table too short");

    msg_vinfo(MESSAGE_LEVEL_DEBUG, "Dispatch %s (%d) to view %s (%s)",
              events[static_cast<unsigned int>(event_id)],
              static_cast<int>(event_id), view_name,
              was_bounced ? "bounced" : "direct");
}

static void log_event_dispatch(const UI::BroadcastEventID event_id,
                               const char *view_name)
{
    static constexpr std::array<const char *const,
                                static_cast<unsigned int>(UI::BroadcastEventID::LAST_EVENT_ID) + 1>
        events
    {
        "NOP",
        "CONFIGURATION_UPDATED",
    };

    static_assert(events[events.size() - 1] != nullptr, "Table too short");

    msg_vinfo(MESSAGE_LEVEL_DEBUG, "Dispatch broadcast %s (%d) to view %s",
              events[static_cast<unsigned int>(event_id)],
              static_cast<int>(event_id), view_name);
}

void ViewManager::Manager::dispatch_event(UI::ViewEventID event_id,
                                          std::unique_ptr<const UI::Parameters> parameters)
{
    static constexpr const InputBouncer::Item global_bounce_table_data[] =
    {
        InputBouncer::Item(UI::ViewEventID::PLAYBACK_COMMAND_STOP, ViewNames::PLAYER),
        InputBouncer::Item(UI::ViewEventID::PLAYBACK_COMMAND_PAUSE, ViewNames::PLAYER),
        InputBouncer::Item(UI::ViewEventID::PLAYBACK_PREVIOUS, ViewNames::PLAYER),
        InputBouncer::Item(UI::ViewEventID::PLAYBACK_NEXT, ViewNames::PLAYER),
        InputBouncer::Item(UI::ViewEventID::PLAYBACK_FAST_WIND_SET_SPEED, ViewNames::PLAYER),
        InputBouncer::Item(UI::ViewEventID::PLAYBACK_SEEK_STREAM_POS, ViewNames::PLAYER),
        InputBouncer::Item(UI::ViewEventID::PLAYBACK_MODE_REPEAT_TOGGLE, ViewNames::PLAYER),
        InputBouncer::Item(UI::ViewEventID::PLAYBACK_MODE_SHUFFLE_TOGGLE, ViewNames::PLAYER),
        InputBouncer::Item(UI::ViewEventID::STORE_STREAM_META_DATA, ViewNames::PLAYER),
        InputBouncer::Item(UI::ViewEventID::STORE_PRELOADED_META_DATA, ViewNames::PLAYER),
        InputBouncer::Item(UI::ViewEventID::NOTIFY_NOW_PLAYING, ViewNames::PLAYER),
        InputBouncer::Item(UI::ViewEventID::NOTIFY_STREAM_STOPPED, ViewNames::PLAYER),
        InputBouncer::Item(UI::ViewEventID::NOTIFY_STREAM_PAUSED, ViewNames::PLAYER),
        InputBouncer::Item(UI::ViewEventID::NOTIFY_STREAM_POSITION, ViewNames::PLAYER),
        InputBouncer::Item(UI::ViewEventID::SEARCH_STORE_PARAMETERS, ViewNames::SEARCH_OPTIONS),
        InputBouncer::Item(UI::ViewEventID::NOTIFY_AIRABLE_SERVICE_LOGIN_STATUS_UPDATE, ViewNames::BROWSER_INETRADIO),
    };

    static constexpr const ViewManager::InputBouncer global_bounce_table(global_bounce_table_data);

    if(!do_input_bounce(global_bounce_table, event_id, parameters))
    {
        log_event_dispatch(event_id, active_view_->name_, false);
        handle_input_result(active_view_->process_event(event_id,
                                                        std::move(parameters)),
                            *active_view_);
    }
}

void ViewManager::Manager::dispatch_event(UI::BroadcastEventID event_id,
                                          std::unique_ptr<const UI::Parameters> parameters)
{
    for(auto &view : all_views_)
    {
        log_event_dispatch(event_id, view.first.c_str());
        view.second->process_broadcast(event_id, parameters.get());
    }
}

void ViewManager::Manager::dispatch_event(UI::VManEventID event_id,
                                          std::unique_ptr<const UI::Parameters> parameters)
{
    switch(event_id)
    {
      case UI::VManEventID::NOP:
        break;

      case UI::VManEventID::OPEN_VIEW:
        {
            const auto params = UI::Events::downcast<UI::VManEventID::OPEN_VIEW>(parameters);

            if(params == nullptr)
                break;

            sync_activate_view_by_name(params->get_specific().c_str(), true);
        }

        break;

      case UI::VManEventID::TOGGLE_VIEWS:
        {
            const auto params = UI::Events::downcast<UI::VManEventID::TOGGLE_VIEWS>(parameters);

            if(params == nullptr)
                break;

            const auto &names(params->get_specific());
            sync_toggle_views_by_name(std::get<0>(names).c_str(),
                                      std::get<1>(names).c_str(), true);
        }

        break;

      case UI::VManEventID::INVALIDATE_LIST_ID:
        {
            const auto params =
                UI::Events::downcast<UI::VManEventID::INVALIDATE_LIST_ID>(parameters);

            if(params == nullptr)
                break;

            const auto &plist = params->get_specific();
            auto *const proxy = std::get<0>(plist);
            auto *const view =
                dynamic_cast<ViewFileBrowser::View *>(get_view_by_dbus_proxy(proxy));

            if(view == nullptr)
                BUG("Could not find view for D-Bus proxy");
            else if(view->list_invalidate(std::get<1>(plist), std::get<2>(plist)))
                update_view_if_active(view, DCP::Queue::Mode::FORCE_ASYNC);
        }

        break;

      case UI::VManEventID::NOTIFY_NOW_PLAYING:
        sync_activate_view_by_name(ViewNames::PLAYER, false);
        break;
    }
}

bool ViewManager::Manager::do_input_bounce(const ViewManager::InputBouncer &bouncer,
                                           UI::ViewEventID event_id,
                                           std::unique_ptr<const UI::Parameters> &parameters)
{
    const auto *item = bouncer.find(event_id);

    if(item == nullptr)
        return false;

    auto *const view = get_view_by_name(item->view_name_);

    if(view != nullptr)
    {
        log_event_dispatch(item->xform_event_id_, view->name_, true);
        handle_input_result(view->process_event(item->xform_event_id_,
                                                std::move(parameters)),
                            *view);
        return true;
    }

    BUG("Failed bouncing command %d, view \"%s\" unknown",
        static_cast<int>(event_id), item->view_name_);

    return false;
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

void ViewManager::Manager::activate_view(ViewIface *view,
                                         bool enforce_reactivation)
{
    if(view == nullptr)
        return;

    if(!enforce_reactivation && view == active_view_)
        return;

    active_view_->defocus();

    active_view_ = view;
    active_view_->focus();

    dynamic_cast<ViewSerializeBase *>(active_view_)->serialize(dcp_transaction_queue_,
                                                               DCP::Queue::Mode::SYNC_IF_POSSIBLE,
                                                               debug_stream_);

    if(active_view_->is_browse_view_)
        last_browse_view_ = active_view_;
}

ViewIface *ViewManager::Manager::get_view_by_name(const char *view_name)
{
    return lookup_view_by_name(all_views_, view_name);
}

ViewIface *ViewManager::Manager::get_view_by_dbus_proxy(const void *dbus_proxy)
{
    return lookup_view_by_dbus_proxy(all_views_, dbus_proxy);
}

void ViewManager::Manager::sync_activate_view_by_name(const char *view_name,
                                                      bool enforce_reactivation)
{
    msg_info("Requested to activate view \"%s\"", view_name);
    activate_view(lookup_view_by_name(all_views_, view_name),
                  enforce_reactivation);
}

void ViewManager::Manager::sync_toggle_views_by_name(const char *view_name_a,
                                                     const char *view_name_b,
                                                     bool enforce_reactivation)
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

    activate_view(next_view, enforce_reactivation);
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

void ViewManager::Manager::process_pending_events()
{
    while(true)
    {
        std::unique_ptr<UI::Events::BaseEvent> event = ui_events_.take();

        if(event == nullptr)
            return;

        if(auto *ev_vi = dynamic_cast<UI::Events::ViewInput *>(event.get()))
            dispatch_event(ev_vi->event_id_, std::move(ev_vi->parameters_));
        else if(auto *ev_bc = dynamic_cast<UI::Events::Broadcast *>(event.get()))
            dispatch_event(ev_bc->event_id_, std::move(ev_bc->parameters_));
        else if(auto *ev_vm = dynamic_cast<UI::Events::ViewMan *>(event.get()))
            dispatch_event(ev_vm->event_id_, std::move(ev_vm->parameters_));
        else
            BUG("Unhandled event");
    }
}

void ViewManager::Manager::busy_state_notification(bool is_busy)
{
    ViewSerializeBase *view = dynamic_cast<ViewSerializeBase *>(active_view_);
    log_assert(view != nullptr);

    view->add_base_update_flags(ViewSerializeBase::UPDATE_FLAGS_BASE_BUSY_FLAG);
    view->update(dcp_transaction_queue_, DCP::Queue::Mode::FORCE_ASYNC,
                 debug_stream_);
}

void ViewManager::Manager::configuration_changed_notification(const char *origin,
                                                              const std::array<bool, Configuration::DrcpdValues::NUMBER_OF_KEYS> &changed)
{
    auto params = UI::Events::mk_params<UI::EventID::CONFIGURATION_UPDATED>();
    auto &vec(params->get_specific_non_const());

    for(size_t i = 0; i < Configuration::DrcpdValues::NUMBER_OF_KEYS; ++i)
    {
        if(changed[i])
            vec.push_back(static_cast<Configuration::DrcpdValues::KeyID>(i));
    }

    store_event(UI::EventID::CONFIGURATION_UPDATED, std::move(params));
}
