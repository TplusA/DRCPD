/*
 * Copyright (C) 2015, 2016  T+A elektroakustik GmbH & Co. KG
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

#ifndef VIEW_SIGNALS_GLIB_HH
#define VIEW_SIGNALS_GLIB_HH

#include <glib.h>

#include "view_signals.hh"
#include "view_manager.hh"

/*!
 * Simple signaling for single view.
 *
 * This class allows a view to wake up the main loop so that pending events can
 * be processed asynchronously, outside of some signal handler or "wrong"
 * thread context.
 */
class ViewSignalsGLib: public ViewSignalsIface
{
  private:
    ViewManagerIface &vm_;

    guint source_id_;
    GMainContext *ctx_;
    ViewIface *view_;
    uint16_t signal_;

  public:
    ViewSignalsGLib(const ViewSignalsGLib &) = delete;
    ViewSignalsGLib &operator=(const ViewSignalsGLib &) = delete;

    explicit ViewSignalsGLib(ViewManagerIface &vm):
        vm_(vm),
        source_id_(0),
        ctx_(nullptr),
        view_(nullptr),
        signal_(0)
    {}

    virtual ~ViewSignalsGLib()
    {
        remove_from_main_loop();
    }

    /*!
     * Add GSource to the given loop that we are supposed to wake up.
     */
    void connect_to_main_loop(GMainLoop *loop);

    /*!
     * Disconnect from main loop.
     */
    void remove_from_main_loop();

    /*!
     * Check #ViewSignalsGLib::signal_ for pending events.
     *
     * \returns
     *     True if any signal was posted through the #ViewSignalsGLib API,
     *     false otherwise.
     */
    bool check() const;

    /*!
     * Process all pending events.
     */
    void dispatch();

    /*!
     * Event: Given view should be updated if active.
     *
     * This is a partial update, not full serialization.
     */
    void request_display_update(ViewIface *view) override;

    /*!
     * Event: Given view should be hidden if active.
     */
    void request_hide_view(ViewIface *view) override;

    /*!
     * Event: Given view should be updated.
     *
     * The given view must be active when this function is called.
     * This is a partial update, not full serialization.
     */
    void display_update_pending(ViewIface *view) override;

    /*!
     * Event: Given view should be fully serialized.
     *
     * The given view must be active when this function is called.
     */
    void display_serialize_pending(ViewIface *view) override;

  private:
    void reset()
    {
        send(nullptr, 0);
    }

    void send(ViewIface *view, uint16_t sig)
    {
        if(view != view_ || view == nullptr)
            signal_ = 0;

        view_ = view;
        signal_ |= sig;
        g_main_context_wakeup(ctx_);
    }
};

#endif /* !VIEW_SIGNALS_GLIB_HH */
