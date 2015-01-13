#ifndef VIEW_SIGNALS_GLIB_HH
#define VIEW_SIGNALS_GLIB_HH

#include <glib.h>

#include "view_signals.hh"
#include "view_manager.hh"

class ViewSignalsGLib: public ViewSignalsIface
{
  private:
    const ViewManagerIface &vm_;

    guint source_id_;
    GMainContext *ctx_;
    ViewIface *is_display_update_needed_for_view_;

  public:
    ViewSignalsGLib(const ViewSignalsGLib &) = delete;
    ViewSignalsGLib &operator=(const ViewSignalsGLib &) = delete;

    explicit ViewSignalsGLib(ViewManagerIface &vm):
        vm_(vm),
        source_id_(0),
        ctx_(nullptr),
        is_display_update_needed_for_view_(nullptr)
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
};

#endif /* !VIEW_SIGNALS_GLIB_HH */
