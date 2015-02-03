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

#ifndef MOCK_VIEW_MANAGER_HH
#define MOCK_VIEW_MANAGER_HH

#include "view_manager.hh"
#include "mock_expectation.hh"

class MockViewManager: public ViewManagerIface
{
  private:
    MockViewManager(const MockViewManager &);
    MockViewManager &operator=(const MockViewManager &);

  public:
    class Expectation;
    typedef MockExpectationsTemplate<Expectation> MockExpectations;
    MockExpectations *expectations_;

    explicit MockViewManager();
    ~MockViewManager();

    void init();
    void check() const;

    void expect_serialization_result(DcpTransaction::Result result);
    void expect_input(DrcpCommand command);
    void expect_input_set_fast_wind_factor(double factor);
    void expect_input_move_cursor_by_line(int lines);
    void expect_input_move_cursor_by_page(int pages);
    void expect_get_view_by_name(const char *view_name);
    void expect_activate_view_by_name(const char *view_name);
    void expect_toggle_views_by_name(const char *view_name_a,
                                     const char *view_name_b);

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
};

#endif /* !MOCK_VIEW_MANAGER_HH */
