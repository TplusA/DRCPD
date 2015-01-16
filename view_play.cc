#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "view_play.hh"
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
        switch(info_.assumed_stream_state_)
        {
          case PlayInfo::Data::STREAM_PLAYING:
            if(!tdbus_splay_playback_call_pause_sync(dbus_get_streamplayer_playback_iface(),
                                                     NULL, NULL))
                msg_error(EIO, LOG_NOTICE, "Failed sending pause playback message");

            break;

          case PlayInfo::Data::STREAM_STOPPED:
          case PlayInfo::Data::STREAM_PAUSED:
            if(!tdbus_splay_playback_call_start_sync(dbus_get_streamplayer_playback_iface(),
                                                     NULL, NULL))
                msg_error(EIO, LOG_NOTICE, "Failed sending start playback message");

            break;
        }

        break;

      case DrcpCommand::PLAYBACK_STOP:
        if(!tdbus_splay_playback_call_stop_sync(dbus_get_streamplayer_playback_iface(),
                                                NULL, NULL))
            msg_error(EIO, LOG_NOTICE, "Failed sending stop playback message");

        break;

      case DrcpCommand::GO_BACK_ONE_LEVEL:
        return InputResult::SHOULD_HIDE;

      default:
        break;
    }

    return InputResult::OK;
}

void ViewPlay::View::notify_stream_start(uint32_t id, const std::string &url,
                                         bool url_fifo_is_full)
{
    info_.url_ = url;
    info_.assumed_stream_state_ = PlayInfo::Data::STREAM_PLAYING;
    msg_info("Play view: stream started, %s",
             is_visible_ ? "send screen update" : "but view is invisible");

    display_update(update_flags_need_full_update);
}

void ViewPlay::View::notify_stream_stop()
{
    info_.assumed_stream_state_ = PlayInfo::Data::STREAM_STOPPED;
    msg_info("Play view: stream stopped, %s",
             is_visible_ ? "send screen update" : "but view is invisible");

    view_signals_->request_hide_view(this);
}

void ViewPlay::View::notify_stream_pause()
{
    info_.assumed_stream_state_ = PlayInfo::Data::STREAM_PAUSED;
    msg_info("Play view: stream paused, %s",
             is_visible_ ? "send screen update" : "but view is invisible");

    display_update(update_flags_playback_state);
}

void ViewPlay::View::notify_stream_position_changed(const std::chrono::milliseconds &position,
                                                    const std::chrono::milliseconds &duration)
{
    if(info_.stream_position_ == position && info_.stream_duration_ == duration)
        return;

    msg_info("Play view: stream position changed, %s",
             is_visible_ ? "send screen update" : "but view is invisible");

    info_.stream_position_ = position;
    info_.stream_duration_ = duration;
    display_update(update_flags_stream_position);
}

bool ViewPlay::View::write_xml(std::ostream &os, bool is_full_view)
{
    if(is_full_view)
        update_flags_ = UINT16_MAX;

    if((update_flags_ & update_flags_meta_data) != 0)
    {
        os << "    <text id=\"artist\">"
           << XmlEscape(info_.meta_data_.values_[PlayInfo::MetaData::ARTIST])
           << "</text>\n";
        os << "    <text id=\"track\">"
           << XmlEscape(info_.meta_data_.values_[PlayInfo::MetaData::TITLE])
           << "</text>\n";
        os << "    <text id=\"album\">"
           << XmlEscape(info_.meta_data_.values_[PlayInfo::MetaData::ALBUM])
           << "</text>\n";
        os << "    <text id=\"bitrate\">"
           << info_.meta_data_.values_[PlayInfo::MetaData::BITRATE_NOM]
           << "</text>\n";
    }

    if((update_flags_ & update_flags_stream_position) != 0)
    {
        os << "    <value id=\"timet\">";
        if(info_.stream_duration_ >= std::chrono::milliseconds(0))
            os << std::chrono::duration_cast<std::chrono::seconds>(info_.stream_duration_).count();
        os << "</value>\n";

        if(info_.stream_position_ >= std::chrono::milliseconds(0))
            os << "    <value id=\"timep\">"
                << std::chrono::duration_cast<std::chrono::seconds>(info_.stream_position_).count()
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

        os << "    <icon id=\"play\">"
           << play_icon[info_.assumed_stream_state_]
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

    *debug_os << "URL: \"" << info_.url_ << "\" ("
              << stream_state_string[info_.assumed_stream_state_]
              << ")" << std::endl;

    for(size_t i = 0; i < info_.meta_data_.values_.size(); ++i)
        *debug_os << "  " << i << ": \"" << info_.meta_data_.values_[i] << "\"" << std::endl;

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
