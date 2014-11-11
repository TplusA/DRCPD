#ifndef VIEW_NOP_HH
#define VIEW_NOP_HH

#include "view.hh"

/*!
 * \addtogroup view_nop Dummy view
 * \ingroup views
 *
 * A view without any functionality.
 *
 * This view was implemented to avoid the need to handle null pointers in
 * various locations.
 */
/*!@{*/

namespace ViewNop
{

class View: public ViewIface
{
  public:
    View(const View &) = delete;
    View &operator=(const View &) = delete;

    explicit View(): ViewIface("#NOP") {}

    bool init() override { return true; }
    void focus() override {}
    void defocus() override {}
    InputResult input(DrcpCommand command) override { return InputResult::SHOULD_HIDE; }
    void serialize(std::ostream &os, std::ostream *debug_os) override {}
    void update(std::ostream &os, std::ostream *debug_os) override {}
};

};

/*!@}*/

#endif /* !VIEW_NOP_HH */
