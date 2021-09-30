/*
 * Copyright (C) 2015--2021  T+A elektroakustik GmbH & Co. KG
 *
 * This file is part of DRCPD.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301, USA.
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <glib.h>

#include "view_manager.hh"
#include "view_filebrowser.hh"
#include "view_nop.hh"
#include "ui_parameters_predefined.hh"
#include "messages.h"

#include <string>
#include <numeric>

static ViewNop::View nop_view;

const char ViewManager::Manager::RESUME_CONFIG_SECTION__AUDIO_SOURCES[] = "audio sources";

ViewManager::Manager::Manager(UI::EventQueue &event_queue, DCP::Queue &dcp_queue,
                              ViewManager::Manager::ConfigMgr &config_manager):
    ui_events_(event_queue),
    config_manager_(config_manager),
    resume_playback_config_filename_(nullptr),
    active_view_(&nop_view),
    return_to_view_(nullptr),
    dcp_transaction_queue_(dcp_queue),
    debug_stream_(nullptr)
{
    inifile_new(&resume_configuration_file_);
}

ViewManager::Manager::~Manager()
{
    inifile_free(&resume_configuration_file_);
}

static inline bool is_view_name_valid(const char *view_name)
{
    return view_name != nullptr && view_name[0] != '#' && view_name[0] != '\0';
}

bool ViewManager::Manager::add_view(ViewIface &view)
{
    if(dynamic_cast<ViewSerializeBase *>(&view) == nullptr)
        return false;

    if(!is_view_name_valid(view.name_))
        return false;

    if(all_views_.find(view.name_) != all_views_.end())
        return false;

    all_views_.insert(ViewsContainer::value_type(view.name_, &view));

    return true;
}

bool ViewManager::Manager::invoke_late_init_functions()
{
    const bool result =
        std::accumulate(all_views_.begin(), all_views_.end(), true,
            [] (bool ok, auto &v)
            {
                return v.second->late_init() && ok;
            });

    config_manager_.set_updated_notification_callback(
        [this] (const char *origin,
                const std::array<bool, Configuration::DrcpdValues::NUMBER_OF_KEYS> &changed)
        {
            ViewManager::Manager::configuration_changed_notification(origin, changed);
        });

    I18n::register_notifier(
        [this] (const char *language_identifier)
        {
            if(active_view_ != nullptr)
                dynamic_cast<ViewSerializeBase *>(active_view_)->serialize(
                    dcp_transaction_queue_, DCP::Queue::Mode::SYNC_IF_POSSIBLE,
                    debug_stream_);
        });

    return result;
}

void ViewManager::Manager::set_output_stream(std::ostream &os)
{
    dcp_transaction_queue_.set_output_stream(&os);
}

void ViewManager::Manager::set_debug_stream(std::ostream &os)
{
    debug_stream_ = &os;
}

void ViewManager::Manager::set_resume_playback_configuration_file(const char *filename)
{
    log_assert(filename != nullptr);
    log_assert(filename[0] != '\0');

    resume_playback_config_filename_ = filename;
    inifile_free(&resume_configuration_file_);
    inifile_parse_from_file(&resume_configuration_file_,
                            resume_playback_config_filename_);
}

void ViewManager::Manager::deselected_notification()
{
    inifile_free(&resume_configuration_file_);
    inifile_new(&resume_configuration_file_);

    struct ini_section *const section =
        inifile_new_section(&resume_configuration_file_,
                            RESUME_CONFIG_SECTION__AUDIO_SOURCES,
                            sizeof(RESUME_CONFIG_SECTION__AUDIO_SOURCES) - 1);

    if(section != nullptr)
    {
        for(const auto &view : all_views_)
        {
            const auto *v = dynamic_cast<const ViewWithAudioSourceBase *>(view.second);

            if(v == nullptr)
                continue;

            v->enumerate_audio_source_resume_urls(
                [&section]
                (const std::string &asrc_id, const std::string &url)
                {
                    inifile_section_store_value(section,
                                                asrc_id.c_str(), asrc_id.length(),
                                                url.c_str(), url.length());
                });
        }
    }

    /* we write the file also in case the section could not be created due to
     * an out-of-memory condition because we don't want to keep around
     * (sometimes *very*) outdated URLs */
    inifile_write_to_file(&resume_configuration_file_,
                          resume_playback_config_filename_);
}

void ViewManager::Manager::shutdown()
{
    deselected_notification();
}

static const char *do_get_resume_url_by_audio_source_id(const struct ini_section *const section,
                                                        const std::string &id)
{
    const struct ini_key_value_pair *kv = (section == nullptr || id.empty())
        ? nullptr
        : inifile_section_lookup_kv_pair(section, id.c_str(), id.length());

    if(kv == nullptr || kv->value == nullptr)
    {
        if(id.empty())
            BUG("Tried to resume playback for empty audio source ID");
        else
            msg_error(0, LOG_NOTICE,
                      "No resume data for audio source \"%s\" available",
                      id.c_str());

        return nullptr;
    }

    msg_vinfo(MESSAGE_LEVEL_NORMAL,
              "Resume URL for %s: %s", id.c_str(), kv->value);

    return kv->value;
}

const char *ViewManager::Manager::get_resume_url_by_audio_source_id(const std::string &id) const
{
    return do_get_resume_url_by_audio_source_id(
                inifile_find_section(&resume_configuration_file_,
                                     RESUME_CONFIG_SECTION__AUDIO_SOURCES,
                                     sizeof(RESUME_CONFIG_SECTION__AUDIO_SOURCES) - 1),
                id);
}

std::string ViewManager::Manager::move_resume_url_by_audio_source_id(const std::string &id)
{
    struct ini_section *const section =
        inifile_find_section(&resume_configuration_file_,
                             RESUME_CONFIG_SECTION__AUDIO_SOURCES,
                             sizeof(RESUME_CONFIG_SECTION__AUDIO_SOURCES) - 1);
    const char *temp = do_get_resume_url_by_audio_source_id(section, id);

    if(temp == nullptr)
        return "";

    std::string result(temp);

    inifile_section_remove_value(section, RESUME_CONFIG_SECTION__AUDIO_SOURCES,
                                 sizeof(RESUME_CONFIG_SECTION__AUDIO_SOURCES) - 1);

    return result;
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
        if(&view == active_view_)
            dynamic_cast<ViewSerializeBase &>(view).update(
                dcp_transaction_queue_, DCP::Queue::Mode::SYNC_IF_POSSIBLE,
                debug_stream_);

        break;

      case ViewIface::InputResult::FULL_SERIALIZE_NEEDED:
        if(&view != active_view_)
            break;

        /* fall-through */

      case ViewIface::InputResult::FORCE_SERIALIZE:
        dynamic_cast<ViewSerializeBase &>(view).serialize(
            dcp_transaction_queue_, DCP::Queue::Mode::SYNC_IF_POSSIBLE,
            debug_stream_);
        break;

      case ViewIface::InputResult::SHOULD_HIDE:
        if(&view == active_view_ &&
           view.flags_.is_any_set(ViewIface::Flags::CAN_HIDE))
            activate_view(return_to_view_, true);

        break;
    }
}

void ViewManager::Manager::notify_main_thread_if_necessary(
        UI::EventID event_id, const UI::Parameters *const parameters)
{
    switch(UI::to_event_type<UI::VManEventID>(event_id))
    {
      case UI::VManEventID::DATA_COOKIE_AVAILABLE:
        {
            const auto params =
                UI::Events::downcast<UI::VManEventID::DATA_COOKIE_AVAILABLE>(parameters);

            if(params == nullptr)
                break;

            const auto &plist = params->get_specific();
            auto *const proxy = std::get<0>(plist);
            auto *const view =
                dynamic_cast<ViewFileBrowser::View *>(get_view_by_dbus_proxy(proxy));
            if(view == nullptr)
                BUG("Could not find view for D-Bus proxy (data cookies available announcement)");
            else
                view->data_cookies_available_announcement(std::get<1>(plist));
        }

        break;

      case UI::VManEventID::DATA_COOKIE_ERROR:
        {
            const auto params =
                UI::Events::downcast<UI::VManEventID::DATA_COOKIE_ERROR>(parameters);

            if(params == nullptr)
                break;

            const auto &plist = params->get_specific();
            auto *const proxy = std::get<0>(plist);
            auto *const view =
                dynamic_cast<ViewFileBrowser::View *>(get_view_by_dbus_proxy(proxy));
            if(view == nullptr)
                BUG("Could not find view for D-Bus proxy (data cookies error announcement)");
            else
                view->data_cookies_error_announcement(std::get<1>(plist));
        }

        break;

      case UI::VManEventID::NOP:
      case UI::VManEventID::OPEN_VIEW:
      case UI::VManEventID::TOGGLE_VIEWS:
      case UI::VManEventID::CRAWLER_OPERATION_COMPLETED:
      case UI::VManEventID::CRAWLER_OPERATION_YIELDED:
      case UI::VManEventID::INVALIDATE_LIST_ID:
      case UI::VManEventID::NOTIFY_NOW_PLAYING:
        break;
    }
}

void ViewManager::Manager::store_event(UI::EventID event_id,
                                       std::unique_ptr<UI::Parameters> parameters)
{
    std::unique_ptr<UI::Events::BaseEvent> ev;

    switch(UI::get_event_type_id(event_id))
    {
      case UI::EventTypeID::INPUT_EVENT:
        ev = std::make_unique<UI::Events::ViewInput>(event_id, std::move(parameters));
        break;

      case UI::EventTypeID::BROADCAST_EVENT:
        ev = std::make_unique<UI::Events::Broadcast>(event_id, std::move(parameters));
        break;

      case UI::EventTypeID::VIEW_MANAGER_EVENT:
        notify_main_thread_if_necessary(event_id, parameters.get());
        ev = std::make_unique<UI::Events::ViewMan>(event_id, std::move(parameters));
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
        "NOTIFY_AIRABLE_SERVICE_LOGIN_STATUS_UPDATE",
        "NOTIFY_AIRABLE_SERVICE_OAUTH_REQUEST",
        "NOTIFY_NOW_PLAYING",
        "NOTIFY_STREAM_STOPPED",
        "NOTIFY_STREAM_PAUSED",
        "NOTIFY_STREAM_UNPAUSED",
        "NOTIFY_STREAM_POSITION",
        "NOTIFY_SPEED_CHANGED",
        "NOTIFY_PLAYBACK_MODE_CHANGED",
        "AUDIO_SOURCE_SELECTED",
        "AUDIO_SOURCE_DESELECTED",
        "AUDIO_PATH_HALF_CHANGED",
        "AUDIO_PATH_CHANGED",
        "STRBO_URL_RESOLVED",
        "SET_DISPLAY_CONTENT",
        "PLAYBACK_TRY_RESUME",
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
                                          std::unique_ptr<UI::Parameters> parameters)
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
        InputBouncer::Item(UI::ViewEventID::NOTIFY_NOW_PLAYING, ViewNames::PLAYER),
        InputBouncer::Item(UI::ViewEventID::NOTIFY_STREAM_STOPPED, ViewNames::PLAYER),
        InputBouncer::Item(UI::ViewEventID::NOTIFY_STREAM_PAUSED, ViewNames::PLAYER),
        InputBouncer::Item(UI::ViewEventID::NOTIFY_STREAM_UNPAUSED, ViewNames::PLAYER),
        InputBouncer::Item(UI::ViewEventID::NOTIFY_STREAM_POSITION, ViewNames::PLAYER),
        InputBouncer::Item(UI::ViewEventID::NOTIFY_PLAYBACK_MODE_CHANGED, ViewNames::PLAYER),
        InputBouncer::Item(UI::ViewEventID::AUDIO_SOURCE_SELECTED, ViewNames::PLAYER),
        InputBouncer::Item(UI::ViewEventID::AUDIO_SOURCE_DESELECTED, ViewNames::PLAYER),
        InputBouncer::Item(UI::ViewEventID::AUDIO_PATH_HALF_CHANGED, ViewNames::PLAYER),
        InputBouncer::Item(UI::ViewEventID::AUDIO_PATH_CHANGED, ViewNames::PLAYER),
        InputBouncer::Item(UI::ViewEventID::SEARCH_STORE_PARAMETERS, ViewNames::SEARCH_OPTIONS),
        InputBouncer::Item(UI::ViewEventID::NOTIFY_AIRABLE_SERVICE_LOGIN_STATUS_UPDATE, ViewNames::BROWSER_INETRADIO),
    };

    static constexpr const ViewManager::InputBouncer global_bounce_table(global_bounce_table_data);

    if(do_input_bounce(global_bounce_table, event_id, parameters))
        return;

    ViewIface *target_view;
    if(event_id != UI::ViewEventID::SET_DISPLAY_CONTENT)
        target_view = active_view_;
    else
    {
        const char *target_view_name = nullptr;

        const auto *params =
            UI::Events::downcast_plain<UI::ViewEventID::SET_DISPLAY_CONTENT>(parameters);
        if(params != nullptr)
        {
            const auto &plist = params->get_specific();
            target_view_name = std::get<0>(plist).c_str();
        }

        target_view = get_view_by_name(target_view_name);

        if(target_view == nullptr)
        {
            BUG("Cannot send display update to unknown view \"%s\"",
                target_view_name);
            return;
        }
    }

    log_event_dispatch(event_id, target_view->name_, false);
    handle_input_result(target_view->process_event(event_id,
                                                   std::move(parameters)),
                        *target_view);
}

void ViewManager::Manager::dispatch_event(UI::BroadcastEventID event_id,
                                          std::unique_ptr<UI::Parameters> parameters)
{
    for(auto &view : all_views_)
    {
        log_event_dispatch(event_id, view.first.c_str());
        view.second->process_broadcast(event_id, parameters.get());
    }
}

void ViewManager::Manager::dispatch_event(UI::VManEventID event_id,
                                          std::unique_ptr<UI::Parameters> parameters)
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

      case UI::VManEventID::DATA_COOKIE_AVAILABLE:
        {
            auto params =
                UI::Events::downcast<UI::VManEventID::DATA_COOKIE_AVAILABLE>(parameters);

            if(params == nullptr)
                break;

            auto &plist = params->get_specific_non_const();
            auto *const proxy = std::get<0>(plist);
            auto *const view =
                dynamic_cast<ViewFileBrowser::View *>(get_view_by_dbus_proxy(proxy));

            if(view == nullptr)
                BUG("Could not find view for D-Bus proxy (data cookies available)");
            else if(view->data_cookies_available(std::move(std::get<1>(plist))))
                update_view_if_active(view, DCP::Queue::Mode::FORCE_ASYNC);
        }

        break;

      case UI::VManEventID::DATA_COOKIE_ERROR:
        {
            auto params =
                UI::Events::downcast<UI::VManEventID::DATA_COOKIE_ERROR>(parameters);

            if(params == nullptr)
                break;

            auto &plist = params->get_specific_non_const();
            auto *const proxy = std::get<0>(plist);
            auto *const view =
                dynamic_cast<ViewFileBrowser::View *>(get_view_by_dbus_proxy(proxy));

            if(view == nullptr)
                BUG("Could not find view for D-Bus proxy (data cookies error)");
            else if(view->data_cookies_error(std::move(std::get<1>(plist))))
                update_view_if_active(view, DCP::Queue::Mode::FORCE_ASYNC);
        }

        break;

      case UI::VManEventID::CRAWLER_OPERATION_COMPLETED:
        {
            auto params =
                UI::Events::downcast<UI::VManEventID::CRAWLER_OPERATION_COMPLETED>(parameters);

            if(params == nullptr)
                break;

            auto &plist = params->get_specific_non_const();

            Playlist::Crawler::Iface::EventStoreFuns::completed(
                std::get<0>(plist), std::move(std::get<1>(plist)));
        }

        break;

      case UI::VManEventID::CRAWLER_OPERATION_YIELDED:
        {
            auto params =
                UI::Events::downcast<UI::VManEventID::CRAWLER_OPERATION_YIELDED>(parameters);

            if(params == nullptr)
                break;

            auto &plist = params->get_specific_non_const();

            Playlist::Crawler::Iface::EventStoreFuns::yielded(
                std::get<0>(plist), std::move(std::get<1>(plist)));
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
                BUG("Could not find view for D-Bus proxy (list invalidation)");
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
                                           std::unique_ptr<UI::Parameters> &parameters)
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

static ViewIface *lookup_view_by_dbus_proxy(const ViewManager::Manager::ViewsContainer &container,
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

    if(active_view_->flags_.is_any_set(ViewIface::Flags::CAN_RETURN_TO_THIS))
        return_to_view_ = active_view_;
}

ViewIface *ViewManager::Manager::get_view_by_name(const char *view_name)
{
    return lookup_view_by_name(all_views_, view_name);
}

const char *ViewManager::Manager::get_view_name_by_dbus_proxy(const void *dbus_proxy) const
{
    const auto *view = get_view_by_dbus_proxy(dbus_proxy);
    const auto *name = view != nullptr ? view->name_ : nullptr;
    return name != nullptr ? name : "*unknown*";
}

ViewIface *ViewManager::Manager::get_view_by_dbus_proxy(const void *dbus_proxy) const
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

LoggedLock::UniqueLock<LoggedLock::RecMutex>
ViewManager::Manager::block_async_result_notifications(const void *proxy)
{
    auto *const view =
        dynamic_cast<ViewFileBrowser::View *>(get_view_by_dbus_proxy(proxy));

    if(view != nullptr)
        return view->data_cookies_block_notifications();

    BUG("No file browser view for given proxy, cannot block cookie notifications");
    throw std::runtime_error("No file browser found for given D-Bus proxy");
}

bool ViewManager::Manager::set_pending_cookie(
        const void *proxy, uint32_t cookie,
        DBusRNF::CookieManagerIface::NotifyByCookieFn &&notify,
        DBusRNF::CookieManagerIface::FetchByCookieFn &&fetch)
{
    if(cookie == 0)
    {
        BUG("Attempted to store invalid cookie");
        return false;
    }

    if(notify == nullptr)
    {
        BUG("Notify function for cookie not given");
        return false;
    }

    if(fetch == nullptr)
    {
        BUG("Fetch function for cookie not given");
        return false;
    }

    auto *const view =
        dynamic_cast<ViewFileBrowser::View *>(get_view_by_dbus_proxy(proxy));

    if(view == nullptr)
    {
        BUG("No file browser view for given proxy, cannot set cookie %u", cookie);
        return false;
    }

    return view->data_cookie_set_pending(cookie, std::move(notify), std::move(fetch));
}

bool ViewManager::Manager::abort_cookie(const void *proxy, uint32_t cookie)
{
    if(cookie == 0)
    {
        BUG("Attempted to drop invalid cookie");
        return false;
    }

    auto *const view =
        dynamic_cast<ViewFileBrowser::View *>(get_view_by_dbus_proxy(proxy));

    if(view == nullptr)
    {
        BUG("No file browser view for given proxy, cannot drop cookie %u", cookie);
        return false;
    }

    return view->data_cookie_abort(cookie);
}

void ViewManager::Manager::configuration_changed_notification(
        const char *origin,
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
