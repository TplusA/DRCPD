/*
 * Copyright (C) 2015, 2016, 2017  T+A elektroakustik GmbH & Co. KG
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

#ifndef VIEW_MOCK_HH
#define VIEW_MOCK_HH

#include "view.hh"
#include "view_serialize.hh"
#include "mock_expectation.hh"

namespace ViewMock
{

class View: public ViewIface, public ViewSerializeBase
{
  public:
    class Expectation;
    typedef MockExpectationsTemplate<Expectation> MockExpectations;
    MockExpectations *expectations_;

    bool ignore_all_;

    View(const View &) = delete;
    View &operator=(const View &) = delete;

    explicit View(const char *name, bool is_browse_view);
    ~View();

    void check() const;

    using CheckViewEventParametersFn =
        void (*)(std::unique_ptr<const UI::Parameters> expected_parameters,
                 std::unique_ptr<const UI::Parameters> actual_parameters);
    using CheckBroadcastEventParametersFn =
        void (*)(std::unique_ptr<const UI::Parameters> expected_parameters,
                 const UI::Parameters *actual_parameters);

    void expect_focus();
    void expect_defocus();
    void expect_process_event(InputResult retval, UI::ViewEventID event_id,
                              bool expect_parameters);
    void expect_process_event_with_callback(InputResult retval, UI::ViewEventID event_id,
                                            std::unique_ptr<const UI::Parameters> expected_parameters,
                                            CheckViewEventParametersFn check_params_callback);
    void expect_process_broadcast(UI::BroadcastEventID event_id,
                                  bool expect_parameters);
    void expect_process_broadcast_with_callback(UI::BroadcastEventID event_id,
                                                std::unique_ptr<const UI::Parameters> expected_parameters,
                                                CheckBroadcastEventParametersFn check_params_callback);
    void expect_serialize(std::ostream &os);
    void expect_update(std::ostream &os);
    void expect_write_xml_begin(bool retval, bool is_full_view);

    bool init() override;
    void focus() override;
    void defocus() override;
    InputResult process_event(UI::ViewEventID event_id,
                              std::unique_ptr<const UI::Parameters> parameters) override;
    void process_broadcast(UI::BroadcastEventID event_id,
                           const UI::Parameters *parameters) override;
    void serialize(DCP::Queue &queue, DCP::Queue::Mode mode, std::ostream *debug_os) override;
    void update(DCP::Queue &queue, DCP::Queue::Mode mode, std::ostream *debug_os) override;

  protected:
    bool write_xml_begin(std::ostream &os, uint32_t bits, const DCP::Queue::Data &data_full_view) override;
    bool write_xml(std::ostream &os, uint32_t bits, const DCP::Queue::Data &data_full_view) override;
    bool write_xml_end(std::ostream &os, uint32_t bits, const DCP::Queue::Data &data_full_view) override;
};

};

#endif /* !VIEW_MOCK_HH */
