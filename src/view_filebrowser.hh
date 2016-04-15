/*
 * Copyright (C) 2015, 2016  T+A elektroakustik GmbH & Co. KG
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

#ifndef VIEW_FILEBROWSER_HH
#define VIEW_FILEBROWSER_HH

#include <memory>

#include "view.hh"
#include "view_serialize.hh"
#include "playbackmode_state.hh"
#include "streaminfo.hh"
#include "search_parameters.hh"
#include "context_map.hh"
#include "timeout.hh"
#include "dbuslist.hh"
#include "dbus_iface.h"
#include "dbus_iface_deep.h"
#include "idtypes.hh"
#include "de_tahifi_lists_item_kinds.hh"

namespace Playback { class Player; }

class WaitForParametersHelper;

/*!
 * \addtogroup view_filesystem Filesystem browsing
 * \ingroup views
 *
 * A browsable tree hierarchy of lists.
 *
 * The lists are usually fed by external list broker processes over D-Bus.
 */
/*!@{*/

namespace ViewFileBrowser
{

List::Item *construct_file_item(const char *name, ListItemKind kind,
                                const char *const *names);

class View: public ViewIface, public ViewSerializeBase
{
  private:
    static constexpr unsigned int assumed_streamplayer_fifo_size = 2;

    List::ContextMap list_contexts_;

    ID::List current_list_id_;

    dbus_listbroker_id_t listbroker_id_;

    /* list for the user */
    List::DBusList file_list_;

    /* list for the automatic directory traversal */
    List::DBusList traversal_list_;

    List::NavItemNoFilter item_flags_;
    List::Nav navigation_;

    const uint8_t drcp_browse_id_;

    Playback::Player &player_;
    bool player_is_mine_;
    Playback::CurrentMode playback_current_mode_;
    Playback::State playback_current_state_;
    Timeout::Timer keep_lists_alive_timeout_;

    ViewIface *search_parameters_view_;
    bool waiting_for_search_parameters_;

  public:
    View(const View &) = delete;

    View &operator=(const View &) = delete;

    explicit View(const char *name, const char *on_screen_name,
                  uint8_t drcp_browse_id, unsigned int max_lines,
                  dbus_listbroker_id_t listbroker_id,
                  Playback::Player &player,
                  Playback::Mode default_playback_mode,
                  ViewManager::VMIface *view_manager):
        ViewIface(name, true, view_manager),
        ViewSerializeBase(on_screen_name, "browse", 102U),
        current_list_id_(0),
        listbroker_id_(listbroker_id),
        file_list_(dbus_get_lists_navigation_iface(listbroker_id_),
                   list_contexts_, max_lines,
                   construct_file_item),
        traversal_list_(dbus_get_lists_navigation_iface(listbroker_id_),
                        list_contexts_, assumed_streamplayer_fifo_size + 1,
                        construct_file_item),
        item_flags_(&file_list_),
        navigation_(max_lines, item_flags_),
        drcp_browse_id_(drcp_browse_id),
        player_(player),
        player_is_mine_(false),
        playback_current_mode_(default_playback_mode),
        playback_current_state_(traversal_list_, playback_current_mode_),
        search_parameters_view_(nullptr),
        waiting_for_search_parameters_(false)
    {}

    bool init() override;
    bool late_init() override;

    void focus() override;
    void defocus() override;

    /*!
     * Query properties of associated list broker.
     *
     * Should be called by #ViewIface::late_init() and whenever the list broker
     * has presumably been restarted.
     */
    bool sync_with_list_broker(bool is_first_call = false);

    InputResult input(DrcpCommand command,
                      std::unique_ptr<const UI::Parameters> parameters) override;

    void serialize(DCP::Queue &queue, std::ostream *debug_os) override;
    void update(DCP::Queue &queue, std::ostream *debug_os) override;

    bool owns_dbus_proxy(const void *dbus_proxy) const;
    bool list_invalidate(ID::List list_id, ID::List replacement_id);

  private:
    /*!
     * Load whole root directory into internal list.
     *
     * \returns
     *     True on success, false on error. In any case the list will have been
     *     modified (empty on error).
     */
    bool point_to_root_directory();

    /*!
     * Load whole selected subdirectory into internal list.
     *
     * \returns
     *     True if the list was updated, false if the list remained unchanged.
     */
    bool point_to_child_directory(const SearchParameters *search_parameters = nullptr);

    /*!
     * Find best matching item in current list and move selection there.
     */
    bool point_to_item(const ViewIface &view, const SearchParameters &search_parameters);

    /*!
     * Load whole parent directory into internal list.
     *
     * \returns
     *     True if the list was updated, false if the list remained unchanged.
     */
    bool point_to_parent_link();

    /*!
     * Reload currently displayed list, try to keep navigation in good shape.
     */
    void reload_list();

    /*!
     * Generate XML document from current state.
     */
    bool write_xml(std::ostream &os, const DCP::Queue::Data &data) override;

    bool wait_for_search_parameters(WaitForParametersHelper &wait_helper, bool via_form);
    bool apply_search_parameters();

    std::chrono::milliseconds keep_lists_alive_timer_callback();
};

class FileItem: public List::TextItem
{
  private:
    ListItemKind kind_;
    PreloadedMetaData preloaded_meta_data_;

  public:
    FileItem(const FileItem &) = delete;
    FileItem &operator=(const FileItem &) = delete;
    explicit FileItem(FileItem &&) = default;

    explicit FileItem(const char *text, unsigned int flags,
                      ListItemKind item_kind, PreloadedMetaData &&meta_data):
        List::Item(flags),
        List::TextItem(text, true, flags),
        kind_(item_kind),
        preloaded_meta_data_(std::move(meta_data))
    {}

    ListItemKind get_kind() const { return kind_; }

    const PreloadedMetaData &get_preloaded_meta_data() const
    {
        return preloaded_meta_data_;
    }
};

};

/*!@}*/

#endif /* !VIEW_FILEBROWSER_HH */
