/*
 * Copyright (C) 2017, 2019, 2020, 2021  T+A elektroakustik GmbH & Co. KG
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

#ifndef PLAYER_RESUMER_HH
#define PLAYER_RESUMER_HH

#include "rnfcall_realize_location.hh"
#include "playlist_crawler.hh"

#include <string>

namespace Player
{

/*!
 * Synchronization of realizing a list location and audio source selection.
 */
class Resumer
{
  private:
    DBusRNF::RealizeLocationCall call_;
    bool is_audio_source_available_;
    bool already_notified_;
    Playlist::Crawler::Handle crawler_handle_;
    UI::EventStoreIface &event_sink_;

  public:
    Resumer(const Resumer &) = delete;
    Resumer &operator=(const Resumer &) = delete;

    explicit Resumer(std::string &&location_key, DBusRNF::CookieManagerIface &cm,
                     tdbuslistsNavigation *nav_proxy, Playlist::Crawler::Handle ch,
                     UI::EventStoreIface &event_sink):
        call_(cm, nav_proxy, std::move(location_key), nullptr,
              [this] (const auto &, auto state, bool) { call_state_changed(state); }),
        is_audio_source_available_(false),
        already_notified_(false),
        crawler_handle_(std::move(ch)),
        event_sink_(event_sink)
    {
        Busy::set(Busy::Source::RESUMING_PLAYBACK);
    }

    ~Resumer()
    {
        call_.abort_request_on_destroy();
        Busy::clear(Busy::Source::RESUMING_PLAYBACK);
    }

    const std::string &get_url() const { return call_.get_url(); }

    void audio_source_available_notification()
    {
        if(is_audio_source_available_)
            return;

        is_audio_source_available_ = true;
        call_.request();
    }

    DBusRNF::RealizeLocationResult get()
    {
        call_.fetch();
        return call_.get_result_locked();
    }

    Playlist::Crawler::Handle take_crawler_handle()
    {
        return std::move(crawler_handle_);
    }

  private:
    void call_state_changed(DBusRNF::CallState state)
    {
        switch(state)
        {
          case DBusRNF::CallState::INITIALIZED:
          case DBusRNF::CallState::WAIT_FOR_NOTIFICATION:
          case DBusRNF::CallState::ABORTING:
            break;

          case DBusRNF::CallState::READY_TO_FETCH:
            event_sink_.store_event(UI::EventID::VIEW_STRBO_URL_RESOLVED);
            already_notified_ = true;
            break;

          case DBusRNF::CallState::RESULT_FETCHED:
          case DBusRNF::CallState::ABORTED_BY_LIST_BROKER:
          case DBusRNF::CallState::FAILED:
            if(!already_notified_)
                event_sink_.store_event(UI::EventID::VIEW_STRBO_URL_RESOLVED);
            break;

          case DBusRNF::CallState::ABOUT_TO_DESTROY:
            break;
        }
    }
};

}

#endif /* !PLAYER_RESUMER_HH */
