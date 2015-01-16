#ifndef VIEW_SIGNALS_HH
#define VIEW_SIGNALS_HH

#include <inttypes.h>

class ViewIface;

class ViewSignalsIface
{
  protected:
    static constexpr uint16_t signal_display_serialize_request = 1U << 0;
    static constexpr uint16_t signal_display_serialize_pending = 1U << 1;
    static constexpr uint16_t signal_display_update_request    = 1U << 2;
    static constexpr uint16_t signal_display_update_pending    = 1U << 3;
    static constexpr uint16_t signal_request_hide_view         = 1U << 4;

    explicit ViewSignalsIface() {}

  public:
    ViewSignalsIface(const ViewSignalsIface &) = delete;
    ViewSignalsIface &operator=(const ViewSignalsIface &) = delete;

    virtual ~ViewSignalsIface() {}

    /*!
     * Current view has new information to show on the display.
     */
    virtual void request_display_update(ViewIface *view) = 0;

    /*!
     * Current view wants to be hidden.
     */
    virtual void request_hide_view(ViewIface *view) = 0;

    /*!
     * Attempt to fully serialize failed, need to try again later.
     */
    virtual void display_serialize_pending(ViewIface *view) = 0;

    /*!
     * Attempt to partially update failed, need to try again later.
     */
    virtual void display_update_pending(ViewIface *view) = 0;
};

#endif /* !VIEW_SIGNALS_HH */
