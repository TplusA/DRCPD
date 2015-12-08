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

#ifndef VIEW_SIGNALS_GLIB_HH
#define VIEW_SIGNALS_GLIB_HH

#include <glib.h>

#include "view_signals.hh"
#include "view_manager.hh"

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

    void connect_to_main_loop(GMainLoop *loop);
    void remove_from_main_loop();
    bool check() const;
    void dispatch();

    void request_display_update(ViewIface *view) override;
    void request_hide_view(ViewIface *view) override;
    void display_update_pending(ViewIface *view) override;
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
