/*
 * Copyright (C) 2016--2023  T+A elektroakustik GmbH & Co. KG
 *
 * This file is part of DRCPD.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301, USA.
 */

#ifndef VIEW_SERIALIZE_HH
#define VIEW_SERIALIZE_HH

#include <ostream>
#include <atomic>
#include <array>

#include "screen_ids.hh"
#include "dcp_transaction_queue.hh"
#include "busy.hh"
#include "maybe.hh"
#include "i18n.hh"
#include "i18nstring.hh"
#include "xmlescape.hh"
#include "guard.hh"

/* open up a bit for unit tests */
namespace ViewMock { class View; }

class ViewSerializeBase
{
  public:
    enum class ViewID
    {
        BROWSE,
        PLAY,
        EDIT,
        MESSAGE,
        ERROR,

        LAST_VIEW_ID = ERROR,

        INVALID,
    };

    const char *const on_screen_name_;
    const ViewID drcp_view_id_;

  private:
    /*!
     * View-specific flags that tell the view what exactly it should update.
     */
    uint32_t update_flags_;
    I18n::String dynamic_title_;
    std::atomic_bool is_serializing_;

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
     *     Default view ID for this view.
     */
    explicit ViewSerializeBase(const char *on_screen_name, ViewID drcp_view_id):
        on_screen_name_(on_screen_name),
        drcp_view_id_(drcp_view_id),
        update_flags_(0),
        dynamic_title_(false),
        is_serializing_(false)
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
     * \param is_busy
     *     The currently busy flag state, if changed. Pass an unknown value if
     *     the busy state has not changed or is unknown to the caller.
     */
    virtual void serialize(DCP::Queue &queue, DCP::Queue::Mode mode,
                           std::ostream *debug_os,
                           const Maybe<bool> &is_busy = Maybe<bool>())
    {
        do_serialize(queue, mode, true, is_busy);
    }

    /*!
     * Write XML representation of parts of the view that need be updated.
     *
     * This function does the same as #serialize(), but only emits things that
     * have changed.
     */
    virtual void update(DCP::Queue &queue, DCP::Queue::Mode mode,
                        std::ostream *debug_os,
                        const Maybe<bool> &is_busy = Maybe<bool>())
    {
        do_serialize(queue, mode, false, is_busy);
    }

    bool write_whole_xml(std::ostream &os, const DCP::Queue::Data &data)
    {
        if(!is_serialization_allowed())
            return false;

        const uint32_t bits = about_to_write_xml(data);
        bool busy_state_triggered = false;

        return (write_xml_begin(os, bits, data) &&
                write_xml(os, bits, data, busy_state_triggered) &&
                write_xml_end(os, bits, data, busy_state_triggered));
    }

    virtual void set_dynamic_title(const I18n::String &t) { dynamic_title_ = t; }
    virtual void set_dynamic_title(const char *t)         { dynamic_title_ = t; }
    virtual void set_dynamic_title(I18n::String &&t)      { dynamic_title_ = std::move(t); }
    virtual void clear_dynamic_title()                    { dynamic_title_.clear(); }

    const I18n::String &get_dynamic_title() const { return dynamic_title_; }

    bool is_serializing() const { return is_serializing_; }

  protected:
    virtual bool is_serialization_allowed() const = 0;

    virtual uint32_t about_to_write_xml(const DCP::Queue::Data &data) const
    {
        return 0;
    }

    virtual std::pair<const ViewID, const ScreenID::id_t>
    get_dynamic_ids(uint32_t bits) const
    {
        return std::make_pair(drcp_view_id_, ScreenID::INVALID_ID);
    }

    /*!
     * Start writing XML data, opens view or update tag and some generic tags.
     *
     * \param os
     *     Output stream the XML data is written to.
     * \param bits
     *     Flags returned by #ViewSerializeBase::about_to_write_xml().
     * \param data
     *     Collected data about the serialization action.
     *
     * \returns True to keep going, false to abort the transaction.
     */
    virtual bool write_xml_begin(std::ostream &os, uint32_t bits,
                                 const DCP::Queue::Data &data)
    {
        static constexpr std::array<const char *const, size_t(ViewID::LAST_VIEW_ID) + 1> idnames
        {
            "browse", "play", "edit", "msg", "error",
        };

        const auto ids(get_dynamic_ids(bits));

        msg_log_assert(ids.first <= ViewID::LAST_VIEW_ID);

        os << "<" << (data.is_full_serialize_ ? "view" : "update") << " id=\""
           << idnames[size_t(ids.first)] << "\">";

        if(data.is_full_serialize_)
        {
            if(ids.first != ViewID::ERROR)
                os << "<text id=\"title\">"
                   << (get_dynamic_title().empty()
                       ? XmlEscape(_(on_screen_name_))
                       : XmlEscape(get_dynamic_title().get_text()))
                   << "</text>";

            if(ids.second != ScreenID::INVALID_ID)
                os << "<text id=\"scrid\">" << ids.second << "</text>";
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
    virtual bool write_xml(std::ostream &os, uint32_t bits,
                           const DCP::Queue::Data &data, bool &busy_state_triggered)
    {
        return true;
    }

    static void append_busy_value(std::ostream &os, const Maybe<bool> &busy_flag,
                                  bool busy_state_triggered)
    {
        if(!busy_state_triggered)
        {
            if(busy_flag.is_known())
                os << "<value id=\"busy\">" << (busy_flag == true ? '1' : '0') << "</value>";
        }
        else if(!(busy_flag == true))
            os << "<value id=\"busy\">1</value>";
    }

    /*!
     * End writing XML, close view or update tag opened by #write_xml_begin().
     *
     * \returns True to keep going, false to abort the transaction.
     */
    virtual bool write_xml_end(std::ostream &os, uint32_t bits,
                               const DCP::Queue::Data &data, bool busy_state_triggered)
    {
        append_busy_value(os, data.busy_flag_, busy_state_triggered);
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

    void serialize_begin()
    {
        MSG_BUG_IF(is_serializing_, "Already serializing");
        is_serializing_ = true;
    }

    void serialize_end()
    {
        MSG_BUG_IF(!is_serializing_, "Not serializing");
        is_serializing_ = false;
    }

  private:
    bool do_serialize(DCP::Queue &queue, DCP::Queue::Mode mode,
                      bool is_full_view, const Maybe<bool> &is_busy)
    {
        if(is_serializing())
            return false;

        serialize_begin();
        const Guard end([this] { serialize_end(); });

        queue.add(this, is_full_view, update_flags_, is_busy);
        update_flags_ = 0;
        return queue.start_transaction(mode);
    }

  public:
    class InternalDoSerialize
    {
      public:
        static inline bool do_serialize(ViewSerializeBase &view,
                                        DCP::Queue &queue, bool is_full_view,
                                        const Maybe<bool> &is_busy)
        {
            return view.do_serialize(queue, DCP::Queue::Mode::SYNC_IF_POSSIBLE,
                                     is_full_view, is_busy);
        }

        friend class ViewMock::View;
    };
};

#endif /* !VIEW_SERIALIZE_HH */
