/*
 * Copyright (C) 2016, 2017  T+A elektroakustik GmbH & Co. KG
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
#include "actor_id.h"

namespace ViewFileBrowser
{

class AirableView: public View
{
  public:
    struct AsyncCallsDecorations
    {
        struct
        {
            unsigned int selected_line_from_root_;
        }
        point_to_child_directory_;

        AsyncCallsDecorations():
            point_to_child_directory_{0}
        {}
    };

    AsyncCallsDecorations async_calls_deco_;

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
                         Playlist::CrawlerIface::RecursiveMode default_recursive_mode,
                         Playlist::CrawlerIface::ShuffleMode default_shuffle_mode,
                         ViewManager::VMIface *view_manager):
        View(name, on_screen_name, drcp_browse_id, max_lines, listbroker_id,
             default_recursive_mode, default_shuffle_mode, view_manager)
    {}

    ~AirableView() {}

    InputResult process_event(UI::ViewEventID event_id,
                              std::unique_ptr<const UI::Parameters> parameters) final override;

    bool list_invalidate(ID::List list_id, ID::List replacement_id) final override;

    void logged_out_from_service_notification(const char *service_id,
                                              enum ActorID actor_id);

  protected:
    void cancel_and_delete_all_async_calls() final override;
    void handle_enter_list_event(List::AsyncListIface::OpResult result,
                                 const std::shared_ptr<List::QueryContextEnterList> &ctx) final override;
    bool point_to_child_directory(const SearchParameters *search_parameters = nullptr) final override;
    GoToSearchForm point_to_search_form(List::context_id_t ctx_id) final override;
    void log_out_from_context(List::context_id_t context) final override;

  private:
    void finish_async_point_to_child_directory();
};

}

#endif /* !VIEW_FILEBROWSER_AIRABLE_HH */
