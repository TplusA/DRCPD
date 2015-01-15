#ifndef VIEW_SIGNALS_HH
#define VIEW_SIGNALS_HH

class ViewIface;

class ViewSignalsIface
{
  protected:
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
};

#endif /* !VIEW_SIGNALS_HH */
