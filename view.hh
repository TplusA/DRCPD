#ifndef VIEW_HH
#define VIEW_HH

#include <ostream>

#include "dcp_transaction.hh"
#include "drcp_commands.hh"
#include "i18n.h"

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
    const char *const on_screen_name_;
    const char *const drcp_view_id_;
    const uint8_t drcp_screen_id_;

  protected:
    /*!
     * Common ctor for all views.
     *
     * \param name
     *     Internal name for selection over D-Bus and debugging.
     * \param on_screen_name
     *     Name as presented to the user. Should be internationalized;
     *     serialization will push the localized name.
     * \param drcp_view_id
     *     View ID as defined in the DRCP specification ("config", "browse",
     *     "play", etc.).
     * \param drcp_screen_id
     *     Numeric screen ID as defined in DRCP specification.
     */
    explicit constexpr ViewIface(const char *name, const char *on_screen_name,
                                 const char *drcp_view_id,
                                 uint8_t drcp_screen_id):
        name_(name),
        on_screen_name_(on_screen_name),
        drcp_view_id_(drcp_view_id),
        drcp_screen_id_(drcp_screen_id)
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
     *
     * The base class implementation uses #write_xml_begin(), #write_xml(), and
     * #write_xml_end() to write XML output to \p dcpd. The \p debug_os stream
     * is not used. Derived classes may want to override this function to make
     * use of the \p debug_os stream and call the base implementation from the
     * overriding function to still get the XML output.
     *
     * \param dcpd
     *     A transaction object to send the XML to.
     * \param debug_os
     *     An optional debug output stream to see what's going on (not used in
     *     base class implementation).
     */
    virtual void serialize(DcpTransaction &dcpd, std::ostream *debug_os = nullptr)
    {
        do_serialize(dcpd, true);
    }

    /*!
     * Write XML representation of parts of the view that need be updated.
     *
     * This function does the same as #serialize(), but only emits things that
     * have changed.
     */
    virtual void update(DcpTransaction &dcpd, std::ostream *debug_os = nullptr)
    {
        do_serialize(dcpd, false);
    }

  private:
    void do_serialize(DcpTransaction &dcpd, bool is_full_view)
    {
        if(!dcpd.start())
            return;

        if(dcpd.stream() != nullptr &&
           write_xml_begin(*dcpd.stream(), is_full_view) &&
           write_xml(*dcpd.stream(), is_full_view) &&
           write_xml_end(*dcpd.stream(), is_full_view))
        {
            (void)dcpd.commit();
        }
        else
            (void)dcpd.abort();
    }

  protected:
    /*!
     * Start writing XML data, opens view or update tag and some generic tags.
     *
     * \param os
     *     Output stream the XML data is written to.
     * \param is_full_view
     *     Set to true for sending a full view document, to false for sending
     *     an update document.
     *
     * \returns True to keep going, false to abort the transaction.
     */
    virtual bool write_xml_begin(std::ostream &os, bool is_full_view)
    {
        os << "<" << (is_full_view ? "view" : "update") << " name=\""
           << drcp_view_id_ << "\">\n";

        if(is_full_view)
        {
            os << "    <text id=\"title\">" << _(on_screen_name_) << "</text>\n";
            os << "    <text id=\"scrid\">" << int(drcp_screen_id_) << "</text>\n";
        }

        return true;
    }

    /*!
     * Write the view-specific XML body.
     *
     * Most deriving classes will want to override this function. The base
     * implementation does not write anything to the output stream.
     *
     * \returns True to keep going, false to abort the transaction.
     */
    virtual bool write_xml(std::ostream &os, bool is_full_view)
    {
        return true;
    }

    /*!
     * End writing XML, close view or update tag opened by #write_xml_begin().
     *
     * \returns True to keep going, false to abort the transaction.
     */
    virtual bool write_xml_end(std::ostream &os, bool is_full_view)
    {
        os << "</" << (is_full_view ? "view" : "update") << ">";
        return true;
    }
};

/*!@}*/

#endif /* !VIEW_HH */
