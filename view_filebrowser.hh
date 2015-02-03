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
#include "ramlist.hh"
#include "listnav.hh"
#include "dbus_iface.h"

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

class View: public ViewIface
{
  private:
    uint32_t current_list_id_;

    List::RamList file_list_;
    List::NavItemNoFilter item_flags_;
    List::Nav navigation_;

    const uint8_t drcp_browse_id_;
    const dbus_listbroker_id_t listbroker_id_;

  public:
    View(const View &) = delete;

    View &operator=(const View &) = delete;

    explicit View(const char *name, const char *on_screen_name,
                  uint8_t drcp_browse_id, unsigned int max_lines,
                  dbus_listbroker_id_t listbroker_id,
                  ViewSignalsIface *view_signals):
        ViewIface(name, on_screen_name, "browse", 102U, true, view_signals),
        current_list_id_(0),
        item_flags_(&file_list_),
        navigation_(max_lines, item_flags_),
        drcp_browse_id_(drcp_browse_id),
        listbroker_id_(listbroker_id)
    {}

    bool init() override;

    void focus() override;
    void defocus() override;

    InputResult input(DrcpCommand command) override;

    bool serialize(DcpTransaction &dcpd, std::ostream *debug_os) override;
    bool update(DcpTransaction &dcpd, std::ostream *debug_os) override;

  private:
    /*!
     * Load whole root directory into internal list.
     *
     * \returns
     *     True on success, false on error. In any case the list will have been
     *     modified (empty on error).
     */
    bool fill_list_from_root();

    /*!
     * Load whole directory for current list ID into internal list.
     *
     * \returns
     *     True on success, false on error. In any case the list will have been
     *     modified (empty on error).
     */
    bool fill_list_from_current_list_id();

    /*!
     * Load whole selected subdirectory into internal list.
     *
     * \returns
     *     True if the list was updated, false if the list remained unchanged.
     */
    bool fill_list_from_selected_line();

    /*!
     * Load whole parent directory into internal list.
     *
     * \returns
     *     True if the list was updated, false if the list remained unchanged.
     */
    bool fill_list_from_parent_link();

    /*!
     * Generate XML document from current state.
     */
    bool write_xml(std::ostream &os, bool is_full_view) override;
};

};

/*!@}*/

#endif /* !VIEW_FILEBROWSER_HH */
