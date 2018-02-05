/*
 * Copyright (C) 2015, 2016, 2017, 2018  T+A elektroakustik GmbH & Co. KG
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

#include <memory>
#include <chrono>

#include "ui_events.hh"

namespace ViewManager { class VMIface; }

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

    class Flags
    {
      public:
        static constexpr uint32_t NONE                        = 0;
        static constexpr uint32_t CAN_RETURN_TO_THIS          = 1U << 0;
        static constexpr uint32_t CAN_HIDE                    = 1U << 1;
        static constexpr uint32_t NO_ENFORCED_USER_INTENTIONS = 1U << 2;

        static constexpr uint32_t DEFAULTS = CAN_HIDE;

      private:
        static constexpr uint32_t LAST = NO_ENFORCED_USER_INTENTIONS;
        static constexpr uint32_t VALID_BITS_MASK = ((LAST << 1) - 1);

        const uint32_t flags_;

      public:
        Flags(const Flags &) = delete;
        Flags(Flags &&) = default;
        Flags &operator=(const Flags &) = delete;

        constexpr explicit Flags(uint32_t flags = DEFAULTS):
            flags_(flags & VALID_BITS_MASK)
        {}

        bool is_any_set(uint32_t flags) const
        {
            return (flags_ & flags & VALID_BITS_MASK) != 0;
        }
    };

    const Flags flags_;

  protected:
    ViewManager::VMIface *const view_manager_;

    /*!
     * Common ctor for all views.
     *
     * \param name
     *     Internal name for selection over D-Bus and debugging.
     * \param flags
     *     Any properties specific to this view.
     * \param view_manager
     *     If the view needs to manage other views, then the view manager must
     *     be passed here. Pass \c nullptr if the view does not use the view
     *     manager.
     */
    explicit constexpr ViewIface(const char *name, ViewIface::Flags &&flags,
                                 ViewManager::VMIface *view_manager):
        name_(name),
        flags_(std::move(flags)),
        view_manager_(view_manager)
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
         * to be done by the caller.
         *
         * Attempting to send an update for the view to the client would result
         * in an update XML document without any content.
         */
        OK,

        /*!
         * Something has changed and an update XML document should be sent to
         * the client.
         *
         * The update is not sent in case the view is not the active view.
         */
        UPDATE_NEEDED,

        /*!
         * Something has changed and an XML document must be sent to the
         * client, regardless of view active state.
         */
        FORCE_SERIALIZE,

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
     * More initialization from the view manager, after all views have been
     * added.
     */
    virtual bool late_init() { return true; }

    /*!
     * Code that needs to run when the view is given the focus.
     */
    virtual void focus() = 0;

    /*!
     * Code that needs to run when the focus is taken from the view.
     */
    virtual void defocus() = 0;

    /*!
     * Called when the system language has been changed.
     *
     * This function should drop any cached messages subject to
     * internationalization. It should *not* try to refresh the screen content
     * since this is a task for the view manager.
     *
     * Since for most views nothings to be done, there is a default
     * implementation which does nothing. Those views that need to do anything
     * special must override this function.
     */
    virtual void language_settings_changed_notification() {}

    /*!
     * Process the given event/command.
     *
     * The view handles the given event synchronously and mutates its internal
     * state accordingly. As far as the caller is concerned, any errors go
     * unnoticed. Errors are supposed to be handled by the views themselves.
     */
    virtual InputResult process_event(UI::ViewEventID event_id,
                                      std::unique_ptr<const UI::Parameters> parameters) = 0;

    /*
     * Process broadcast event.
     */
    virtual void process_broadcast(UI::BroadcastEventID event_id,
                                   const UI::Parameters *parameters) = 0;
};

/*!@}*/

#endif /* !VIEW_HH */
