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

#ifndef VIEW_NOP_HH
#define VIEW_NOP_HH

#include "view.hh"
#include "view_serialize.hh"
#include "view_names.hh"

/*!
 * \addtogroup view_nop Dummy view
 * \ingroup views
 *
 * A view without any functionality.
 *
 * This view was implemented to avoid the need to handle null pointers in
 * various locations.
 */
/*!@{*/

namespace ViewNop
{

class View: public ViewIface, public ViewSerializeBase
{
  public:
    View(const View &) = delete;
    View &operator=(const View &) = delete;

    explicit View():
        ViewIface(ViewNames::NOP, ViewIface::Flags()),
        ViewSerializeBase("", ViewID::INVALID)
    {}

    bool init() override { return true; }
    void focus() override {}
    void defocus() override {}

    InputResult process_event(UI::ViewEventID event_id,
                              std::unique_ptr<UI::Parameters> parameters) final override
    {
        return InputResult::SHOULD_HIDE;
    }

    void process_broadcast(UI::BroadcastEventID event_id,
                           UI::Parameters *parameters) final override {}

    void serialize(DCP::Queue &queue, DCP::Queue::Mode mode, std::ostream *debug_os) override {}
    void update(DCP::Queue &queue, DCP::Queue::Mode mode, std::ostream *debug_os) override {}

  private:
    bool is_serialization_allowed() const final override { return false; }
};

}

/*!@}*/

#endif /* !VIEW_NOP_HH */
