/*
 * Copyright (C) 2017  T+A elektroakustik GmbH & Co. KG
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

#ifndef VIEW_INACTIVE_HH
#define VIEW_INACTIVE_HH

#include "view.hh"
#include "view_serialize.hh"
#include "view_names.hh"

/*!
 * \addtogroup view_inactive View for inactive state.
 * \ingroup views
 *
 * Like the NOP view, this is a view without any functionality. It is activated
 * when deselecting the audio source.
 */
/*!@{*/

namespace ViewInactive
{

class View: public ViewIface, public ViewSerializeBase
{
  public:
    View(const View &) = delete;
    View &operator=(const View &) = delete;

    explicit View(const char *on_screen_name):
        ViewIface(ViewNames::INACTIVE, false, nullptr),
        ViewSerializeBase(on_screen_name, ViewID::INVALID)
    {}

    bool init() override { return true; }
    void focus() override {}
    void defocus() override {}

    InputResult process_event(UI::ViewEventID event_id,
                              std::unique_ptr<const UI::Parameters> parameters) final override
    {
        return InputResult::OK;
    }

    void process_broadcast(UI::BroadcastEventID event_id,
                           const UI::Parameters *parameters) final override {}

    void serialize(DCP::Queue &queue, DCP::Queue::Mode mode, std::ostream *debug_os) override {}
    void update(DCP::Queue &queue, DCP::Queue::Mode mode, std::ostream *debug_os) override {}
};

};

/*!@}*/

#endif /* !VIEW_INACTIVE_HH */
