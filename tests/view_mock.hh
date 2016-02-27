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

#ifndef VIEW_MOCK_HH
#define VIEW_MOCK_HH

#include "view.hh"
#include "mock_expectation.hh"

namespace ViewMock
{

class View: public ViewIface
{
  public:
    class Expectation;
    typedef MockExpectationsTemplate<Expectation> MockExpectations;
    MockExpectations *expectations_;

    bool ignore_all_;

    View(const View &) = delete;
    View &operator=(const View &) = delete;

    explicit View(const char *name, bool is_browse_view,
                  ViewSignalsIface *view_signals);
    ~View();

    void check() const;

    using CheckParametersFn = void (*)(const UI::Parameters *expected_parameters,
                                       const UI::Parameters *actual_parameters);

    void expect_focus();
    void expect_defocus();
    void expect_input(InputResult retval, DrcpCommand command,
                      bool expect_parameters);
    void expect_input_with_callback(InputResult retval, DrcpCommand command,
                                    const UI::Parameters *expected_parameters,
                                    CheckParametersFn check_params_callback);
    void expect_serialize(std::ostream &os);
    void expect_update(std::ostream &os);

    bool init() override;
    void focus() override;
    void defocus() override;
    InputResult input(DrcpCommand command,
                      const UI::Parameters *parameters) override;
    bool serialize(DcpTransaction &dcpd, std::ostream *debug_os) override;
    bool update(DcpTransaction &dcpd, std::ostream *debug_os) override;
};

};

#endif /* !VIEW_MOCK_HH */
