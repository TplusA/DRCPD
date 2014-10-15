#ifndef VIEW_MANAGER_HH
#define VIEW_MANAGER_HH

#include "drcp_commands.hh"

class ViewManagerIface
{
  private:
    ViewManagerIface(const ViewManagerIface &);
    ViewManagerIface &operator=(const ViewManagerIface &);

  protected:
    explicit ViewManagerIface() {}

  public:
    virtual ~ViewManagerIface() {}

    virtual void input(DrcpCommand command) = 0;
    virtual void input_set_fast_wind_factor(double factor) = 0;
    virtual void activate_view_by_name(const char *view_name) = 0;
    virtual void toggle_views_by_name(const char *view_name_a,
                                      const char *view_name_b) = 0;
};

class ViewManager: public ViewManagerIface
{
  private:
    ViewManager(const ViewManager &);
    ViewManager &operator=(const ViewManager &);

  public:
    explicit ViewManager() {}

    void input(DrcpCommand command) override;
    void input_set_fast_wind_factor(double factor) override;
    void activate_view_by_name(const char *view_name) override;
    void toggle_views_by_name(const char *view_name_a,
                              const char *view_name_b) override;
};

#endif /* !VIEW_MANAGER_HH */
