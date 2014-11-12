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

    const dbus_listbroker_id_t listbroker_id_;

  public:
    View(const View &) = delete;

    View &operator=(const View &) = delete;

    explicit View(const char *name, unsigned int max_lines,
                  dbus_listbroker_id_t listbroker_id):
        ViewIface(name),
        current_list_id_(0),
        item_flags_(&file_list_),
        navigation_(max_lines, item_flags_),
        listbroker_id_(listbroker_id)
    {}

    bool init() override;

    void focus() override;
    void defocus() override;

    InputResult input(DrcpCommand command) override;

    void serialize(DcpTransaction &dcpd, std::ostream *debug_os) override;
    void update(DcpTransaction &dcpd, std::ostream *debug_os) override;

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
};

};

/*!@}*/

#endif /* !VIEW_FILEBROWSER_HH */
