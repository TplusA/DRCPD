/*
 * Copyright (C) 2015, 2016  T+A elektroakustik GmbH & Co. KG
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

class MockViewManager: public ViewManager::VMIface, public UI::EventStoreIface
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

    using CheckParametersFn = void (*)(std::unique_ptr<const UI::Parameters> expected_parameters,
                                       std::unique_ptr<const UI::Parameters> actual_parameters);

    void expect_store_event(UI::EventID event_id,
                            std::unique_ptr<const UI::Parameters> parameters = nullptr);
    void expect_store_event_with_callback(UI::EventID event_id,
                                          std::unique_ptr<const UI::Parameters> parameters,
                                          CheckParametersFn check_params_callback);
    void expect_serialization_result(DCP::Transaction::Result result);
    void expect_input_bounce(ViewIface::InputResult retval, UI::ViewEventID event_id, std::unique_ptr<const UI::Parameters> parameters, UI::ViewEventID xform_event_id = UI::ViewEventID::NOP, const char *view_name = nullptr);
    void expect_get_view_by_name(const char *view_name);
    void expect_get_view_by_dbus_proxy(const void *dbus_proxy);
    void expect_sync_activate_view_by_name(const char *view_name);
    void expect_sync_toggle_views_by_name(const char *view_name_a,
                                          const char *view_name_b);

    void store_event(UI::EventID event_id,
                     std::unique_ptr<const UI::Parameters> parameters = nullptr) override;

    bool add_view(ViewIface *view) override;
    bool invoke_late_init_functions() override;
    void set_output_stream(std::ostream &os) override;
    void set_debug_stream(std::ostream &os) override;
    void serialization_result(DCP::Transaction::Result result) override;
    ViewIface::InputResult input_bounce(const ViewManager::InputBouncer &bouncer, UI::ViewEventID event_id, std::unique_ptr<const UI::Parameters> parameters) override;
    ViewIface *get_view_by_name(const char *view_name) override;
    void sync_activate_view_by_name(const char *view_name) override;
    void sync_toggle_views_by_name(const char *view_name_a,
                                   const char *view_name_b) override;
    bool is_active_view(const ViewIface *view) const override;
    void serialize_view_if_active(const ViewIface *view, DCP::Queue::Mode mode) const override;
    void serialize_view_forced(const ViewIface *view, DCP::Queue::Mode mode) const override;
    void update_view_if_active(const ViewIface *view, DCP::Queue::Mode mode) const override;
    void hide_view_if_active(const ViewIface *view) override;
};

#endif /* !MOCK_VIEW_MANAGER_HH */
