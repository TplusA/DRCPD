#ifndef VIEW_FILEBROWSER_HH
#define VIEW_FILEBROWSER_HH

#include "view.hh"
#include "ramlist.hh"
#include "listnav.hh"

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

  public:
    View(const View &) = delete;

    View &operator=(const View &) = delete;

    explicit View(const std::string &name, unsigned int max_lines):
        ViewIface(name),
        current_list_id_(0),
        item_flags_(&file_list_),
        navigation_(max_lines, item_flags_)
    {}

    bool init() override;

    void focus() override;
    void defocus() override;

    InputResult input(DrcpCommand command) override;

    void serialize(std::ostream &os) override;
    void update(std::ostream &os) override;

  private:
    /*!
     * Load whole root directory into internal list.
     */
    void fill_list_from_root();

    /*!
     * Load whole directory for current list ID into internal list.
     */
    void fill_list_from_current_list_id();
};

};

/*!@}*/

#endif /* !VIEW_FILEBROWSER_HH */
