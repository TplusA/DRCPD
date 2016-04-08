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

ViewIface::InputResult ViewPlay::View::input(DrcpCommand command,
                                             std::unique_ptr<const UI::Parameters> parameters)
{
    switch(command)
    {
      case DrcpCommand::PLAYBACK_START:
        switch(player_.get_assumed_stream_state())
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

      case DrcpCommand::PLAYBACK_STOP:
        player_.release(true);
        break;

      case DrcpCommand::PLAYBACK_PREVIOUS:
        player_.skip_to_previous(std::chrono::milliseconds(2000));
        break;

      case DrcpCommand::PLAYBACK_NEXT:
        player_.skip_to_next();
        break;

      case DrcpCommand::GO_BACK_ONE_LEVEL:
      case DrcpCommand::SCROLL_UP_ONE:
      case DrcpCommand::SCROLL_DOWN_ONE:
      case DrcpCommand::SCROLL_PAGE_UP:
      case DrcpCommand::SCROLL_PAGE_DOWN:
        return InputResult::SHOULD_HIDE;

      case DrcpCommand::FAST_WIND_SET_SPEED:
        {
            const auto speed =
                UI::Parameters::downcast<const UI::ParamsFWSpeed>(parameters);

            if(speed != nullptr)
                BUG("Not implemented: FastWindSetFactor %f", speed->get_specific());
        }

        break;

      case DrcpCommand::X_TA_SET_STREAM_INFO:
        {
            const auto external_stream_info =
                UI::Parameters::downcast<const UI::ParamsStreamInfo>(parameters);

            msg_info("PLAY VIEW: Set external stream information %p", external_stream_info.get());

            if(external_stream_info != nullptr)
            {
                const auto &info(external_stream_info->get_specific());
                player_.set_external_stream_meta_data(std::get<0>(info), std::get<1>(info),
                                                      std::get<2>(info), std::get<3>(info),
                                                      std::get<4>(info), std::get<5>(info));
            }
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

    view_manager_->serialize_view_if_active(this);
}

void ViewPlay::View::notify_stream_stop()
{
    msg_info("Play view: stream stopped, %s",
             is_visible_ ? "send screen update" : "but view is invisible");

    player_.release(false);
    add_update_flags(UPDATE_FLAGS_PLAYBACK_STATE);
    view_manager_->update_view_if_active(this);
    view_manager_->hide_view_if_active(this);
}

void ViewPlay::View::notify_stream_pause()
{
    msg_info("Play view: stream paused, %s",
             is_visible_ ? "send screen update" : "but view is invisible");

    add_update_flags(UPDATE_FLAGS_PLAYBACK_STATE);
    view_manager_->update_view_if_active(this);
}

void ViewPlay::View::notify_stream_position_changed()
{
    msg_info("Play view: stream position changed, %s",
             is_visible_ ? "send screen update" : "but view is invisible");

    add_update_flags(UPDATE_FLAGS_STREAM_POSITION);
    view_manager_->update_view_if_active(this);
}

void ViewPlay::View::notify_stream_meta_data_changed()
{
    msg_info("Play view: stream meta data changed, %s",
             is_visible_ ? "send screen update" : "but view is invisible");

    add_update_flags(UPDATE_FLAGS_META_DATA);
    view_manager_->update_view_if_active(this);
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
    const auto &md_with_lock = player_.get_track_meta_data();
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

void ViewPlay::View::serialize(DCP::Queue &queue, std::ostream *debug_os)
{
    if(!is_visible_)
        BUG("serializing invisible ViewPlay::View");

    ViewSerializeBase::serialize(queue);

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

    const auto &md_with_lock = player_.get_track_meta_data();
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
