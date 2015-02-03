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

#ifndef VIEW_MANAGER_HH
#define VIEW_MANAGER_HH

#include <map>

#include "view.hh"
#include "dcp_transaction.hh"

/*!
 * \addtogroup view_manager Management of the various views
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
    virtual void set_debug_stream(std::ostream &os) = 0;

    virtual void serialization_result(DcpTransaction::Result result) = 0;

    virtual void input(DrcpCommand command) = 0;
    virtual void input_set_fast_wind_factor(double factor) = 0;
    virtual void input_move_cursor_by_line(int lines) = 0;
    virtual void input_move_cursor_by_page(int pages) = 0;
    virtual ViewIface *get_view_by_name(const char *view_name) = 0;
    virtual void activate_view_by_name(const char *view_name) = 0;
    virtual void toggle_views_by_name(const char *view_name_a,
                                      const char *view_name_b) = 0;
    virtual bool is_active_view(const ViewIface *view) const = 0;
    virtual bool serialize_view_if_active(const ViewIface *view) const = 0;
    virtual bool update_view_if_active(const ViewIface *view) const = 0;
    virtual void hide_view_if_active(const ViewIface *view) = 0;
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
    ViewIface *last_browse_view_;
    DcpTransaction &dcp_transaction_;
    std::ostream *debug_stream_;

  public:
    explicit ViewManager(DcpTransaction &dcpd);

    bool add_view(ViewIface *view) override;
    void set_output_stream(std::ostream &os) override;
    void set_debug_stream(std::ostream &os) override;

    void serialization_result(DcpTransaction::Result result) override;

    void input(DrcpCommand command) override;
    void input_set_fast_wind_factor(double factor) override;
    void input_move_cursor_by_line(int lines) override;
    void input_move_cursor_by_page(int pages) override;
    ViewIface *get_view_by_name(const char *view_name) override;
    void activate_view_by_name(const char *view_name) override;
    void toggle_views_by_name(const char *view_name_a,
                              const char *view_name_b) override;
    bool is_active_view(const ViewIface *view) const override;
    bool serialize_view_if_active(const ViewIface *view) const override;
    bool update_view_if_active(const ViewIface *view) const override;
    void hide_view_if_active(const ViewIface *view) override;

  private:
    void activate_view(ViewIface *view);
    void handle_input_result(ViewIface::InputResult result, ViewIface &view);
};

/*!@}*/

#endif /* !VIEW_MANAGER_HH */
