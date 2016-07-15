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

#include <cstdlib>

#include "view_play.hh"
#include "view_manager.hh"
#include "player.hh"
#include "ui_parameters_predefined.hh"
#include "dbus_iface_deep.h"
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

ViewIface::InputResult
ViewPlay::View::process_event(UI::ViewEventID event_id,
                              std::unique_ptr<const UI::Parameters> parameters)
{
    switch(event_id)
    {
      case UI::ViewEventID::PLAYBACK_START:
        switch(player_.get_assumed_stream_state__locked())
        {
          case PlayInfo::Data::STREAM_BUFFERING:
          case PlayInfo::Data::STREAM_PLAYING:
            if(!tdbus_splay_playback_call_pause_sync(dbus_get_streamplayer_playback_iface(),
                                                     NULL, NULL))
                msg_error(0, LOG_NOTICE, "Failed sending pause playback message");

            break;

          case PlayInfo::Data::STREAM_STOPPED:
          case PlayInfo::Data::STREAM_PAUSED:
            if(!tdbus_splay_playback_call_start_sync(dbus_get_streamplayer_playback_iface(),
                                                     NULL, NULL))
                msg_error(0, LOG_NOTICE, "Failed sending start playback message");

            break;
        }

        break;

      case UI::ViewEventID::PLAYBACK_STOP:
        player_.release(true);
        break;

      case UI::ViewEventID::PLAYBACK_PREVIOUS:
        player_.skip_to_previous(std::chrono::milliseconds(2000));
        break;

      case UI::ViewEventID::PLAYBACK_NEXT:
        player_.skip_to_next();
        break;

      case UI::ViewEventID::NAV_GO_BACK_ONE_LEVEL:
      case UI::ViewEventID::NAV_SCROLL_LINES:
      case UI::ViewEventID::NAV_SCROLL_PAGES:
        return InputResult::SHOULD_HIDE;

      case UI::ViewEventID::PLAYBACK_FAST_WIND_SET_SPEED:
        {
            const auto speed =
                UI::Events::downcast<UI::ViewEventID::PLAYBACK_FAST_WIND_SET_SPEED>(parameters);

            if(speed != nullptr)
                BUG("Not implemented: FastWindSetFactor %f", speed->get_specific());
        }

        break;

      case UI::ViewEventID::NOW_PLAYING:
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

            const bool have_preloaded_meta_data =
                player_.start_notification(stream_id, !queue_is_full);

            {
                const auto info = player_.get_stream_info__locked(stream_id);
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

                player_.meta_data_put__unlocked(meta_data, copy_mode);
            }

            if(have_preloaded_meta_data)
                this->notify_stream_meta_data_changed();

            this->notify_stream_start();

            auto *view = view_manager_->get_playback_initiator_view();
            if(view != nullptr && view != this)
                view->notify_stream_start();
        }

        break;

      case UI::ViewEventID::STREAM_STOPPED:
        {
            player_.stop_notification();
            this->notify_stream_stop();

            auto *view = view_manager_->get_playback_initiator_view();
            if(view != nullptr && view != this)
                view->notify_stream_stop();
        }

        break;

      case UI::ViewEventID::STREAM_PAUSED:
        player_.pause_notification();
        this->notify_stream_pause();

        break;

      case UI::ViewEventID::STREAM_POSITION:
        {
            const auto params =
                UI::Events::downcast<UI::EventID::VIEW_PLAYER_STREAM_POSITION>(parameters);
            const auto &plist = params->get_specific();

            if(player_.track_times_notification(std::get<1>(plist),
                                                std::get<2>(plist)))
                this->notify_stream_position_changed();
        }

        break;

      case UI::ViewEventID::STORE_STREAM_META_DATA:
        {
            const auto params =
                UI::Events::downcast<UI::EventID::VIEW_PLAYER_STORE_STREAM_META_DATA>(parameters);

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

            player_.meta_data_put__locked(meta_data,
                                          PlayInfo::MetaData::CopyMode::NON_EMPTY);
            this->notify_stream_meta_data_changed();
        }

        break;

      case UI::ViewEventID::STORE_PRELOADED_META_DATA:
        {
            const auto external_stream_info =
                UI::Events::downcast<UI::ViewEventID::STORE_PRELOADED_META_DATA>(parameters);
            log_assert(external_stream_info != nullptr);

            const auto &info(external_stream_info->get_specific());
            player_.set_external_stream_meta_data(std::get<0>(info), std::get<1>(info),
                                                  std::get<2>(info), std::get<3>(info),
                                                  std::get<4>(info), std::get<5>(info));
        }

        break;

      default:
        break;
    }

    return InputResult::OK;
}

void ViewPlay::View::notify_stream_start()
{
    msg_info("Play view: stream started, %s",
             is_visible_ ? "send screen update" : "but view is invisible");

    view_manager_->serialize_view_if_active(this, DCP::Queue::Mode::FORCE_ASYNC);
}

void ViewPlay::View::notify_stream_stop()
{
    msg_info("Play view: stream stopped, %s",
             is_visible_ ? "send screen update" : "but view is invisible");

    player_.release(false);
    add_update_flags(UPDATE_FLAGS_PLAYBACK_STATE);
    view_manager_->update_view_if_active(this, DCP::Queue::Mode::FORCE_ASYNC);
    view_manager_->hide_view_if_active(this);
}

void ViewPlay::View::notify_stream_pause()
{
    msg_info("Play view: stream paused, %s",
             is_visible_ ? "send screen update" : "but view is invisible");

    add_update_flags(UPDATE_FLAGS_PLAYBACK_STATE);
    view_manager_->update_view_if_active(this, DCP::Queue::Mode::FORCE_ASYNC);
}

void ViewPlay::View::notify_stream_position_changed()
{
    add_update_flags(UPDATE_FLAGS_STREAM_POSITION);
    view_manager_->update_view_if_active(this, DCP::Queue::Mode::FORCE_ASYNC);
}

void ViewPlay::View::notify_stream_meta_data_changed()
{
    add_update_flags(UPDATE_FLAGS_META_DATA);
    view_manager_->update_view_if_active(this, DCP::Queue::Mode::FORCE_ASYNC);
}

static const std::string &mk_alt_track_name(const PlayInfo::MetaData &meta_data)
{
    if(!meta_data.values_[PlayInfo::MetaData::INTERNAL_DRCPD_TITLE].empty())
        return meta_data.values_[PlayInfo::MetaData::INTERNAL_DRCPD_TITLE];

    if(!meta_data.values_[PlayInfo::MetaData::INTERNAL_DRCPD_URL].empty())
        return meta_data.values_[PlayInfo::MetaData::INTERNAL_DRCPD_URL];

    static const std::string no_name_fallback("(no data available)");
    return no_name_fallback;

}

static const std::string &get_bitrate(const PlayInfo::MetaData &md)
{
    if(!md.values_[PlayInfo::MetaData::BITRATE].empty())
        return md.values_[PlayInfo::MetaData::BITRATE];

    if(!md.values_[PlayInfo::MetaData::BITRATE_NOM].empty())
        return md.values_[PlayInfo::MetaData::BITRATE_NOM];

    if(!md.values_[PlayInfo::MetaData::BITRATE_MAX].empty())
        return md.values_[PlayInfo::MetaData::BITRATE_MAX];

    return md.values_[PlayInfo::MetaData::BITRATE_MIN];
}

bool ViewPlay::View::write_xml(std::ostream &os, const DCP::Queue::Data &data)
{
    const auto &md_with_lock = player_.get_track_meta_data__locked();
    const auto &md(*md_with_lock.first);
    const bool is_buffering =
        (player_.get_assumed_stream_state__unlocked() == PlayInfo::Data::STREAM_BUFFERING);

    const uint32_t update_flags =
        data.is_full_serialize_ ? UINT32_MAX : data.view_update_flags_;

    if(data.is_full_serialize_ && is_buffering)
        os << "<text id=\"track\">"
           << XmlEscape(N_("Buffering")) << "..."
           << "</text>";
    else if((update_flags & UPDATE_FLAGS_META_DATA) != 0)
    {
        os << "<text id=\"artist\">"
           << XmlEscape(md.values_[PlayInfo::MetaData::ARTIST])
           << "</text>";
        os << "<text id=\"track\">"
           << XmlEscape(md.values_[PlayInfo::MetaData::TITLE])
           << "</text>";
        os << "<text id=\"alttrack\">"
           << XmlEscape(mk_alt_track_name(md))
           << "</text>";
        os << "<text id=\"album\">"
           << XmlEscape(md.values_[PlayInfo::MetaData::ALBUM])
           << "</text>";
        os << "<text id=\"bitrate\">"
           << get_bitrate(md).c_str()
           << "</text>";
    }

    if((update_flags & UPDATE_FLAGS_STREAM_POSITION) != 0)
    {
        auto times = player_.get_times__unlocked();

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
        /* matches enum #PlayInfo::Data::StreamState */
        static const char *play_icon[] =
        {
            "",
            "",
            "play",
            "pause",
        };

        static_assert(sizeof(play_icon) / sizeof(play_icon[0]) == PlayInfo::Data::STREAM_STATE_LAST + 1, "Array has wrong size");

        os << "<icon id=\"play\">"
           << play_icon[player_.get_assumed_stream_state__unlocked()]
           << "</icon>";
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

    /* matches enum #PlayInfo::Data::StreamState */
    static const char *stream_state_string[] =
    {
        "not playing",
        "buffering",
        "playing",
        "paused",
    };

    static_assert(sizeof(stream_state_string) / sizeof(stream_state_string[0]) == PlayInfo::Data::STREAM_STATE_LAST + 1, "Array has wrong size");

    const auto &md_with_lock = player_.get_track_meta_data__locked();
    const auto &md(*md_with_lock.first);

    *debug_os << "URL: \""
        << md.values_[PlayInfo::MetaData::INTERNAL_DRCPD_URL]
        << "\" ("
        << stream_state_string[player_.get_assumed_stream_state__unlocked()]
        << ")" << std::endl;
    *debug_os << "Stream state: " << player_.get_assumed_stream_state__unlocked() << std::endl;

    for(size_t i = 0; i < md.values_.size(); ++i)
        *debug_os << "  " << i << ": \"" << md.values_[i] << "\"" << std::endl;
}

static const std::string reformat_bitrate(const char *in)
{
    log_assert(in != NULL);

    bool failed = false;
    unsigned long result;

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

const PlayInfo::Reformatters ViewPlay::meta_data_reformatters =
{
    .bitrate = reformat_bitrate,
};
