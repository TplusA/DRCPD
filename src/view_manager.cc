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
#include "view_play.hh"
#include "player.hh"
#include "ui_parameters_predefined.hh"
#include "messages.h"
#include "os.h"

static ViewNop::View nop_view;

ViewManager::Manager::Manager(UI::EventQueue &event_queue, DCP::Queue &dcp_queue):
    ui_events_(event_queue),
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

void ViewManager::Manager::store_event(UI::EventID event_id,
                                       std::unique_ptr<const UI::Parameters> parameters)
{
    std::unique_ptr<UI::Events::BaseEvent> ev;

    switch(UI::get_event_type_id(event_id))
    {
      case UI::EventTypeID::INPUT_EVENT:
        ev.reset(new UI::Events::ViewInput(event_id, std::move(parameters)));
        break;

      case UI::EventTypeID::VIEW_MANAGER_EVENT:
        ev.reset(new UI::Events::ViewMan(event_id, std::move(parameters)));
        break;
    }

    ui_events_.post(std::move(ev));
}

void ViewManager::Manager::dispatch_event(UI::ViewEventID event_id,
                                          std::unique_ptr<const UI::Parameters> parameters)
{
    static constexpr const InputBouncer::Item global_bounce_table_data[] =
    {
        InputBouncer::Item(UI::ViewEventID::PLAYBACK_STOP, ViewNames::PLAYER),
        InputBouncer::Item(UI::ViewEventID::PLAYBACK_FAST_WIND_SET_SPEED, ViewNames::PLAYER),
        InputBouncer::Item(UI::ViewEventID::SEARCH_STORE_PARAMETERS, ViewNames::SEARCH_OPTIONS),
        InputBouncer::Item(UI::ViewEventID::STORE_PRELOADED_META_DATA, ViewNames::PLAYER),
    };

    static constexpr const ViewManager::InputBouncer global_bounce_table(global_bounce_table_data);

    if(!do_input_bounce(global_bounce_table, event_id, parameters))
        handle_input_result(active_view_->process_event(event_id,
                                                        std::move(parameters)),
                            *active_view_);
}

static void enhance_meta_data(PlayInfo::MetaData &md,
                              const std::string *fallback_title = NULL,
                              const std::string &url = NULL)
{
    if(fallback_title == NULL)
    {
        BUG("No fallback title available for stream");
        md.add("x-drcpd-title", NULL, ViewPlay::meta_data_reformatters);
    }
    else
        md.add("x-drcpd-title", fallback_title->c_str(), ViewPlay::meta_data_reformatters);

    if(url.empty())
    {
        BUG("No URL available for stream");
        md.add("x-drcpd-url", NULL, ViewPlay::meta_data_reformatters);
    }
    else
        md.add("x-drcpd-url", url.c_str(), ViewPlay::meta_data_reformatters);
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
            const auto params = UI::Events::downcast<UI::EventID::VIEW_OPEN>(parameters);

            if(params == nullptr)
                break;

            sync_activate_view_by_name(params->get_specific().c_str());
        }

        break;

      case UI::VManEventID::TOGGLE_VIEWS:
        {
            const auto params = UI::Events::downcast<UI::EventID::VIEW_TOGGLE>(parameters);

            if(params == nullptr)
                break;

            const auto &names(params->get_specific());
            sync_toggle_views_by_name(std::get<0>(names).c_str(),
                                      std::get<1>(names).c_str());
        }

        break;

      case UI::VManEventID::INVALIDATE_LIST_ID:
        {
            const auto params =
                UI::Events::downcast<UI::EventID::VIEW_INVALIDATE_LIST_ID>(parameters);

            if(params == nullptr)
                break;

            const auto &plist = params->get_specific();
            auto *const proxy = static_cast<GDBusProxy *>(std::get<0>(plist));
            auto *const view =
                dynamic_cast<ViewFileBrowser::View *>(get_view_by_dbus_proxy(proxy));

            if(view == nullptr)
                BUG("Could not find view for D-Bus proxy");
            else if(view->list_invalidate(std::get<1>(plist), std::get<2>(plist)))
                update_view_if_active(view, DCP::Queue::Mode::FORCE_ASYNC);
        }

        break;

      case UI::VManEventID::NOW_PLAYING:
        {
            const auto params =
                UI::Events::downcast<UI::EventID::VIEW_PLAYER_NOW_PLAYING>(parameters);

            if(params == nullptr)
                break;

            const auto &plist = params->get_specific();
            const ID::Stream stream_id(std::get<0>(plist));

            if(!stream_id.is_valid())
            {
                /* we are not sending such IDs */
                BUG("Invalid stream ID %u received from Streamplayer",
                    stream_id.get_raw_id());
                break;
            }

            const bool queue_is_full(std::get<1>(plist));
            auto &meta_data(const_cast<PlayInfo::MetaData &>(std::get<2>(plist)));
            const std::string &url_string(std::get<3>(plist));
            DBus::SignalData &data(*std::get<4>(plist));

            const bool have_preloaded_meta_data =
                data.player_.start_notification(stream_id, !queue_is_full);

            {
                const auto info = data.player_.get_stream_info__locked(stream_id);
                const StreamInfoItem *const &info_item = info.first;

                if(info_item == nullptr)
                {
                    msg_error(EINVAL, LOG_ERR,
                              "No fallback title found for stream ID %u",
                              stream_id.get_raw_id());
                    enhance_meta_data(meta_data, nullptr, url_string);
                }

                const PlayInfo::MetaData::CopyMode copy_mode =
                    (have_preloaded_meta_data || info_item != nullptr)
                    ? PlayInfo::MetaData::CopyMode::NON_EMPTY
                    : PlayInfo::MetaData::CopyMode::ALL;

                data.mdstore_.meta_data_put__unlocked(meta_data, copy_mode);
            }

            if(have_preloaded_meta_data)
                data.play_view_->notify_stream_meta_data_changed();

            data.play_view_->notify_stream_start();
            sync_activate_view_by_name(ViewNames::PLAYER);

            auto *view = get_playback_initiator_view();
            if(view != nullptr && view != data.play_view_)
                view->notify_stream_start();
        }

        break;

      case UI::VManEventID::META_DATA_UPDATE:
        {
            const auto params =
                UI::Events::downcast<UI::EventID::VIEW_PLAYER_META_DATA_UPDATE>(parameters);

            if(params == nullptr)
                break;

            const auto &plist = params->get_specific();
            const ID::Stream stream_id(std::get<0>(plist));

            if(!stream_id.is_valid())
            {
                /* we are not sending such IDs */
                BUG("Invalid stream ID %u received from Streamplayer",
                    stream_id.get_raw_id());
                break;
            }

            auto &meta_data(const_cast<PlayInfo::MetaData &>(std::get<1>(plist)));
            DBus::SignalData &data(*std::get<2>(plist));

            data.mdstore_.meta_data_put__locked(meta_data,
                                                PlayInfo::MetaData::CopyMode::NON_EMPTY);
            data.play_view_->notify_stream_meta_data_changed();
        }

        break;

      case UI::VManEventID::PLAYER_STOPPED:
        {
            const auto params =
                UI::Events::downcast<UI::EventID::VIEW_PLAYER_STOPPED>(parameters);
            const auto &plist = params->get_specific();

            DBus::SignalData &data(*std::get<1>(plist));

            data.player_.stop_notification();
            data.play_view_->notify_stream_stop();

            auto *view = get_playback_initiator_view();
            if(view != nullptr && view != data.play_view_)
                view->notify_stream_stop();
        }

        break;

      case UI::VManEventID::PLAYER_PAUSED:
        {
            const auto params =
                UI::Events::downcast<UI::EventID::VIEW_PLAYER_PAUSED>(parameters);
            const auto &plist = params->get_specific();

            DBus::SignalData &data(*std::get<1>(plist));

            data.player_.pause_notification();
            data.play_view_->notify_stream_pause();
        }

        break;

      case UI::VManEventID::PLAYER_POSITION_UPDATE:
        {
            const auto params =
                UI::Events::downcast<UI::EventID::VIEW_PLAYER_POSITION_UPDATE>(parameters);
            const auto &plist = params->get_specific();

            DBus::SignalData &data(*std::get<3>(plist));

            if(data.player_.track_times_notification(std::get<1>(plist),
                                                     std::get<2>(plist)))
                data.play_view_->notify_stream_position_changed();
        }

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

ViewIface *ViewManager::Manager::get_playback_initiator_view() const
{
    return last_browse_view_;
}

void ViewManager::Manager::sync_activate_view_by_name(const char *view_name)
{
    msg_info("Requested to activate view \"%s\"", view_name);
    activate_view(lookup_view_by_name(all_views_, view_name));
}

void ViewManager::Manager::sync_toggle_views_by_name(const char *view_name_a,
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

void ViewManager::Manager::process_pending_events()
{
    while(true)
    {
        std::unique_ptr<UI::Events::BaseEvent> event = ui_events_.take();

        if(event == nullptr)
            return;

        if(auto *ev_vi = dynamic_cast<UI::Events::ViewInput *>(event.get()))
        {
            std::unique_ptr<const UI::Parameters> parameters;
            parameters.swap(ev_vi->parameters_);

            dispatch_event(ev_vi->event_id_, std::move(parameters));
        }
        else if(auto *ev_vm = dynamic_cast<UI::Events::ViewMan *>(event.get()))
        {
            std::unique_ptr<const UI::Parameters> parameters;
            parameters.swap(ev_vm->parameters_);

            dispatch_event(ev_vm->event_id_, std::move(parameters));
        }
        else
        {
            BUG("Unhandled event");
        }
    }
}

void ViewManager::Manager::busy_state_notification(bool is_busy)
{
    ViewSerializeBase *view = dynamic_cast<ViewSerializeBase *>(active_view_);
    log_assert(view != nullptr);

    view->add_base_update_flags(ViewSerializeBase::UPDATE_FLAGS_BASE_BUSY_FLAG);

    /*
     * TODO: This locks up. Busy notification is broken at the moment.
     */
    /*
    view->update(dcp_transaction_queue_, DCP::Queue::Mode::FORCE_ASYNC,
                 debug_stream_);
    */
}
