/*
 * Copyright (C) 2016  T+A elektroakustik GmbH & Co. KG
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

#ifndef VIEW_SERIALIZE_HH
#define VIEW_SERIALIZE_HH

#include <ostream>

#include "dcp_transaction_queue.hh"
#include "busy.hh"
#include "i18n.h"
#include "xmlescape.hh"

/* open up a bit for unit tests */
namespace ViewMock { class View; }

class ViewSerializeBase
{
  public:
    /*!
     * Reserved bits for globally used update flags handled by this base class.
     *
     * Views may safely ignore these bits, but they may also rely them if
     * needed.
     */
    static constexpr const uint32_t UPDATE_FLAGS_BASE_MASK = 7U << 29;

    /*!
     * Base update flag: busy flags.
     */
    static constexpr const uint32_t UPDATE_FLAGS_BASE_BUSY_FLAG = 1U << 31;

    const char *const on_screen_name_;
    const char *const drcp_view_id_;
    const uint8_t drcp_screen_id_;

  private:
    /*!
     * View-specific flags that tell the view what exactly it should update.
     */
    uint32_t update_flags_;

  public:
    ViewSerializeBase(const ViewSerializeBase &) = delete;
    ViewSerializeBase &operator=(const ViewSerializeBase &) = delete;

    /*!
     * Common ctor for all serializable views.
     *
     * \param on_screen_name
     *     Name as presented to the user. Should be internationalized;
     *     serialization will push the localized name.
     * \param drcp_view_id
     *     View ID as defined in the DRCP specification ("config", "browse",
     *     "play", etc.).
     * \param drcp_screen_id
     *     Numeric screen ID as defined in DRCP specification.
     */
    explicit ViewSerializeBase(const char *on_screen_name,
                               const char *drcp_view_id,
                               uint8_t drcp_screen_id):
        on_screen_name_(on_screen_name),
        drcp_view_id_(drcp_view_id),
        drcp_screen_id_(drcp_screen_id),
        update_flags_(0)
    {}

    virtual ~ViewSerializeBase() {}

    /*!
     * Write XML representation of the whole view to given output stream.
     *
     * The base class implementation uses #write_xml_begin(), #write_xml(), and
     * #write_xml_end() to write XML output to \p dcpd. The \p debug_os stream
     * is not used. Derived classes may want to override this function to make
     * use of the \p debug_os stream and call the base implementation from the
     * overriding function to still get the XML output.
     *
     * \param queue
     *     A transaction queue object to send the XML to.
     * \param mode
     *     DCP queuing mode.
     * \param debug_os
     *     An optional debug output stream to see what's going on (not used in
     *     base class implementation).
     */
    virtual void serialize(DCP::Queue &queue, DCP::Queue::Mode mode,
                           std::ostream *debug_os = nullptr)
    {
        do_serialize(queue, mode, true);
    }

    /*!
     * Write XML representation of parts of the view that need be updated.
     *
     * This function does the same as #serialize(), but only emits things that
     * have changed.
     */
    virtual void update(DCP::Queue &queue, DCP::Queue::Mode mode,
                        std::ostream *debug_os = nullptr)
    {
        do_serialize(queue, mode, false);
    }

    bool write_whole_xml(std::ostream &os, const DCP::Queue::Data &data)
    {
        return (write_xml_begin(os, data) &&
                write_xml(os, data) &&
                write_xml_end(os, data));
    }

    void add_base_update_flags(uint32_t flags)
    {
        update_flags_ |= (flags & UPDATE_FLAGS_BASE_MASK);
    }

  protected:
    /*!
     * Start writing XML data, opens view or update tag and some generic tags.
     *
     * \param os
     *     Output stream the XML data is written to.
     * \param data
     *     Collected data about the serialization action.
     *
     * \returns True to keep going, false to abort the transaction.
     */
    virtual bool write_xml_begin(std::ostream &os,
                                 const DCP::Queue::Data &data)
    {
        os << "<" << (data.is_full_serialize_ ? "view" : "update") << " id=\""
           << drcp_view_id_ << "\">";

        os << "<value id=\"busy\">" << (Busy::is_busy() ? '1' : '0') << "</value>";

        if(data.is_full_serialize_)
        {
            os << "<text id=\"title\">" << XmlEscape(_(on_screen_name_)) << "</text>";
            os << "<text id=\"scrid\">" << int(drcp_screen_id_) << "</text>";
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
    virtual bool write_xml(std::ostream &os, const DCP::Queue::Data &data)
    {
        return true;
    }

    /*!
     * End writing XML, close view or update tag opened by #write_xml_begin().
     *
     * \returns True to keep going, false to abort the transaction.
     */
    virtual bool write_xml_end(std::ostream &os, const DCP::Queue::Data &data)
    {
        os << "</" << (data.is_full_serialize_ ? "view" : "update") << ">";
        return true;
    }

    /*!
     * Add view-specific update flags.
     *
     * The flags are OR'ed with previously added flags. When partial
     * serialization takes place, the update flags are stored in the view's
     * queue entry and reset to 0.
     *
     * This function is meant for collecting update events over time until
     * serialization should take place.
     */
    void add_update_flags(uint32_t flags) { update_flags_ |= flags; }

  private:
    bool do_serialize(DCP::Queue &queue, DCP::Queue::Mode mode, bool is_full_view)
    {
        queue.add(this, is_full_view, update_flags_);
        update_flags_ = 0;
        return queue.start_transaction(mode);
    }

  public:
    class InternalDoSerialize
    {
      public:
        static inline bool do_serialize(ViewSerializeBase &view,
                                        DCP::Queue &queue, bool is_full_view)
        {
            return view.do_serialize(queue, DCP::Queue::Mode::SYNC_IF_POSSIBLE,
                                     is_full_view);
        }

        friend class ViewMock::View;
    };
};

#endif /* !VIEW_SERIALIZE_HH */
