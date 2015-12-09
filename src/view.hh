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

#ifndef VIEW_HH
#define VIEW_HH

#include <ostream>
#include <chrono>

#include "dcp_transaction.hh"
#include "drcp_commands.hh"
#include "view_signals.hh"
#include "i18n.h"
#include "xmlescape.hh"

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
  public:
    const char *const name_;
    const char *const on_screen_name_;
    const char *const drcp_view_id_;
    const uint8_t drcp_screen_id_;
    const bool is_browse_view_;

  protected:
    ViewSignalsIface *const view_signals_;

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
     * \param is_browse_view
     *     True if the view is a content browser, false otherwise.
     * \param view_signals
     *     Object that should be notified in case a view needs to communicate
     *     some information.
     */
    explicit constexpr ViewIface(const char *name, const char *on_screen_name,
                                 const char *drcp_view_id,
                                 uint8_t drcp_screen_id, bool is_browse_view,
                                 ViewSignalsIface *view_signals):
        name_(name),
        on_screen_name_(on_screen_name),
        drcp_view_id_(drcp_view_id),
        drcp_screen_id_(drcp_screen_id),
        is_browse_view_(is_browse_view),
        view_signals_(view_signals)
    {}

  public:
    ViewIface(const ViewIface &) = delete;
    ViewIface &operator=(const ViewIface &) = delete;

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
     *
     * \returns
     *     True if the serialization transaction could be started, false if
     *     another transaction was already in progress and the serialization
     *     must be tried again at some later point.
     */
    virtual bool serialize(DcpTransaction &dcpd, std::ostream *debug_os = nullptr)
    {
        return do_serialize(dcpd, true);
    }

    /*!
     * Write XML representation of parts of the view that need be updated.
     *
     * This function does the same as #serialize(), but only emits things that
     * have changed.
     */
    virtual bool update(DcpTransaction &dcpd, std::ostream *debug_os = nullptr)
    {
        return do_serialize(dcpd, false);
    }

    /*!
     * Called when a stream has started playing.
     */
    virtual void notify_stream_start(uint32_t id, bool url_fifo_is_full) {}

    /*!
     * Called when streamplayer has stopped playing.
     */
    virtual void notify_stream_stop() {}

    /*!
     * Called when the playing stream is paused.
     */
    virtual void notify_stream_pause() {}

    /*!
     * Called when a stream position has changed.
     */
    virtual void notify_stream_position_changed(const std::chrono::milliseconds &position,
                                                const std::chrono::milliseconds &duration)
    {}

    virtual void meta_data_add_begin(bool is_update) {}
    virtual void meta_data_add(const char *key, const char *value) {}
    virtual void meta_data_add_end() {}

  private:
    bool do_serialize(DcpTransaction &dcpd, bool is_full_view)
    {
        if(!dcpd.start())
        {
            if(is_full_view)
                view_signals_->display_serialize_pending(this);
            else
                view_signals_->display_update_pending(this);

            return false;
        }

        if(dcpd.stream() != nullptr &&
           write_xml_begin(*dcpd.stream(), is_full_view) &&
           write_xml(*dcpd.stream(), is_full_view) &&
           write_xml_end(*dcpd.stream(), is_full_view))
        {
            (void)dcpd.commit();
        }
        else
            (void)dcpd.abort();

        return true;
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
        os << "<" << (is_full_view ? "view" : "update") << " id=\""
           << drcp_view_id_ << "\">\n";

        if(is_full_view)
        {
            os << "    <text id=\"title\">" << XmlEscape(_(on_screen_name_)) << "</text>\n";
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