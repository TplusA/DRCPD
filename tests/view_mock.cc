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

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <cppcutter.h>

#include "view_mock.hh"

enum class MemberFn
{
    focus,
    defocus,
    input,
    serialize,
    update,

    first_valid_member_fn_id = focus,
    last_valid_member_fn_id = update,
};

static std::ostream &operator<<(std::ostream &os, const MemberFn id)
{
    if(id < MemberFn::first_valid_member_fn_id ||
       id > MemberFn::last_valid_member_fn_id)
    {
        os << "INVALID";
        return os;
    }

    switch(id)
    {
      case MemberFn::focus:
        os << "focus";
        break;

      case MemberFn::defocus:
        os << "defocus";
        break;

      case MemberFn::input:
        os << "input";
        break;

      case MemberFn::serialize:
        os << "serialize";
        break;

      case MemberFn::update:
        os << "update";
        break;
    }

    os << "()";

    return os;
}

class ViewMock::View::Expectation
{
  private:
    Expectation(const Expectation &);
    Expectation &operator=(const Expectation &);

  public:
    const MemberFn function_id_;

    const InputResult retval_input_;
    const DrcpCommand arg_command_;
    const bool expect_parameters_;
    const CheckParametersFn check_parameters_fn_;
    const UI::Parameters *const expected_parameters_;

    explicit Expectation(MemberFn id):
        function_id_(id),
        retval_input_(InputResult::OK),
        arg_command_(DrcpCommand::UNDEFINED_COMMAND),
        expect_parameters_(false),
        check_parameters_fn_(nullptr),
        expected_parameters_(nullptr)
    {}

    explicit Expectation(MemberFn id, InputResult retval, DrcpCommand command,
                         bool expect_parameters):
        function_id_(id),
        retval_input_(retval),
        arg_command_(command),
        expect_parameters_(expect_parameters),
        check_parameters_fn_(nullptr),
        expected_parameters_(nullptr)
    {}

    explicit Expectation(MemberFn id, InputResult retval, DrcpCommand command,
                         const UI::Parameters *expected_parameters,
                         CheckParametersFn check_params_callback):
        function_id_(id),
        retval_input_(retval),
        arg_command_(command),
        expect_parameters_(expected_parameters != nullptr),
        check_parameters_fn_(check_params_callback),
        expected_parameters_(expected_parameters)
    {}

    Expectation(Expectation &&) = default;
};

ViewMock::View::View(const char *name, bool is_browse_view,
                     ViewSignalsIface *view_signals):
    ViewIface(name, "The mock view", "mockview", 200U, is_browse_view,
              nullptr, view_signals),
    ignore_all_(false)
{
    expectations_ = new MockExpectations();
}

ViewMock::View::~View()
{
    delete expectations_;
}

bool ViewMock::View::init()
{
    cppcut_assert_not_null(expectations_);
    expectations_->init();
    return true;
}

void ViewMock::View::check() const
{
    cppcut_assert_not_null(expectations_);
    expectations_->check();
}

void ViewMock::View::expect_focus()
{
    expectations_->add(Expectation(MemberFn::focus));
}

void ViewMock::View::expect_defocus()
{
    expectations_->add(Expectation(MemberFn::defocus));
}

void ViewMock::View::expect_input(InputResult retval, DrcpCommand command,
                                  bool expect_parameters)
{
    expectations_->add(Expectation(MemberFn::input, retval, command,
                                   expect_parameters));
}

void ViewMock::View::expect_input_with_callback(InputResult retval, DrcpCommand command,
                                                const UI::Parameters *expected_parameters,
                                                CheckParametersFn check_params_callback)
{
    expectations_->add(Expectation(MemberFn::input, retval, command,
                                   expected_parameters,
                                   check_params_callback));
}

void ViewMock::View::expect_serialize(std::ostream &os)
{
    expectations_->add(Expectation(MemberFn::serialize));
    os << name_ << " serialize\n";
}

void ViewMock::View::expect_update(std::ostream &os)
{
    expectations_->add(Expectation(MemberFn::update));
    os << name_ << " update\n";
}


void ViewMock::View::focus()
{
    if(ignore_all_)
        return;

    const auto &expect(expectations_->get_next_expectation(__func__));

    cppcut_assert_equal(expect.function_id_, MemberFn::focus);
}

void ViewMock::View::defocus()
{
    if(ignore_all_)
        return;

    const auto &expect(expectations_->get_next_expectation(__func__));

    cppcut_assert_equal(expect.function_id_, MemberFn::defocus);
}

ViewIface::InputResult ViewMock::View::input(DrcpCommand command,
                                             std::unique_ptr<const UI::Parameters> parameters)
{
    if(ignore_all_)
        return ViewIface::InputResult::OK;

    const auto &expect(expectations_->get_next_expectation(__func__));

    cppcut_assert_equal(expect.function_id_, MemberFn::input);
    cppcut_assert_equal(int(expect.arg_command_), int(command));

    if(expect.check_parameters_fn_ != nullptr)
        expect.check_parameters_fn_(expect.expected_parameters_, parameters);
    else if(expect.expect_parameters_)
        cppcut_assert_not_null(parameters.get());
    else
        cppcut_assert_null(parameters.get());

    return expect.retval_input_;
}

bool ViewMock::View::serialize(DCP::Transaction &dcpd, std::ostream *debug_os)
{
    if(ignore_all_)
        return true;

    const auto &expect(expectations_->get_next_expectation(__func__));

    cppcut_assert_equal(expect.function_id_, MemberFn::serialize);

    cut_assert_true(dcpd.start());
    cut_assert_true(dcpd.commit());

    return true;
}

bool ViewMock::View::update(DCP::Transaction &dcpd, std::ostream *debug_os)
{
    if(ignore_all_)
        return true;

    const auto &expect(expectations_->get_next_expectation(__func__));

    cppcut_assert_equal(expect.function_id_, MemberFn::update);

    cut_assert_true(dcpd.start());
    cut_assert_true(dcpd.commit());

    return true;
}
