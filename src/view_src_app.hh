/*
 * Copyright (C) 2017, 2018, 2019, 2020  T+A elektroakustik GmbH & Co. KG
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

#ifndef VIEW_SRC_APP_HH
#define VIEW_SRC_APP_HH

#include "view_external_source_base.hh"
#include "view_names.hh"

namespace ViewSourceApp
{

class View: public ViewExternalSource::Base
{
  public:
    View(const View &) = delete;
    View &operator=(const View &) = delete;

    explicit View(const char *on_screen_name, ViewManager::VMIface &view_manager):
        Base(ViewNames::APP, on_screen_name, "strbo.plainurl", view_manager,
             ViewIface::Flags(ViewIface::Flags::CAN_RETURN_TO_THIS |
                              ViewIface::Flags::IS_PASSIVE))
    {}

    const Player::LocalPermissionsIface &get_local_permissions() const final override;

  protected:
    std::string generate_resume_url(const Player::AudioSource &asrc) const final override
    {
        const auto &d(asrc.get_resume_data().plain_url_data_);
        return d.is_set() ? d.get().plain_stream_url_ : "";
    }
};

}

#endif /* !VIEW_SRC_APP_HH */
