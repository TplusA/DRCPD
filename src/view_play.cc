/*
 * Copyright (C) 2015--2020  T+A elektroakustik GmbH & Co. KG
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

#include "view_play.hh"
#include "view_external_source_base.hh"
#include "view_manager.hh"
#include "audiosource.hh"
#include "ui_parameters_predefined.hh"
#include "dbus_iface_proxies.hh"
#include "gerrorwrapper.hh"
#include "xmlescape.hh"
#include "messages.h"

bool ViewPlay::View::init()
{
    return true;
}

void ViewPlay::View::focus()
{
    is_visible_ = true;
}

void ViewPlay::View::defocus()
{
    is_visible_ = false;
}

void ViewPlay::View::register_audio_source(Player::AudioSource &audio_source,
                                           const ViewIface &associated_view)
{
    audio_sources_with_view_.emplace(std::move(std::string(audio_source.id_)),
                                     std::move(std::make_pair(&audio_source, &associated_view)));
}

void ViewPlay::View::plug_audio_source(Player::AudioSource &audio_source,
                                       bool with_enforced_intentions,
                                       const std::string *external_player_id)
{
    player_control_.plug(audio_source, with_enforced_intentions,
                         [this] { player_finished_playing(); },
                         external_player_id);
}

void ViewPlay::View::prepare_for_playing(
        Player::AudioSource &audio_source,
        const std::function<Playlist::Crawler::Handle()> &get_crawler_handle,
        std::shared_ptr<Playlist::Crawler::FindNextOpBase> find_op,
        const Player::LocalPermissionsIface &permissions)
{
    const auto lock_ctrl(player_control_.lock());
    const auto lock_data(player_data_.lock());

    if(player_control_.is_active_controller_for_audio_source(audio_source))
    {
        /* we already own the player, so we can do a "soft" jump to the newly
         * selected stream to avoid going through stopped/playing signals and
         * associated queue management (which would fail without extra
         * precautions) */
    }
    else
    {
        /* we do not own the player yet, so stop the player just in case it is
         * playing, then plug to it */
        player_control_.stop_request("take control over player");
        player_control_.unplug(true);
        plug_audio_source(audio_source, true);
        player_control_.plug(player_data_);
    }

    player_control_.plug(get_crawler_handle, permissions);
    player_control_.play_request(std::move(find_op));
}

void ViewPlay::View::stop_playing(const Player::AudioSource &audio_source)
{
    const auto lock_ctrl(player_control_.lock());

    if(player_control_.is_active_controller_for_audio_source(audio_source))
        player_control_.unplug(false);
}

void ViewPlay::View::player_finished_playing()
{
    player_control_.unplug(false);
    player_data_.player_finished_and_idle();

    add_update_flags(UPDATE_FLAGS_PLAYBACK_STATE);
    view_manager_->update_view_if_active(this, DCP::Queue::Mode::FORCE_ASYNC);
    view_manager_->hide_view_if_active(this);
}

void ViewPlay::View::append_referenced_lists(const Player::AudioSource &audio_source,
                                             std::vector<ID::List> &list_ids) const
{
    const auto lock_ctrl(player_control_.lock());
    const auto lock_data(player_data_.lock());

    if(player_control_.is_active_controller_for_audio_source(audio_source))
        player_data_.append_referenced_lists(list_ids);
}

static void send_current_stream_info_to_dcpd(const Player::Data &player_data)
{
    const auto &md(player_data.get_current_meta_data());
    const auto stream_id(player_data.get_now_playing().get_stream_id());

    if(Player::AppStreamID::compatible_with(stream_id))
    {
        /* streams started by the app are managed by dcpd itself, it won't need
         * our data (in fact, it would emit a bug message if we would)  */
        return;
    }

    GErrorWrapper error;

    tdbus_dcpd_playback_call_set_stream_info_sync(
            DBus::get_dcpd_playback_iface(), stream_id.get_raw_id(),
            md.values_[MetaData::Set::ID::INTERNAL_DRCPD_TITLE].c_str(),
            md.values_[MetaData::Set::ID::INTERNAL_DRCPD_URL].c_str(),
            NULL, error.await());

    if(error.log_failure("Set stream info"))
        msg_error(0, LOG_NOTICE, "Failed sending stream information to dcpd");
}

static void lookup_source_and_view(ViewPlay::View::AudioSourceAndViewByID &audio_sources,
                                   const std::string &audio_source_id,
                                   Player::AudioSource *&audio_source,
                                   const ViewIface *&view)
{
    try
    {
        const auto ausrc_and_view(audio_sources.at(audio_source_id));

        audio_source = ausrc_and_view.first;
        view = ausrc_and_view.second;

        if(view == nullptr)
            BUG("Have no view for audio source %s", audio_source_id.c_str());
    }
    catch(const std::out_of_range &e)
    {
        BUG("Audio source %s not known", audio_source_id.c_str());
    }
}

static void lookup_view_for_external_source(ViewPlay::View::AudioSourceAndViewByID &audio_sources,
                                            const std::string &audio_source_id,
                                            Player::AudioSource *&audio_source,
                                            const ViewExternalSource::Base *&view)
{
    try
    {
        const auto ausrc_and_perm(audio_sources.at(audio_source_id));

        view = dynamic_cast<const ViewExternalSource::Base *>(ausrc_and_perm.second);

        if(view != nullptr)
            audio_source = ausrc_and_perm.first;
    }
    catch(const std::out_of_range &e)
    {
        BUG("View for external audio source %s not known", audio_source_id.c_str());
    }
}

static const ViewIface *
lookup_view_by_audio_source(ViewPlay::View::AudioSourceAndViewByID &audio_sources,
                            const Player::AudioSource *src)
{
    if(src == nullptr)
        return nullptr;

    Player::AudioSource *dummy;
    const ViewIface *result;

    lookup_source_and_view(audio_sources, src->id_, dummy, result);

    return result;
}

ViewIface::InputResult
ViewPlay::View::process_event(UI::ViewEventID event_id,
                              std::unique_ptr<UI::Parameters> parameters)
{
    const auto lock_ctrl(player_control_.lock());
    const auto lock_data(player_data_.lock());

    switch(event_id)
    {
      case UI::ViewEventID::NOP:
        break;

      case UI::ViewEventID::PLAYBACK_COMMAND_START:
        switch(player_data_.get_player_state())
        {
          case Player::PlayerState::BUFFERING:
          case Player::PlayerState::PLAYING:
            player_control_.pause_request();
            break;

          case Player::PlayerState::STOPPED:
          case Player::PlayerState::PAUSED:
            player_control_.play_request(nullptr);
            break;
        }

        break;

      case UI::ViewEventID::PLAYBACK_COMMAND_STOP:
        player_control_.stop_request("stop command issued by user");
        break;

      case UI::ViewEventID::PLAYBACK_COMMAND_PAUSE:
        player_control_.pause_request();
        break;

      case UI::ViewEventID::PLAYBACK_PREVIOUS:
        {
            static constexpr const auto rewind_threshold(std::chrono::milliseconds(5000));
            const auto times(player_data_.get_now_playing().get_times());

            if(times.first <= rewind_threshold)
                player_control_.skip_backward_request();
            else
            {
                switch(player_data_.get_intention())
                {
                  case Player::UserIntention::NOTHING:
                  case Player::UserIntention::STOPPING:
                  case Player::UserIntention::SKIPPING_PAUSED:
                  case Player::UserIntention::SKIPPING_LIVE:
                    break;

                  case Player::UserIntention::PAUSING:
                  case Player::UserIntention::LISTENING:
                    player_control_.rewind_request();
                    break;
                }
            }
        }

        break;

      case UI::ViewEventID::PLAYBACK_NEXT:
        player_control_.skip_forward_request();
        break;

      case UI::ViewEventID::NAV_SELECT_ITEM:
        break;

      case UI::ViewEventID::NAV_GO_BACK_ONE_LEVEL:
      case UI::ViewEventID::NAV_SCROLL_LINES:
      case UI::ViewEventID::NAV_SCROLL_PAGES:
        return is_navigation_locked_ ? InputResult::OK : InputResult::SHOULD_HIDE;

      case UI::ViewEventID::PLAYBACK_FAST_WIND_SET_SPEED:
        {
            const auto speed =
                UI::Events::downcast<UI::ViewEventID::PLAYBACK_FAST_WIND_SET_SPEED>(parameters);

            if(speed == nullptr)
                break;

            player_control_.fast_wind_set_speed_request(speed->get_specific());
        }

        break;

      case UI::ViewEventID::PLAYBACK_SEEK_STREAM_POS:
        {
            const auto params =
                UI::Events::downcast<UI::ViewEventID::PLAYBACK_SEEK_STREAM_POS>(parameters);

            if(params == nullptr)
                break;

            const auto &plist = params->get_specific();

            player_control_.seek_stream_request(std::get<0>(plist),
                                                std::get<1>(plist));
        }

        break;

      case UI::ViewEventID::NOTIFY_NOW_PLAYING:
        {
            const auto params =
                UI::Events::downcast<UI::ViewEventID::NOTIFY_NOW_PLAYING>(parameters);

            if(params == nullptr)
                break;

            auto &plist = params->get_specific_non_const();
            const ID::Stream stream_id(std::get<0>(plist));

            if(!stream_id.is_valid())
            {
                /* we are not sending such IDs */
                BUG("Invalid stream ID %u received from Streamplayer",
                    stream_id.get_raw_id());
                break;
            }

            const bool queue_is_full(std::get<2>(plist));
            const auto &dropped_ids(std::get<3>(plist));
            const bool switched_stream(!player_data_.get_now_playing().is_stream(stream_id));
            auto &meta_data(std::get<4>(plist));
            std::string &url_string(std::get<5>(plist));

            player_data_.player_dropped_from_queue(dropped_ids);

            if(switched_stream)
                player_data_.player_now_playing_stream(stream_id);

            player_data_.merge_meta_data(stream_id, std::move(meta_data),
                                         std::move(url_string));
            player_control_.play_notification(stream_id, switched_stream);

            msg_info("Play view: stream %s, %s",
                     switched_stream ? "started" : "updated",
                     is_visible_ ? "send screen update" : "but view is invisible");

            if(switched_stream)
                view_manager_->serialize_view_if_active(this, DCP::Queue::Mode::FORCE_ASYNC);
            else
            {
                add_update_flags(UPDATE_FLAGS_PLAYBACK_STATE);
                view_manager_->update_view_if_active(this, DCP::Queue::Mode::FORCE_ASYNC);
            }

            if(!queue_is_full && switched_stream)
            {
                msg_info("Trigger prefetching next item because queue isn't full");
                player_control_.start_prefetch_next_item(
                        "triggered by play notification",
                        Playlist::Crawler::Bookmark::PREFETCH_CURSOR,
                        Playlist::Crawler::Direction::FORWARD);
            }

            if(!switched_stream)
                send_current_stream_info_to_dcpd(player_data_);
        }

        break;

      case UI::ViewEventID::NOTIFY_STREAM_STOPPED:
        {
            const auto params =
                UI::Events::downcast<UI::ViewEventID::NOTIFY_STREAM_STOPPED>(parameters);

            if(params == nullptr)
                break;

            const auto &plist = params->get_specific();
            const ID::Stream stream_id(std::get<0>(plist));
            const auto &dropped_ids(std::get<2>(plist));
            const std::string &error_id(std::get<3>(plist));

            player_data_.player_dropped_from_queue(dropped_ids);
            player_data_.player_has_stopped();

            const auto stop_reaction = error_id.empty()
                ? player_control_.stop_notification_ok(stream_id)
                : player_control_.stop_notification_with_error(stream_id, error_id,
                                                               std::get<1>(plist));

            switch(stop_reaction)
            {
              case Player::Control::StopReaction::NOT_ATTACHED:
              case Player::Control::StopReaction::STREAM_IGNORED:
              case Player::Control::StopReaction::STOPPED:
                msg_info("Play view: stream stopped%s, %s",
                         error_id.empty() ? "" : " with error",
                         is_visible_ ? "send screen update" : "but view is invisible");

                player_finished_playing();

                break;

              case Player::Control::StopReaction::QUEUED:
              case Player::Control::StopReaction::RETRIEVE_QUEUED:
              case Player::Control::StopReaction::NOP:
              case Player::Control::StopReaction::REPLAY_QUEUE:
              case Player::Control::StopReaction::TAKE_NEXT:
                msg_info("Play view: stream stopped%s, but player keeps going",
                         error_id.empty() ? "" : " with error");
                break;

              case Player::Control::StopReaction::RETRY:
                msg_info("Play view: stream stopped with error; retrying");
                break;
            }
        }

        break;

      case UI::ViewEventID::NOTIFY_STREAM_PAUSED:
        {
            const auto stream_id =
                UI::Events::downcast<UI::ViewEventID::NOTIFY_STREAM_PAUSED>(parameters);

            if(stream_id == nullptr)
                break;

            player_data_.player_has_paused();
            player_control_.pause_notification(stream_id->get_specific());

            msg_info("Play view: stream paused, %s",
                     is_visible_ ? "send screen update" : "but view is invisible");

            add_update_flags(UPDATE_FLAGS_PLAYBACK_STATE);
            view_manager_->update_view_if_active(this, DCP::Queue::Mode::FORCE_ASYNC);
        }

        break;

      case UI::ViewEventID::NOTIFY_STREAM_POSITION:
        {
            const auto params =
                UI::Events::downcast<UI::ViewEventID::NOTIFY_STREAM_POSITION>(parameters);

            if(params == nullptr)
                break;

            const auto &plist = params->get_specific();

            if(player_data_.update_track_times(std::get<0>(plist),
                                               std::get<1>(plist),
                                               std::get<2>(plist)))
            {
                add_update_flags(UPDATE_FLAGS_STREAM_POSITION);
                view_manager_->update_view_if_active(this, DCP::Queue::Mode::FORCE_ASYNC);
            }
        }

        break;

      case UI::ViewEventID::NOTIFY_SPEED_CHANGED:
        {
            const auto params =
                UI::Events::downcast<UI::ViewEventID::NOTIFY_SPEED_CHANGED>(parameters);

            if(params == nullptr)
                break;

            const auto &plist = params->get_specific();

            if(player_data_.update_playback_speed(std::get<0>(plist),
                                                  std::get<1>(plist)))
            {
                add_update_flags(UPDATE_FLAGS_PLAYBACK_STATE);
                view_manager_->update_view_if_active(this, DCP::Queue::Mode::FORCE_ASYNC);
            }
        }

        break;

      case UI::ViewEventID::NOTIFY_PLAYBACK_MODE_CHANGED:
        {
            const auto params =
                UI::Events::downcast<UI::ViewEventID::NOTIFY_PLAYBACK_MODE_CHANGED>(parameters);

            if(params == nullptr)
                break;

            const auto &plist = params->get_specific();

            if(player_data_.set_reported_playback_state(std::get<0>(plist),
                                                        std::get<1>(plist)))
            {
                add_update_flags(UPDATE_FLAGS_PLAYBACK_MODES);
                view_manager_->update_view_if_active(this, DCP::Queue::Mode::FORCE_ASYNC);
            }
        }

        break;

      case UI::ViewEventID::STORE_STREAM_META_DATA:
        {
            const auto params =
                UI::Events::downcast<UI::ViewEventID::STORE_STREAM_META_DATA>(parameters);

            if(params == nullptr)
                break;

            auto &plist = params->get_specific_non_const();
            const ID::Stream stream_id(std::get<0>(plist));
            if(!stream_id.is_valid())
            {
                /* we are not sending such IDs */
                BUG("Invalid stream ID %u received from Streamplayer",
                    stream_id.get_raw_id());
                break;
            }

            if(player_data_.merge_meta_data(stream_id, std::move(std::get<1>(plist))) &&
               player_data_.get_now_playing().is_stream(stream_id))
            {
                add_update_flags(UPDATE_FLAGS_META_DATA);
                view_manager_->update_view_if_active(this, DCP::Queue::Mode::FORCE_ASYNC);
            }

            if(player_data_.get_now_playing().is_stream(stream_id))
                send_current_stream_info_to_dcpd(player_data_);
        }

        break;

      case UI::ViewEventID::STORE_PRELOADED_META_DATA:
        {
            const auto params =
                UI::Events::downcast<UI::ViewEventID::STORE_PRELOADED_META_DATA>(parameters);

            if(params == nullptr)
                break;

            auto &plist = params->get_specific_non_const();
            const auto stream_id(Player::AppStreamID::make_from_generic_id(std::get<0>(plist)));

            player_data_.announce_app_stream(stream_id, std::move(std::get<1>(plist)));

            if(player_data_.get_now_playing().is_stream(stream_id.get()))
                send_current_stream_info_to_dcpd(player_data_);
        }

        break;

      case UI::ViewEventID::AUDIO_SOURCE_SELECTED:
        {
            /* one of our own audio sources has been selected */
            const auto params =
                UI::Events::downcast<UI::ViewEventID::AUDIO_SOURCE_SELECTED>(parameters);

            if(params == nullptr)
                break;

            const auto &plist = params->get_specific();
            const std::string &ausrc_id(std::get<0>(plist));
            const bool is_on_hold(std::get<1>(plist));

            if(is_on_hold)
                Busy::set(Busy::DirectSource::WAITING_FOR_APPLIANCE_AUDIO);
            else
                Busy::clear(Busy::DirectSource::WAITING_FOR_APPLIANCE_AUDIO);

            if(player_control_.is_any_audio_source_plugged())
            {
                if(player_control_.source_selected_notification(ausrc_id, is_on_hold))
                    break;

                /* source has been changed, need to switch views */
                player_control_.unplug(true);
            }
            else
                msg_info("Fresh activation, selection of audio source %s",
                         ausrc_id.c_str());

            Player::AudioSource *audio_source = nullptr;
            const ViewIface *view = nullptr;
            lookup_source_and_view(audio_sources_with_view_, ausrc_id,
                                   audio_source, view);

            if(audio_source != nullptr)
            {
                msg_info("Plug selected audio source %s into player",
                         audio_source->id_.c_str());

                audio_source->select_now();
                plug_audio_source(*audio_source, true);
                player_control_.plug(player_data_);
                player_control_.source_selected_notification(ausrc_id, is_on_hold);
            }

            if(view != nullptr)
                view_manager_->sync_activate_view_by_name(view->name_, true);
        }

        break;

      case UI::ViewEventID::AUDIO_SOURCE_DESELECTED:
        {
            /* one of our own audio sources has been deselected */
            const auto params =
                UI::Events::downcast<UI::ViewEventID::AUDIO_SOURCE_DESELECTED>(parameters);

            if(params == nullptr)
                break;

            Busy::clear(Busy::DirectSource::WAITING_FOR_APPLIANCE_AUDIO);

            const auto &plist = params->get_specific();
            const std::string &ausrc_id(std::get<0>(plist));

            if(!player_control_.source_deselected_notification(&ausrc_id))
            {
                Player::AudioSource *audio_source = nullptr;
                const ViewIface *view = nullptr;
                lookup_source_and_view(audio_sources_with_view_, ausrc_id,
                                       audio_source, view);

                if(audio_source == nullptr)
                    msg_info("Dropped deselect notification for unknown audio source %s",
                             ausrc_id.c_str());
                else
                {
                    audio_source->deselected_notification();
                    msg_info("Deselected unplugged audio source %s",
                             audio_source->id_.c_str());
                }
            }
        }

        break;

      case UI::ViewEventID::AUDIO_PATH_CHANGED:
        {
            /* some (any!) audio path has been activated */
            const auto params =
                UI::Events::downcast<UI::ViewEventID::AUDIO_PATH_CHANGED>(parameters);

            if(params == nullptr)
                break;

            const auto &plist = params->get_specific();
            const std::string &ausrc_id(std::get<0>(plist));
            const std::string &player_id(std::get<1>(plist));
            const bool is_on_hold(std::get<2>(plist));
            Player::AudioSource *audio_source = nullptr;
            const ViewExternalSource::Base *view = nullptr;

            if(is_on_hold)
                Busy::set(Busy::DirectSource::WAITING_FOR_APPLIANCE_AUDIO);
            else
                Busy::clear(Busy::DirectSource::WAITING_FOR_APPLIANCE_AUDIO);

            if(!ausrc_id.empty())
                lookup_view_for_external_source(audio_sources_with_view_,
                                                ausrc_id, audio_source, view);

            is_navigation_locked_ = view != nullptr
                ? view->flags_.is_any_set(ViewIface::Flags::IS_PASSIVE)
                : false;

            if(player_control_.is_active_controller_for_audio_source(ausrc_id))
                break;

            /* this must be an audio source not owned by us (or empty string),
             * otherwise we would already be controlling it */
            const bool audio_source_is_deselected =
                audio_source == nullptr || ausrc_id.empty();

            const auto *const view_for_deselected_audio_source =
                lookup_view_by_audio_source(audio_sources_with_view_,
                                            player_control_.get_plugged_audio_source());

            player_control_.source_deselected_notification(nullptr);
            player_control_.unplug(true);

            if(!audio_source_is_deselected)
            {
                /* plug in audio source, pass player ID so that the D-Bus
                 * proxies for that player can be set up */
                msg_info("Plug external audio source %s into player",
                         audio_source->id_.c_str());

                log_assert(view != nullptr);

                audio_source->select_now();
                plug_audio_source(*audio_source,
                                  !view->flags_.is_any_set(ViewIface::Flags::NO_ENFORCED_USER_INTENTIONS),
                                  &player_id);
                player_control_.plug(player_data_);
                player_control_.plug(view->get_local_permissions());
                view_manager_->sync_activate_view_by_name(view->name_, true);
            }
            else
            {
                /* plain deselect and unplug: either we don't know the selected
                 * source or the audio path has been shutdown completely */
                if(view_for_deselected_audio_source != nullptr &&
                   view_for_deselected_audio_source->flags_.is_any_set(ViewIface::Flags::DROP_IN_FOR_INACTIVE_VIEW))
                    view_manager_->sync_activate_view_by_name(view_for_deselected_audio_source->name_,
                                                              false);
                else
                    view_manager_->sync_activate_view_by_name(ViewNames::INACTIVE,
                                                              false);
            }
        }

        break;

      case UI::ViewEventID::PLAYBACK_MODE_REPEAT_TOGGLE:
        player_control_.repeat_mode_toggle_request();
        break;

      case UI::ViewEventID::PLAYBACK_MODE_SHUFFLE_TOGGLE:
        player_control_.shuffle_mode_toggle_request();
        break;

      case UI::ViewEventID::SEARCH_COMMENCE:
      case UI::ViewEventID::SEARCH_STORE_PARAMETERS:
      case UI::ViewEventID::NOTIFY_AIRABLE_SERVICE_LOGIN_STATUS_UPDATE:
      case UI::ViewEventID::PLAYBACK_TRY_RESUME:
      case UI::ViewEventID::STRBO_URL_RESOLVED:
        BUG("Unexpected view event 0x%08x for play view",
            static_cast<unsigned int>(event_id));

        break;
    }

    return InputResult::OK;
}

void ViewPlay::View::process_broadcast(UI::BroadcastEventID event_id,
                                       UI::Parameters *parameters)
{
    const auto lock_ctrl(player_control_.lock());
    const auto lock_data(player_data_.lock());

    switch(event_id)
    {
      case UI::BroadcastEventID::NOP:
        break;

      case UI::BroadcastEventID::CONFIGURATION_UPDATED:
        {
            const auto params =
                UI::Events::downcast<UI::BroadcastEventID::CONFIGURATION_UPDATED>(parameters);

            if(params == nullptr)
                break;

            const auto &changed_ids(params->get_specific());

            if(std::find(changed_ids.begin(), changed_ids.end(),
                         Configuration::DrcpdValues::KeyID::MAXIMUM_BITRATE) != changed_ids.end())
                maximum_bitrate_ = view_manager_->get_configuration().maximum_bitrate_;
        }

        break;
    }
}

static I18n::StringView mk_alt_track_name(const MetaData::Set &meta_data)
{
    if(!meta_data.values_[MetaData::Set::INTERNAL_DRCPD_TITLE].empty())
        return I18n::StringView(false, meta_data.values_[MetaData::Set::INTERNAL_DRCPD_TITLE]);

    if(!meta_data.values_[MetaData::Set::INTERNAL_DRCPD_URL].empty())
        return I18n::StringView(false, meta_data.values_[MetaData::Set::INTERNAL_DRCPD_URL]);

    static const std::string no_name_fallback(N_("(no data available)"));
    return I18n::StringView(true, no_name_fallback);

}

static const std::string &get_bitrate(const MetaData::Set &md)
{
    if(!md.values_[MetaData::Set::BITRATE].empty())
        return md.values_[MetaData::Set::BITRATE];

    if(!md.values_[MetaData::Set::BITRATE_NOM].empty())
        return md.values_[MetaData::Set::BITRATE_NOM];

    if(!md.values_[MetaData::Set::BITRATE_MAX].empty())
        return md.values_[MetaData::Set::BITRATE_MAX];

    return md.values_[MetaData::Set::BITRATE_MIN];
}

static bool want_artist_track_album(const MetaData::Set &md)
{
    const auto &tags(md.values_);

    if(!tags[MetaData::Set::ARTIST].empty() ||
       !tags[MetaData::Set::TITLE].empty() ||
       !tags[MetaData::Set::ALBUM].empty())
        return true;

    return (tags[MetaData::Set::INTERNAL_DRCPD_OPAQUE_LINE_1].empty() &&
            tags[MetaData::Set::INTERNAL_DRCPD_OPAQUE_LINE_2].empty() &&
            tags[MetaData::Set::INTERNAL_DRCPD_OPAQUE_LINE_3].empty());
}

bool ViewPlay::View::write_xml(std::ostream &os, uint32_t bits,
                               const DCP::Queue::Data &data)
{
    const auto lock(player_data_.lock());
    const auto &md(player_data_.get_current_meta_data());
    const Player::VisibleStreamState stream_state(player_data_.get_current_visible_stream_state());
    const bool is_buffering = (stream_state == Player::VisibleStreamState::BUFFERING);

    const uint32_t update_flags =
        data.is_full_serialize_ ? UINT32_MAX : data.view_update_flags_;

    if(data.is_full_serialize_ && is_buffering)
        os << "<text id=\"track\">"
           << XmlEscape(_("Buffering")) << "..."
           << "</text>";
    else if((update_flags & UPDATE_FLAGS_META_DATA) != 0)
    {
        if(want_artist_track_album(md))
        {
            os << "<text id=\"artist\">"
               << XmlEscape(md.values_[MetaData::Set::ARTIST])
               << "</text>";
            os << "<text id=\"track\">"
               << XmlEscape(md.values_[MetaData::Set::TITLE])
               << "</text>";
            os << "<text id=\"album\">"
               << XmlEscape(md.values_[MetaData::Set::ALBUM])
               << "</text>";
        }
        else
        {
            os << "<text id=\"line0\">"
               << XmlEscape(md.values_[MetaData::Set::INTERNAL_DRCPD_OPAQUE_LINE_1])
               << "</text>";
            os << "<text id=\"line1\">"
               << XmlEscape(md.values_[MetaData::Set::INTERNAL_DRCPD_OPAQUE_LINE_2])
               << "</text>";
            os << "<text id=\"line2\">"
               << XmlEscape(md.values_[MetaData::Set::INTERNAL_DRCPD_OPAQUE_LINE_3])
               << "</text>";
        }
        os << "<text id=\"alttrack\">"
           << XmlEscape(mk_alt_track_name(md).get_text())
           << "</text>";
        os << "<text id=\"bitrate\">"
           << get_bitrate(md).c_str()
           << "</text>";
    }

    if((update_flags & UPDATE_FLAGS_STREAM_POSITION) != 0)
    {
        auto times = player_data_.get_now_playing().get_times();

        os << "<value id=\"timet\">";
        if(times.second >= std::chrono::milliseconds(0))
            os << std::chrono::duration_cast<std::chrono::seconds>(times.second).count();
        os << "</value>";

        if(times.first >= std::chrono::milliseconds(0))
            os << "<value id=\"timep\">"
                << std::chrono::duration_cast<std::chrono::seconds>(times.first).count()
                << "</value>";
    }

    if((update_flags & UPDATE_FLAGS_PLAYBACK_STATE) != 0)
    {
        /* matches enum #Player::VisibleStreamState */
        static const char *play_icon[] =
        {
            "",
            "",
            "play",
            "pause",
            "ffmode",
            "frmode",
        };

        static_assert(sizeof(play_icon) / sizeof(play_icon[0]) == static_cast<size_t>(Player::VisibleStreamState::LAST) + 1, "Array has wrong size");

        os << "<icon id=\"play\">"
           << play_icon[static_cast<size_t>(stream_state)]
           << "</icon>";
    }

    if((update_flags & UPDATE_FLAGS_PLAYBACK_MODES) != 0)
    {
        switch(player_data_.get_repeat_mode())
        {
          case DBus::ReportedRepeatMode::UNKNOWN:
          case DBus::ReportedRepeatMode::OFF:
            os << "<icon id=\"repeat\"/>";
            break;

          case DBus::ReportedRepeatMode::ALL:
            os << "<icon id=\"repeat\">repeat all</icon>";
            break;

          case DBus::ReportedRepeatMode::ONE:
            os << "<icon id=\"repeat\">repeat</icon>";
            break;
        }

        switch(player_data_.get_shuffle_mode())
        {
          case DBus::ReportedShuffleMode::UNKNOWN:
          case DBus::ReportedShuffleMode::OFF:
            os << "<icon id=\"shuffle\"/>";
            break;

          case DBus::ReportedShuffleMode::ON:
            os << "<icon id=\"shuffle\">shuffle</icon>";
            break;
        }
    }

    return true;
}

void ViewPlay::View::serialize(DCP::Queue &queue, DCP::Queue::Mode mode,
                               std::ostream *debug_os)
{
    if(!is_visible_)
        BUG("serializing invisible ViewPlay::View");

    ViewSerializeBase::serialize(queue, mode);

    if(!debug_os)
        return;

    /* matches enum #Player::VisibleStreamState */
    static const char *stream_state_string[] =
    {
        "not playing",
        "buffering",
        "playing",
        "paused",
        "fast forward",
        "fast rewind",
    };

    static_assert(sizeof(stream_state_string) / sizeof(stream_state_string[0]) == static_cast<size_t>(Player::VisibleStreamState::LAST) + 1, "Array has wrong size");

    const auto lock(player_data_.lock());
    const auto &md(player_data_.get_current_meta_data());
    const Player::VisibleStreamState stream_state(player_data_.get_current_visible_stream_state());

    *debug_os << "URL: \""
        << md.values_[MetaData::Set::INTERNAL_DRCPD_URL]
        << "\" ("
        << stream_state_string[static_cast<size_t>(stream_state)]
        << ")\n";
    *debug_os << "Stream state: " << static_cast<size_t>(stream_state) << '\n';

    for(size_t i = 0; i < md.values_.size(); ++i)
        *debug_os << "  " << i << ": \"" << md.values_[i] << "\"\n";
}

static const std::string reformat_bitrate(const char *in)
{
    log_assert(in != NULL);

    bool failed = false;
    unsigned long result = 0;

    if(in[0] < '0' || in[0] > '9')
        failed = true;

    if(!failed)
    {
        char *endptr = NULL;

        result = strtoul(in, &endptr, 10);
        failed = (*endptr != '\0' || (result == ULONG_MAX && errno == ERANGE) || result > UINT32_MAX);
    }

    if(failed)
    {
        msg_error(EINVAL, LOG_NOTICE,
                  "Invalid bitrate string: \"%s\", leaving as is", in);
        return in;
    }

    result = (result + 500UL) / 1000UL;
    std::ostringstream os;
    os << result;

    return os.str();
}

const MetaData::Reformatters ViewPlay::meta_data_reformatters =
{
    .bitrate_fn_ = reformat_bitrate,
};
