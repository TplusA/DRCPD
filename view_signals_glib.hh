#ifndef VIEW_SIGNALS_GLIB_HH
#define VIEW_SIGNALS_GLIB_HH

#include <glib.h>

#include "view_signals.hh"
#include "view_manager.hh"

class ViewSignalsGLib: public ViewSignalsIface
{
  private:
    enum signal_t
    {
        NONE = 0,
        DISPLAY_UPDATE_REQUEST,
        HIDE_VIEW_REQUEST,
    };

    ViewManagerIface &vm_;

    guint source_id_;
    GMainContext *ctx_;
    ViewIface *view_;
    signal_t signal_;

  public:
    ViewSignalsGLib(const ViewSignalsGLib &) = delete;
    ViewSignalsGLib &operator=(const ViewSignalsGLib &) = delete;

    explicit ViewSignalsGLib(ViewManagerIface &vm):
        vm_(vm),
        source_id_(0),
        ctx_(nullptr),
        view_(nullptr),
        signal_(NONE)
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

  private:
    void reset()
    {
        send(nullptr, NONE);
    }

    void send(ViewIface *view, signal_t sig)
    {
        view_ = view;
        signal_ = sig;
        g_main_context_wakeup(ctx_);
    }
};

#endif /* !VIEW_SIGNALS_GLIB_HH */
