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

#include <vector>
#include <string>

#include "mock_view_manager.hh"

enum class MemberFn
{
    serialization_result,
    input,
    input_bounce,
    input_move_cursor_by_line,
    input_move_cursor_by_page,
    get_view_by_name,
    get_view_by_dbus_proxy,
    get_playback_initiator_view,
    activate_view_by_name,
    toggle_views_by_name,

    first_valid_member_fn_id = serialization_result,
    last_valid_member_fn_id = toggle_views_by_name,
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
      case MemberFn::serialization_result:
        os << "serialization_result";
        break;

      case MemberFn::input:
        os << "input";
        break;

      case MemberFn::input_bounce:
        os << "input_bounce";
        break;

      case MemberFn::input_move_cursor_by_line:
        os << "input_move_cursor_by_line";
        break;

      case MemberFn::input_move_cursor_by_page:
        os << "input_move_cursor_by_page";
        break;

      case MemberFn::activate_view_by_name:
        os << "activate_view_by_name";
        break;

      case MemberFn::get_view_by_name:
        os << "get_view_by_name";
        break;

      case MemberFn::get_view_by_dbus_proxy:
        os << "get_view_by_dbus_proxy";
        break;

      case MemberFn::get_playback_initiator_view:
        os << "get_playback_initiator_view";
        break;

      case MemberFn::toggle_views_by_name:
        os << "toggle_views_by_name";
        break;
    }

    os << "()";

    return os;
}

class MockViewManager::Expectation
{
  public:
    struct Data
    {
        const MemberFn function_id_;

        ViewIface::InputResult ret_result_;
        DrcpCommand arg_command_;
        DrcpCommand bounce_xform_command_;
        bool expect_parameters_;
        CheckParametersFn check_parameters_fn_;
        const UI::Parameters *expected_parameters_;
        int arg_lines_or_pages_;
        std::string arg_view_name_;
        std::string arg_view_name_b_;
        DcpTransaction::Result arg_dcp_result_;

        explicit Data(MemberFn fn):
            function_id_(fn),
            ret_result_(ViewIface::InputResult::OK),
            arg_command_(DrcpCommand::UNDEFINED_COMMAND),
            bounce_xform_command_(DrcpCommand::UNDEFINED_COMMAND),
            expect_parameters_(false),
            check_parameters_fn_(nullptr),
            expected_parameters_(nullptr),
            arg_lines_or_pages_(-9999),
            arg_dcp_result_(DcpTransaction::Result::OK)
        {}
    };

    const Data d;

  private:
    /* writable reference for simple ctor code */
    Data &data_ = *const_cast<Data *>(&d);

  public:
    Expectation(const Expectation &) = delete;
    Expectation &operator=(const Expectation &) = delete;

    explicit Expectation(MemberFn id, DcpTransaction::Result result):
        d(id)
    {
        data_.arg_dcp_result_ = result;
    }

    explicit Expectation(MemberFn id, DrcpCommand command, bool expect_parameters):
        d(id)
    {
        data_.arg_command_ = command;
        data_.expect_parameters_ = expect_parameters;
    }

    explicit Expectation(MemberFn id, DrcpCommand command,
                         const UI::Parameters *expected_parameters,
                         CheckParametersFn check_params_callback):
        d(id)
    {
        data_.arg_command_ = command;
        data_.expect_parameters_ = (expected_parameters != nullptr);
        data_.check_parameters_fn_ = check_params_callback;
        data_.expected_parameters_ = expected_parameters;
    }

    explicit Expectation(MemberFn id, int lines_or_pages):
        d(id)
    {
        data_.arg_lines_or_pages_ = lines_or_pages;
    }

    explicit Expectation(MemberFn id, const char *view_name):
        d(id)
    {
        data_.arg_view_name_ = view_name;
    }

    explicit Expectation(MemberFn id,
                         const char *view_name_a, const char *view_name_b):
        d(id)
    {
        data_.arg_view_name_ = view_name_a;
        data_.arg_view_name_b_ = view_name_b;
    }

    explicit Expectation(MemberFn id, ViewIface::InputResult retval,
                         DrcpCommand command, bool expect_parameters,
                         DrcpCommand xform_command, const char *view_name):
        d(id)
    {
        data_.ret_result_ = retval;
        data_.arg_command_ = command;
        data_.bounce_xform_command_ = xform_command;
        data_.expect_parameters_ = expect_parameters;

        if(view_name != nullptr)
            data_.arg_view_name_ = view_name;
    }

    explicit Expectation(MemberFn id):
        d(id)
    {}

    Expectation(Expectation &&) = default;
};


MockViewManager::MockViewManager()
{
    expectations_ = new MockExpectations();
}

MockViewManager::~MockViewManager()
{
    delete expectations_;
}

void MockViewManager::init()
{
    cppcut_assert_not_null(expectations_);
    expectations_->init();
}

void MockViewManager::check() const
{
    cppcut_assert_not_null(expectations_);
    expectations_->check();
}

void MockViewManager::expect_serialization_result(DcpTransaction::Result result)
{
    expectations_->add(Expectation(MemberFn::serialization_result, result));
}

void MockViewManager::expect_input(DrcpCommand command, bool expect_parameters)
{
    expectations_->add(Expectation(MemberFn::input, command, expect_parameters));
}

void MockViewManager::expect_input_with_callback(DrcpCommand command,
                                                 const UI::Parameters *expected_parameters,
                                                 CheckParametersFn check_params_callback)
{
    expectations_->add(Expectation(MemberFn::input, command, expected_parameters, check_params_callback));
}

void MockViewManager::expect_input_bounce(ViewIface::InputResult retval, DrcpCommand command, bool expect_parameters, DrcpCommand xform_command, const char *view_name)
{
    expectations_->add(Expectation(MemberFn::input_bounce, retval, command, expect_parameters, xform_command, view_name));
}

void MockViewManager::expect_input_move_cursor_by_line(int lines)
{
    expectations_->add(Expectation(MemberFn::input_move_cursor_by_line, lines));
}

void MockViewManager::expect_input_move_cursor_by_page(int pages)
{
    expectations_->add(Expectation(MemberFn::input_move_cursor_by_page, pages));
}

void MockViewManager::expect_get_view_by_name(const char *view_name)
{
    expectations_->add(Expectation(MemberFn::get_view_by_name));
}

void MockViewManager::expect_get_view_by_dbus_proxy(const void *dbus_proxy)
{
    expectations_->add(Expectation(MemberFn::get_view_by_dbus_proxy));
}

void MockViewManager::expect_get_playback_initiator_view()
{
    expectations_->add(Expectation(MemberFn::get_playback_initiator_view));
}

void MockViewManager::expect_activate_view_by_name(const char *view_name)
{
    expectations_->add(Expectation(MemberFn::activate_view_by_name, view_name));
}

void MockViewManager::expect_toggle_views_by_name(const char *view_name_a,
                                                  const char *view_name_b)
{
    expectations_->add(Expectation(MemberFn::toggle_views_by_name,
                                   view_name_a, view_name_b));
}


bool MockViewManager::add_view(ViewIface *view)
{
    cut_fail("Not implemented");
    return false;
}

void MockViewManager::set_output_stream(std::ostream &os)
{
    cut_fail("Not implemented");
}

void MockViewManager::set_debug_stream(std::ostream &os)
{
    cut_fail("Not implemented");
}

void MockViewManager::serialization_result(DcpTransaction::Result result)
{
    const auto &expect(expectations_->get_next_expectation(__func__));

    cppcut_assert_equal(expect.d.function_id_, MemberFn::serialization_result);
    cppcut_assert_equal(int(expect.d.arg_dcp_result_), int(result));
}

void MockViewManager::input(DrcpCommand command, std::unique_ptr<const UI::Parameters> parameters)
{
    const auto &expect(expectations_->get_next_expectation(__func__));

    cppcut_assert_equal(expect.d.function_id_, MemberFn::input);
    cppcut_assert_equal(int(expect.d.arg_command_), int(command));

    if(expect.d.check_parameters_fn_ != nullptr)
        expect.d.check_parameters_fn_(expect.d.expected_parameters_, parameters);
    if(expect.d.expect_parameters_)
        cppcut_assert_not_null(parameters.get());
    else
        cppcut_assert_null(parameters.get());
}

ViewIface::InputResult MockViewManager::input_bounce(const ViewManager::InputBouncer &bouncer, DrcpCommand command, std::unique_ptr<const UI::Parameters> parameters)
{
    const auto &expect(expectations_->get_next_expectation(__func__));

    cppcut_assert_equal(expect.d.function_id_, MemberFn::input_bounce);
    cppcut_assert_equal(int(expect.d.arg_command_), int(command));

    if(expect.d.expect_parameters_)
        cppcut_assert_not_null(parameters.get());
    else
        cppcut_assert_null(parameters.get());

    const auto *item = bouncer.find(command);

    if(item == nullptr)
    {
        cppcut_assert_equal(int(ViewIface::InputResult::OK), int(expect.d.ret_result_));
        cppcut_assert_equal(int(DrcpCommand::UNDEFINED_COMMAND), int(expect.d.bounce_xform_command_));
        cut_assert_true(expect.d.arg_view_name_.empty());
    }
    else
    {
        cppcut_assert_equal(int(expect.d.bounce_xform_command_), int(item->xform_command_));
        cppcut_assert_equal(expect.d.arg_view_name_.c_str(), item->view_name_);
        cut_assert_false(expect.d.arg_view_name_.empty());
    }

    return expect.d.ret_result_;
}

void MockViewManager::input_move_cursor_by_line(int lines)
{
    const auto &expect(expectations_->get_next_expectation(__func__));

    cppcut_assert_equal(expect.d.function_id_, MemberFn::input_move_cursor_by_line);
    cppcut_assert_equal(expect.d.arg_lines_or_pages_, lines);
}

void MockViewManager::input_move_cursor_by_page(int pages)
{
    const auto &expect(expectations_->get_next_expectation(__func__));

    cppcut_assert_equal(expect.d.function_id_, MemberFn::input_move_cursor_by_page);
    cppcut_assert_equal(expect.d.arg_lines_or_pages_, pages);
}

ViewIface *MockViewManager::get_view_by_name(const char *view_name)
{
    const auto &expect(expectations_->get_next_expectation(__func__));

    cppcut_assert_equal(expect.d.function_id_, MemberFn::get_view_by_name);
    return nullptr;
}

ViewIface *MockViewManager::get_view_by_dbus_proxy(const void *dbus_proxy)
{
    const auto &expect(expectations_->get_next_expectation(__func__));

    cppcut_assert_equal(expect.d.function_id_, MemberFn::get_view_by_dbus_proxy);
    return nullptr;
}

ViewIface *MockViewManager::get_playback_initiator_view() const
{
    const auto &expect(expectations_->get_next_expectation(__func__));

    cppcut_assert_equal(expect.d.function_id_, MemberFn::get_playback_initiator_view);
    return nullptr;
}

void MockViewManager::activate_view_by_name(const char *view_name)
{
    const auto &expect(expectations_->get_next_expectation(__func__));

    cppcut_assert_equal(expect.d.function_id_, MemberFn::activate_view_by_name);
    cppcut_assert_equal(expect.d.arg_view_name_, std::string(view_name));
}

void MockViewManager::toggle_views_by_name(const char *view_name_a,
                                           const char *view_name_b)
{
    const auto &expect(expectations_->get_next_expectation(__func__));

    cppcut_assert_equal(expect.d.function_id_, MemberFn::toggle_views_by_name);
    cppcut_assert_equal(expect.d.arg_view_name_, std::string(view_name_a));
    cppcut_assert_equal(expect.d.arg_view_name_b_, std::string(view_name_b));
}

bool MockViewManager::is_active_view(const ViewIface *view) const
{
    cut_fail("Not implemented");
    return false;
}

bool MockViewManager::serialize_view_if_active(const ViewIface *view) const
{
    cut_fail("Not implemented");
    return true;
}

bool MockViewManager::serialize_view_forced(const ViewIface *view) const
{
    cut_fail("Not implemented");
    return true;
}

bool MockViewManager::update_view_if_active(const ViewIface *view) const
{
    cut_fail("Not implemented");
    return true;
}

void MockViewManager::hide_view_if_active(const ViewIface *view)
{
    cut_fail("Not implemented");
}
