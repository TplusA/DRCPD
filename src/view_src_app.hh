/*
 * Copyright (C) 2017, 2018  T+A elektroakustik GmbH & Co. KG
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

    explicit View(const char *on_screen_name, ViewManager::VMIface *view_manager):
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
