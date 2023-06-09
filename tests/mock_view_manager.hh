/*
 * Copyright (C) 2015--2020  T+A elektroakustik GmbH & Co. KG
 *
 * This file is part of DRCPD.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301, USA.
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
    void expect_sync_activate_view_by_name(const char *view_name,
                                           bool enforce_reactivation);
    void expect_sync_toggle_views_by_name(const char *view_name_a,
                                          const char *view_name_b,
                                          bool enforce_reactivation);

    void store_event(UI::EventID event_id,
                     std::unique_ptr<UI::Parameters> parameters = nullptr) override;

    bool add_view(ViewIface &view) override;
    bool invoke_late_init_functions() override;
    void set_output_stream(std::ostream &os) override;
    void set_debug_stream(std::ostream &os) override;
    void set_resume_playback_configuration_file(const char *filename) override;
    void deselected_notification() override;
    void shutdown() override;
    const char *get_resume_url_by_audio_source_id(const std::string &id) const override;
    std::string move_resume_url_by_audio_source_id(const std::string &id) override;
    void serialization_result(DCP::Transaction::Result result) override;
    ViewIface::InputResult input_bounce(const ViewManager::InputBouncer &bouncer, UI::ViewEventID event_id, std::unique_ptr<UI::Parameters> parameters) override;
    ViewIface *get_view_by_name(const char *view_name) override;
    void sync_activate_view_by_name(const char *view_name,
                                    bool enforce_reactivation) override;
    void sync_toggle_views_by_name(const char *view_name_a,
                                   const char *view_name_b,
                                   bool enforce_reactivation) override;
    bool is_active_view(const ViewIface *view) const override;
    void serialize_view_if_active(const ViewIface *view, DCP::Queue::Mode mode) const override;
    void serialize_view_forced(const ViewIface *view, DCP::Queue::Mode mode) const override;
    void update_view_if_active(const ViewIface *view, DCP::Queue::Mode mode) const override;
    void hide_view_if_active(const ViewIface *view) override;
    Configuration::ConfigChangedIface &get_config_changer() const override;
    const Configuration::DrcpdValues &get_configuration() const override;
};

#endif /* !MOCK_VIEW_MANAGER_HH */
