/*
 * Copyright (C) 2017--2021, 2023  T+A elektroakustik GmbH & Co. KG
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

#ifndef VIEW_INACTIVE_HH
#define VIEW_INACTIVE_HH

#include "view.hh"
#include "view_manager.hh"
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
  private:
    bool enable_deselect_notifications_;

  public:
    View(const View &) = delete;
    View &operator=(const View &) = delete;

    explicit View(const char *on_screen_name, ViewManager::VMIface &vm):
        ViewIface(ViewNames::INACTIVE, ViewIface::Flags(), vm),
        ViewSerializeBase(on_screen_name, ViewID::INVALID),
        enable_deselect_notifications_(false)
    {}

    void enable_deselect_notifications()
    {
        enable_deselect_notifications_ = true;
    }

    bool init() override { return true; }

    void focus() override
    {
        if(enable_deselect_notifications_)
            view_manager_->deselected_notification();
    }

    void defocus() override {}

    InputResult process_event(UI::ViewEventID event_id,
                              std::unique_ptr<UI::Parameters> parameters) final override
    {
        return InputResult::OK;
    }

    void process_broadcast(UI::BroadcastEventID event_id,
                           UI::Parameters *parameters) final override {}

    void serialize(DCP::Queue &queue, DCP::Queue::Mode mode,
                   std::ostream *debug_os, const Maybe<bool> &is_busy) override {}
    void update(DCP::Queue &queue, DCP::Queue::Mode mode,
                std::ostream *debug_os, const Maybe<bool> &is_busy) override {}

  private:
    bool is_serialization_allowed() const final override { return false; }
};

}

/*!@}*/

#endif /* !VIEW_INACTIVE_HH */
