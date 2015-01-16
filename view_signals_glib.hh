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
