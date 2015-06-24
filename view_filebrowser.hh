/*
 * Copyright (C) 2015  T+A elektroakustik GmbH & Co. KG
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

#include "view.hh"
#include "playbackmode_state.hh"
#include "dbuslist.hh"
#include "dbus_iface.h"
#include "dbus_iface_deep.h"
#include "idtypes.hh"

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

List::Item *construct_file_item(const char *name, bool is_directory);

class View: public ViewIface
{
  private:
    ID::List current_list_id_;

    List::DBusList file_list_;
    List::NavItemNoFilter item_flags_;
    List::Nav navigation_;

    const uint8_t drcp_browse_id_;

    Playback::CurrentMode playback_current_mode_;
    Playback::State playback_current_state_;

  public:
    View(const View &) = delete;

    View &operator=(const View &) = delete;

    explicit View(const char *name, const char *on_screen_name,
                  uint8_t drcp_browse_id, unsigned int max_lines,
                  dbus_listbroker_id_t listbroker_id,
                  Playback::Mode default_playback_mode,
                  ViewSignalsIface *view_signals):
        ViewIface(name, on_screen_name, "browse", 102U, true, view_signals),
        current_list_id_(0),
        file_list_(dbus_get_lists_navigation_iface(listbroker_id), max_lines,
                   construct_file_item),
        item_flags_(&file_list_),
        navigation_(max_lines, item_flags_),
        drcp_browse_id_(drcp_browse_id),
        playback_current_mode_(default_playback_mode),
        playback_current_state_(&file_list_, playback_current_mode_)
    {}

    bool init() override;

    void focus() override;
    void defocus() override;

    InputResult input(DrcpCommand command) override;

    bool serialize(DcpTransaction &dcpd, std::ostream *debug_os) override;
    bool update(DcpTransaction &dcpd, std::ostream *debug_os) override;

    void notify_stream_start(uint32_t id, const std::string &url,
                             bool url_fifo_is_full) override;
    void notify_stream_stop() override;

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
    bool point_to_child_directory();

    /*!
     * Load whole parent directory into internal list.
     *
     * \returns
     *     True if the list was updated, false if the list remained unchanged.
     */
    bool point_to_parent_link();

    /*!
     * Generate XML document from current state.
     */
    bool write_xml(std::ostream &os, bool is_full_view) override;
};

class FileItem: public List::TextItem
{
  private:
    bool is_directory_;

  public:
    FileItem(const FileItem &) = delete;
    FileItem &operator=(const FileItem &) = delete;
    explicit FileItem(FileItem &&) = default;

    explicit FileItem(const char *text, unsigned int flags,
                      bool item_is_directory):
        List::Item(flags),
        List::TextItem(text, true, flags),
        is_directory_(item_is_directory)
    {}

    bool is_directory() const
    {
        return is_directory_;
    }
};

};

/*!@}*/

#endif /* !VIEW_FILEBROWSER_HH */
