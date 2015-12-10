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

#include <cstdlib>

#include "view_play.hh"
#include "player.hh"
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

ViewIface::InputResult ViewPlay::View::input(DrcpCommand command)
{
    switch(command)
    {
      case DrcpCommand::PLAYBACK_START:
        switch(player_.get_assumed_stream_state())
        {
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
        return InputResult::SHOULD_HIDE;

      default:
        break;
    }

    return InputResult::OK;
}

void ViewPlay::View::notify_stream_start()
{
    msg_info("Play view: stream started, %s",
             is_visible_ ? "send screen update" : "but view is invisible");

    display_update(update_flags_need_full_update);
}

void ViewPlay::View::notify_stream_stop()
{
    msg_info("Play view: stream stopped, %s",
             is_visible_ ? "send screen update" : "but view is invisible");

    player_.release(false);
    view_signals_->request_hide_view(this);
}

void ViewPlay::View::notify_stream_pause()
{
    msg_info("Play view: stream paused, %s",
             is_visible_ ? "send screen update" : "but view is invisible");

    display_update(update_flags_playback_state);
}

void ViewPlay::View::notify_stream_position_changed()
{
    msg_info("Play view: stream position changed, %s",
             is_visible_ ? "send screen update" : "but view is invisible");

    display_update(update_flags_stream_position);
}

void ViewPlay::View::notify_stream_meta_data_changed()
{
    msg_info("Play view: stream meta data changed, %s",
             is_visible_ ? "send screen update" : "but view is invisible");

    display_update(update_flags_meta_data);
}

static const std::string mk_alt_track_name(const PlayInfo::MetaData &meta_data,
                                           size_t max_length)
{
    log_assert(max_length > 0);

    const std::string &alt_track =
        (meta_data.values_[PlayInfo::MetaData::INTERNAL_DRCPD_TITLE].empty()
         ? meta_data.values_[PlayInfo::MetaData::INTERNAL_DRCPD_URL]
         : meta_data.values_[PlayInfo::MetaData::INTERNAL_DRCPD_TITLE]);

    if(!alt_track.empty())
    {
        const glong len = g_utf8_strlen(alt_track.c_str(), -1);

        if(len > 0)
        {
            if(size_t(len) <= max_length)
                return alt_track;

            const gchar *const end = g_utf8_offset_to_pointer(alt_track.c_str(), max_length);
            const ptrdiff_t num_of_bytes = end - alt_track.c_str();

            if(num_of_bytes > 0)
                return std::string(alt_track, 0, num_of_bytes);
        }
    }

    return std::string("NO NAME", max_length);
}

bool ViewPlay::View::write_xml(std::ostream &os, bool is_full_view)
{
    const auto &md = player_.get_track_meta_data();

    if(is_full_view)
        update_flags_ = UINT16_MAX;

    if((update_flags_ & update_flags_meta_data) != 0)
    {
        os << "    <text id=\"artist\">"
           << XmlEscape(md.values_[PlayInfo::MetaData::ARTIST])
           << "</text>\n";
        os << "    <text id=\"track\">"
           << XmlEscape(md.values_[PlayInfo::MetaData::TITLE])
           << "</text>\n";
        os << "   <text id=\"alttrack\">"
           << XmlEscape(mk_alt_track_name(md, 20))
           << "</text>\n";
        os << "    <text id=\"album\">"
           << XmlEscape(md.values_[PlayInfo::MetaData::ALBUM])
           << "</text>\n";
        os << "    <text id=\"bitrate\">"
           << md.values_[PlayInfo::MetaData::BITRATE_NOM]
           << "</text>\n";
    }

    if((update_flags_ & update_flags_stream_position) != 0)
    {
        auto times = player_.get_times();

        os << "    <value id=\"timet\">";
        if(times.second >= std::chrono::milliseconds(0))
            os << std::chrono::duration_cast<std::chrono::seconds>(times.second).count();
        os << "</value>\n";

        if(times.first >= std::chrono::milliseconds(0))
            os << "    <value id=\"timep\">"
                << std::chrono::duration_cast<std::chrono::seconds>(times.first).count()
                << "</value>\n";
    }

    if((update_flags_ & update_flags_playback_state) != 0)
    {
        /* matches enum #PlayInfo::Data::StreamState */
        static const char *play_icon[] =
        {
            "",
            "play",
            "pause",
        };

        static_assert(sizeof(play_icon) / sizeof(play_icon[0]) == PlayInfo::Data::STREAM_STATE_LAST + 1, "Array has wrong size");

        os << "    <icon id=\"play\">"
           << play_icon[player_.get_assumed_stream_state()]
           << "</icon>\n";
    }

    return true;
}

bool ViewPlay::View::serialize(DcpTransaction &dcpd, std::ostream *debug_os)
{
    if(!is_visible_)
        BUG("serializing invisible ViewPlay::View");

    const bool retval = ViewIface::serialize(dcpd);

    if(retval)
        update_flags_ = 0;

    if(!debug_os)
        return retval;

    /* matches enum #PlayInfo::Data::StreamState */
    static const char *stream_state_string[] =
    {
        "not playing",
        "playing",
        "paused",
    };

    static_assert(sizeof(stream_state_string) / sizeof(stream_state_string[0]) == PlayInfo::Data::STREAM_STATE_LAST + 1, "Array has wrong size");

    const auto &md = player_.get_track_meta_data();

    *debug_os << "URL: \""
        << md.values_[PlayInfo::MetaData::INTERNAL_DRCPD_URL]
        << "\" ("
        << stream_state_string[player_.get_assumed_stream_state()]
        << ")" << std::endl;

    for(size_t i = 0; i < md.values_.size(); ++i)
        *debug_os << "  " << i << ": \"" << md.values_[i] << "\"" << std::endl;

    return retval;
}

bool ViewPlay::View::update(DcpTransaction &dcpd, std::ostream *debug_os)
{
    if(!is_visible_)
        BUG("updating invisible ViewPlay::View");

    if(update_flags_ == 0)
        BUG("display update requested, but nothing to update");

    bool retval;

    if((update_flags_ & update_flags_need_full_update) != 0)
        retval = serialize(dcpd, debug_os);
    else
        retval = ViewIface::update(dcpd, debug_os);

    if(retval)
        update_flags_ = 0;

    return retval;
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
