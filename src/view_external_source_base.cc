/*
 * Copyright (C) 2017, 2019  T+A elektroakustik GmbH & Co. KG
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
    new_audio_source(default_audio_source_name_, nullptr);
    select_audio_source(0);

    auto *const pview = static_cast<ViewPlay::View *>(play_view_);
    pview->register_audio_source(get_audio_source_by_index(0), *this);

    return true;
}

bool ViewExternalSource::Base::write_xml(std::ostream &os, uint32_t bits,
                                         const DCP::Queue::Data &data)
{
    os << "<text id=\"line0\">" << XmlEscape(_(on_screen_name_)) << "</text>";
    return true;
}
