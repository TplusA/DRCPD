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

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "view_external_source_base.hh"
#include "view_play.hh"
#include "view_manager.hh"

bool ViewExternalSource::Base::late_init()
{
    if(!ViewIface::late_init())
        return false;

    play_view_ =
        dynamic_cast<ViewPlay::View *>(view_manager_->get_view_by_name(ViewNames::PLAYER));

    if(play_view_ == nullptr)
        return false;

    return register_audio_sources();
}

bool ViewExternalSource::Base::register_audio_sources()
{
    log_assert(default_audio_source_name_ != nullptr);
    new_audio_source(default_audio_source_name_);
    select_audio_source(0);

    auto *const pview = static_cast<ViewPlay::View *>(play_view_);
    pview->register_audio_source(get_audio_source_by_index(0), *this);

    return true;
}

ViewIface::InputResult
ViewExternalSource::Base::process_event(UI::ViewEventID event_id,
                                        std::unique_ptr<const UI::Parameters> parameters)
{
    MSG_TRACE();
    return InputResult::OK;
}

void ViewExternalSource::Base::process_broadcast(UI::BroadcastEventID event_id,
                                                 const UI::Parameters *parameters)
{
    MSG_TRACE();
}
