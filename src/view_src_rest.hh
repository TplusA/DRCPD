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

#ifndef VIEW_SRC_REST_HH
#define VIEW_SRC_REST_HH

#include "view_external_source_base.hh"
#include "view_names.hh"

namespace ViewSourceREST
{

class View: public ViewExternalSource::Base
{
  private:
    static constexpr const uint32_t UPDATE_FLAGS_LINE0 = 1U << 0;
    static constexpr const uint32_t UPDATE_FLAGS_LINE1 = 1U << 1;

    std::array<std::string, 2> lines_;

  public:
    View(const View &) = delete;
    View &operator=(const View &) = delete;

    explicit View(const char *on_screen_name, ViewManager::VMIface &view_manager):
        Base(ViewNames::REST_API, on_screen_name, "strbo.rest", view_manager,
             ViewIface::Flags(ViewIface::Flags::CAN_RETURN_TO_THIS |
                              ViewIface::Flags::NAVIGATION_BLOCKED |
                              ViewIface::Flags::PLAYER_COMMANDS_BLOCKED))
    {}

    InputResult process_event(UI::ViewEventID event_id,
                              std::unique_ptr<UI::Parameters> parameters) final override;

    const Player::LocalPermissionsIface &get_local_permissions() const final override;

    /*!
     * Set the title and up to two lines on the screen.
     *
     * This functionality should *not* be used to show playback information (we
     * have the play screen for this purpose). The REST API client could show a
     * friendly name, phone name, current browse context, or whatever
     * information that has something to do with the REST API client itself.
     *
     * The request is a JSON object containing a display operation
     * ("display_set" or "display_update") which tells us what to do. The title
     * is expected in field "title", and the two lines are expected in fields
     * "first_line" and "second_line".
     *
     * If both lines are cleared, then a fallback string will be show instead.
     * Therefore, it is not possible to show a blank screen (unless using
     * characters that have no on-screen representation). The same is true for
     * the title string.
     */
    ViewIface::InputResult
    set_display_update_request(const std::string &request);

    bool set_line(size_t idx, std::string &&str);
    bool set_line(size_t idx, const std::string &str);

  protected:
    bool write_xml(std::ostream &os, uint32_t bits,
                   const DCP::Queue::Data &data) final override;
};

}

#endif /* !VIEW_SRC_REST_HH */
