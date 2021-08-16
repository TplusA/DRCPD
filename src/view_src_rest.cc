/*
 * Copyright (C) 2021  T+A elektroakustik GmbH & Co. KG
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

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch-enum"
#include "json.hh"
#pragma GCC diagnostic pop

#include "view_src_rest.hh"
#include "player_permissions.hh"
#include "ui_parameters_predefined.hh"

class RESTPermissions: public Player::DefaultLocalPermissions
{
  public:
    RESTPermissions(const RESTPermissions &) = delete;
    RESTPermissions &operator=(const RESTPermissions &) = delete;

    constexpr explicit RESTPermissions() {}

    bool can_show_listing()         const override { return false; }
    bool can_prefetch_for_gapless() const override { return false; }
};

ViewIface::InputResult
ViewSourceREST::View::process_event(UI::ViewEventID event_id,
                                    std::unique_ptr<UI::Parameters> parameters)
{
    switch(event_id)
    {
      case UI::ViewEventID::NOP:
        break;

      case UI::ViewEventID::SET_DISPLAY_CONTENT:
        {
            const auto params =
                UI::Events::downcast<UI::ViewEventID::SET_DISPLAY_CONTENT>(parameters);
            if(params == nullptr)
                break;

            const auto &plist = params->get_specific();
            return set_display_update_request(std::get<1>(plist));
        }

        break;

      case UI::ViewEventID::PLAYBACK_COMMAND_START:
      case UI::ViewEventID::PLAYBACK_COMMAND_STOP:
      case UI::ViewEventID::PLAYBACK_COMMAND_PAUSE:
      case UI::ViewEventID::PLAYBACK_PREVIOUS:
      case UI::ViewEventID::PLAYBACK_NEXT:
      case UI::ViewEventID::PLAYBACK_FAST_WIND_SET_SPEED:
      case UI::ViewEventID::PLAYBACK_SEEK_STREAM_POS:
      case UI::ViewEventID::PLAYBACK_MODE_REPEAT_TOGGLE:
      case UI::ViewEventID::PLAYBACK_MODE_SHUFFLE_TOGGLE:
      case UI::ViewEventID::NAV_SELECT_ITEM:
      case UI::ViewEventID::NAV_SCROLL_LINES:
      case UI::ViewEventID::NAV_SCROLL_PAGES:
      case UI::ViewEventID::NAV_GO_BACK_ONE_LEVEL:
      case UI::ViewEventID::SEARCH_COMMENCE:
        break;

      case UI::ViewEventID::SEARCH_STORE_PARAMETERS:
      case UI::ViewEventID::STORE_STREAM_META_DATA:
      case UI::ViewEventID::STORE_PRELOADED_META_DATA:
      case UI::ViewEventID::NOTIFY_AIRABLE_SERVICE_LOGIN_STATUS_UPDATE:
      case UI::ViewEventID::NOTIFY_NOW_PLAYING:
      case UI::ViewEventID::NOTIFY_STREAM_STOPPED:
      case UI::ViewEventID::NOTIFY_STREAM_PAUSED:
      case UI::ViewEventID::NOTIFY_STREAM_UNPAUSED:
      case UI::ViewEventID::NOTIFY_STREAM_POSITION:
      case UI::ViewEventID::NOTIFY_SPEED_CHANGED:
      case UI::ViewEventID::NOTIFY_PLAYBACK_MODE_CHANGED:
      case UI::ViewEventID::AUDIO_SOURCE_SELECTED:
      case UI::ViewEventID::AUDIO_SOURCE_DESELECTED:
      case UI::ViewEventID::AUDIO_PATH_CHANGED:
      case UI::ViewEventID::STRBO_URL_RESOLVED:
      case UI::ViewEventID::PLAYBACK_TRY_RESUME:
        BUG("Unexpected view event 0x%08x for REST audio source view",
            static_cast<unsigned int>(event_id));
        break;
    }

    return InputResult::OK;
}

const Player::LocalPermissionsIface &ViewSourceREST::View::get_local_permissions() const
{
    static const RESTPermissions permissions;
    return permissions;
}

static ViewIface::InputResult
process_display_update(ViewSourceREST::View &view, const nlohmann::json &req,
                       bool is_complete_update)
{
    bool changed_title = false;

    if(!req.contains("title"))
    {
        if(is_complete_update)
        {
            changed_title = !view.get_dynamic_title().empty();
            view.clear_dynamic_title();
        }
    }
    else
    {
        const auto &title(req["title"].get<std::string>());

        if(title.empty())
        {
            changed_title = !view.get_dynamic_title().empty();
            view.clear_dynamic_title();
        }
        else
        {
            changed_title = !view.get_dynamic_title().is_equal_untranslated(title);
            view.set_dynamic_title(title.c_str());
        }
    }

    bool changed_lines = false;

    if(!req.contains("first_line"))
    {
        if(is_complete_update)
            changed_lines = view.set_line(0, "");
    }
    else if(view.set_line(0, req["first_line"].get<std::string>()))
        changed_lines = true;

    if(!req.contains("second_line"))
    {
        if(is_complete_update)
            changed_lines = view.set_line(1, "");
    }
    else if(view.set_line(1, req["second_line"].get<std::string>()))
        changed_lines = true;

    return changed_title
        ? ViewIface::InputResult::FULL_SERIALIZE_NEEDED
        : (changed_lines
           ? ViewIface::InputResult::UPDATE_NEEDED
           : ViewIface::InputResult::OK);
}

bool ViewSourceREST::View::set_line(size_t idx, std::string &&str)
{
    log_assert(idx < lines_.size());

    if(lines_.at(idx) == str)
        return false;

    lines_[idx] = std::move(str);

    if(idx == 0)
        add_update_flags(UPDATE_FLAGS_LINE0);
    else
        add_update_flags(UPDATE_FLAGS_LINE1);

    return true;
}

bool ViewSourceREST::View::set_line(size_t idx, const std::string &str)
{
    log_assert(idx < lines_.size());

    if(lines_.at(idx) == str)
        return false;

    lines_[idx] = str;

    if(idx == 0)
        add_update_flags(UPDATE_FLAGS_LINE0);
    else
        add_update_flags(UPDATE_FLAGS_LINE1);

    return true;
}

ViewIface::InputResult
ViewSourceREST::View::set_display_update_request(const std::string &request)
{
    auto changed = ViewIface::InputResult::OK;

    try
    {
        const auto req(nlohmann::json::parse(request));
        const auto &opname(req.at("op").get<std::string>());

        if(opname == "display_set")
            changed = process_display_update(*this, req, true);
        else if(opname == "display_update")
            changed = process_display_update(*this, req, false);
        else
            msg_error(0, LOG_NOTICE,
                      "Unknown display operation \"%s\"", opname.c_str());
    }
    catch(const std::exception &e)
    {
        msg_error(0, LOG_ERR,
                  "Failed parsing display update request: %s", e.what());
        return changed;
    }

    return changed;
}

bool ViewSourceREST::View::write_xml(std::ostream &os, uint32_t bits,
                                     const DCP::Queue::Data &data)
{
    if(lines_[0].empty() and lines_[1].empty())
        return ViewExternalSource::Base::write_xml(os, bits, data);

    const uint32_t update_flags =
        data.is_full_serialize_ ? UINT32_MAX : data.view_update_flags_;

    if((update_flags & UPDATE_FLAGS_LINE0) != 0)
        os << "<text id=\"line0\">" << XmlEscape(lines_[0]) << "</text>";

    if((update_flags & UPDATE_FLAGS_LINE1) != 0)
        os << "<text id=\"line1\">" << XmlEscape(lines_[1]) << "</text>";

    return true;
}
