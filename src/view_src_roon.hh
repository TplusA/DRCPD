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

#ifndef VIEW_SRC_ROON_HH
#define VIEW_SRC_ROON_HH

#include "view_external_source_base.hh"
#include "view_names.hh"

namespace ViewSourceRoon
{

class View: public ViewExternalSource::Base
{
  public:
    View(const View &) = delete;
    View &operator=(const View &) = delete;

    explicit View(const char *on_screen_name, ViewManager::VMIface *view_manager):
        Base(ViewNames::ROON, on_screen_name, "roon", view_manager)
    {}

    const Player::LocalPermissionsIface &get_local_permissions() const final override;
};

}

#endif /* !VIEW_SRC_ROON_HH */