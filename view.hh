#ifndef VIEW_HH
#define VIEW_HH

#include <ostream>

#include "drcp_commands.hh"

/*!
 * \addtogroup views Various views with their specific behaviors
 *
 * Also known as "the user interface".
 */
/*!@{*/

/*!
 * Interface shared by all views.
 *
 * All views are concerned with
 * - initialization;
 * - basic input processing;
 * - focus handling; and
 * - serialization to DRCP XML.
 *
 * These concerns are covered by this interface. Anything beyond is defined by
 * the specific views.
 */
class ViewIface
{
  private:
    ViewIface(const ViewIface &);
    ViewIface &operator=(const ViewIface &);

  public:
    const char *const name_;

  protected:
    explicit ViewIface(const char *name):
        name_(name)
    {}

  public:
    /*!
     * How to proceed after processing a DRC command.
     */
    enum class InputResult
    {
        /*!
         * The view should be kept on screen as is, there is nothing that needs
         * to be done by the caller. Attempting send an update to the client
         * would result in an update document without any content.
         */
        OK,

        /*!
         * Something has changed and an update XML document should be sent to
         * the client.
         */
        UPDATE_NEEDED,

        /*!
         * The input has caused the view to close itself. The caller should now
         * pick a different view and show it.
         */
        SHOULD_HIDE,
    };

    virtual ~ViewIface() {}

    /*!
     * Initialization of internal state, if any.
     *
     * This is for stuff that should go into the ctor, such as D-Bus accesses,
     * big memory allocations that may fail, or accessing resources with
     * unknown state at ctor time.
     *
     * \returns True on success, false on error.
     */
    virtual bool init() = 0;

    /*!
     * Code that needs to run when the view is given the focus.
     */
    virtual void focus() = 0;

    /*!
     * Code that needs to run when the focus is taken from the view.
     */
    virtual void defocus() = 0;

    /*!
     * Process given DRC command.
     *
     * The view tries to handle the given command code and mutates its internal
     * state accordingly. As far as the caller is concerned, any errors go
     * unnoticed. Errors are supposed to be handled by the views themselves.
     */
    virtual InputResult input(DrcpCommand command) = 0;

    /*!
     * Write XML representation of the whole view to given output stream.
     */
    virtual void serialize(std::ostream &os, std::ostream *debug_os = nullptr) = 0;

    /*!
     * Write XML representation of parts of the view that need be updated.
     */
    virtual void update(std::ostream &os, std::ostream *debug_os = nullptr) = 0;
};

/*!@}*/

#endif /* !VIEW_HH */
