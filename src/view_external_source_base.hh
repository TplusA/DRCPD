/*
 * Copyright (C) 2017--2021  T+A elektroakustik GmbH & Co. KG
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

#ifndef VIEW_EXTERNAL_SOURCE_BASE_HH
#define VIEW_EXTERNAL_SOURCE_BASE_HH

#include "view.hh"
#include "view_serialize.hh"
#include "view_audiosource.hh"
#include "player_permissions.hh"

namespace ViewExternalSource
{

class Base: public ViewIface, public ViewSerializeBase, public ViewWithAudioSourceBase
{
  private:
    ViewIface *play_view_;
    const char *const default_audio_source_name_;

  protected:
    explicit Base(const char *name, const char *on_screen_name,
                  const char *audio_source_name,
                  ViewManager::VMIface &view_manager):
        Base(name, on_screen_name, audio_source_name, view_manager,
             ViewIface::Flags(ViewIface::Flags::CAN_RETURN_TO_THIS))
    {}

    explicit Base(const char *name, const char *on_screen_name,
                  const char *audio_source_name,
                  ViewManager::VMIface &view_manager,
                  ViewIface::Flags &&flags):
        ViewIface(name, std::move(flags), view_manager),
        ViewSerializeBase(on_screen_name, ViewID::MESSAGE),
        play_view_(nullptr),
        default_audio_source_name_(audio_source_name)
    {}

    virtual ~Base() {}

  public:
    Base(const Base &) = delete;
    Base &operator=(const Base &) = delete;

    bool init() final override { return true; }
    bool late_init() override;
    void focus() final override {}
    void defocus() final override {}

    bool register_audio_sources() final override;

    virtual const Player::LocalPermissionsIface &get_local_permissions() const = 0;

    InputResult process_event(UI::ViewEventID event_id,
                              std::unique_ptr<UI::Parameters> parameters) override
    {
        return InputResult::OK;
    }

    void process_broadcast(UI::BroadcastEventID event_id,
                           UI::Parameters *parameters) final override {}

  protected:
    bool is_serialization_allowed() const override;
    bool write_xml(std::ostream &os, uint32_t bits,
                   const DCP::Queue::Data &data) override;
};

}

#endif /* !VIEW_EXTERNAL_SOURCE_BASE_HH */
