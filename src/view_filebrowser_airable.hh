/*
 * Copyright (C) 2016  T+A elektroakustik GmbH & Co. KG
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

#ifndef VIEW_FILEBROWSER_AIRABLE_HH
#define VIEW_FILEBROWSER_AIRABLE_HH

#include <map>

#include "view_filebrowser.hh"
#include "context_map.hh"

namespace ViewFileBrowser
{

class AirableView: public View
{
  private:
    ID::List root_list_id_;

    /* collection of search form items found so far */
    std::map<List::context_id_t, std::pair<unsigned int, unsigned int>> search_forms_;

  public:
    AirableView(const AirableView &) = delete;
    AirableView &operator=(const AirableView &) = delete;

    explicit AirableView(const char *name, const char *on_screen_name,
                         uint8_t drcp_browse_id, unsigned int max_lines,
                         dbus_listbroker_id_t listbroker_id,
                         Playback::Player &player,
                         Playback::Mode default_playback_mode,
                         ViewManager::VMIface *view_manager):
        View(name, on_screen_name, drcp_browse_id, max_lines, listbroker_id,
             player, default_playback_mode, view_manager)
    {}

    ~AirableView() {}

  private:
    bool point_to_root_directory() final override;
    bool point_to_child_directory(const SearchParameters *search_parameters = nullptr) final override;
    GoToSearchForm point_to_search_form(List::context_id_t ctx_id) final override;
};

}

#endif /* !VIEW_FILEBROWSER_AIRABLE_HH */
