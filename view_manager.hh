#ifndef VIEW_MANAGER_HH
#define VIEW_MANAGER_HH

#include <map>

#include "view.hh"

/*!
 * \addtogroup view_manager Management of the various views.
 */
/*!@{*/

class ViewManagerIface
{
  private:
    ViewManagerIface(const ViewManagerIface &);
    ViewManagerIface &operator=(const ViewManagerIface &);

  protected:
    explicit ViewManagerIface() {}

  public:
    virtual ~ViewManagerIface() {}

    virtual bool add_view(ViewIface *view) = 0;
    virtual void set_output_stream(std::ostream &os) = 0;

    virtual void input(DrcpCommand command) = 0;
    virtual void input_set_fast_wind_factor(double factor) = 0;
    virtual void activate_view_by_name(const char *view_name) = 0;
    virtual void toggle_views_by_name(const char *view_name_a,
                                      const char *view_name_b) = 0;
};

class ViewManager: public ViewManagerIface
{
  public:
    typedef std::map<const std::string, ViewIface *> views_container_t;

  private:
    ViewManager(const ViewManager &);
    ViewManager &operator=(const ViewManager &);

    views_container_t all_views_;

    ViewIface *active_view_;
    std::ostream *output_stream_;

  public:
    explicit ViewManager();

    bool add_view(ViewIface *view) override;
    void set_output_stream(std::ostream &os) override;

    void input(DrcpCommand command) override;
    void input_set_fast_wind_factor(double factor) override;
    void activate_view_by_name(const char *view_name) override;
    void toggle_views_by_name(const char *view_name_a,
                              const char *view_name_b) override;
};

/*!@}*/

#endif /* !VIEW_MANAGER_HH */
