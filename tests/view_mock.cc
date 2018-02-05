/*
 * Copyright (C) 2015, 2016, 2017, 2018  T+A elektroakustik GmbH & Co. KG
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
    process_event,
    process_broadcast,
    write_xml_begin,
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

      case MemberFn::process_event:
        os << "process_event";
        break;

      case MemberFn::process_broadcast:
        os << "process_broadcast";
        break;

      case MemberFn::write_xml_begin:
        os << "write_xml_begin";
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
  public:
    Expectation(const Expectation &) = delete;
    Expectation(Expectation &&) = default;
    Expectation &operator=(const Expectation &) = delete;

    const MemberFn function_id_;

    const InputResult retval_input_;
    const bool retval_bool_;
    const UI::ViewEventID arg_view_event_id_;
    const UI::BroadcastEventID arg_broadcast_event_id_;
    const bool arg_is_full_view_;
    const bool expect_parameters_;
    const CheckViewEventParametersFn check_view_event_parameters_fn_;
    const CheckBroadcastEventParametersFn check_broadcast_event_parameters_fn_;
    std::unique_ptr<const UI::Parameters> expected_parameters_;

    explicit Expectation(MemberFn id):
        function_id_(id),
        retval_input_(InputResult::OK),
        retval_bool_(false),
        arg_view_event_id_(UI::ViewEventID::NOP),
        arg_broadcast_event_id_(UI::BroadcastEventID::NOP),
        arg_is_full_view_(false),
        expect_parameters_(false),
        check_view_event_parameters_fn_(nullptr),
        check_broadcast_event_parameters_fn_(nullptr)
    {}

    explicit Expectation(MemberFn id, bool retval, bool is_full_view):
        function_id_(id),
        retval_input_(InputResult::OK),
        retval_bool_(retval),
        arg_view_event_id_(UI::ViewEventID::NOP),
        arg_broadcast_event_id_(UI::BroadcastEventID::NOP),
        arg_is_full_view_(is_full_view),
        expect_parameters_(false),
        check_view_event_parameters_fn_(nullptr),
        check_broadcast_event_parameters_fn_(nullptr)
    {}

    explicit Expectation(MemberFn id, InputResult retval, UI::ViewEventID event_id,
                         bool expect_parameters):
        function_id_(id),
        retval_input_(retval),
        retval_bool_(false),
        arg_view_event_id_(event_id),
        arg_broadcast_event_id_(UI::BroadcastEventID::NOP),
        arg_is_full_view_(false),
        expect_parameters_(expect_parameters),
        check_view_event_parameters_fn_(nullptr),
        check_broadcast_event_parameters_fn_(nullptr)
    {}

    explicit Expectation(MemberFn id, InputResult retval, UI::ViewEventID event_id,
                         std::unique_ptr<const UI::Parameters> expected_parameters,
                         CheckViewEventParametersFn check_params_callback):
        function_id_(id),
        retval_input_(retval),
        retval_bool_(false),
        arg_view_event_id_(event_id),
        arg_broadcast_event_id_(UI::BroadcastEventID::NOP),
        arg_is_full_view_(false),
        expect_parameters_(expected_parameters != nullptr),
        check_view_event_parameters_fn_(check_params_callback),
        check_broadcast_event_parameters_fn_(nullptr),
        expected_parameters_(std::move(expected_parameters))
    {}

    explicit Expectation(MemberFn id, UI::BroadcastEventID event_id,
                         bool expect_parameters):
        function_id_(id),
        retval_input_(InputResult::OK),
        retval_bool_(false),
        arg_view_event_id_(UI::ViewEventID::NOP),
        arg_broadcast_event_id_(event_id),
        arg_is_full_view_(false),
        expect_parameters_(expect_parameters),
        check_view_event_parameters_fn_(nullptr),
        check_broadcast_event_parameters_fn_(nullptr)
    {}

    explicit Expectation(MemberFn id, UI::BroadcastEventID event_id,
                         std::unique_ptr<const UI::Parameters> expected_parameters,
                         CheckBroadcastEventParametersFn check_params_callback):
        function_id_(id),
        retval_input_(InputResult::OK),
        retval_bool_(false),
        arg_view_event_id_(UI::ViewEventID::NOP),
        arg_broadcast_event_id_(event_id),
        arg_is_full_view_(false),
        expect_parameters_(expected_parameters != nullptr),
        check_view_event_parameters_fn_(nullptr),
        check_broadcast_event_parameters_fn_(check_params_callback),
        expected_parameters_(std::move(expected_parameters))
    {}
};

ViewMock::View::View(const char *name, ViewIface::Flags &&flags):
    ViewIface(name, std::move(flags), nullptr),
    ViewSerializeBase("The mock view", ViewID::MESSAGE),
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

void ViewMock::View::expect_process_event(InputResult retval, UI::ViewEventID event_id,
                                          bool expect_parameters)
{
    expectations_->add(Expectation(MemberFn::process_event, retval, event_id,
                                   expect_parameters));
}

void ViewMock::View::expect_process_event_with_callback(InputResult retval, UI::ViewEventID event_id,
                                                        std::unique_ptr<const UI::Parameters> expected_parameters,
                                                        CheckViewEventParametersFn check_params_callback)
{
    expectations_->add(Expectation(MemberFn::process_event, retval, event_id,
                                   std::move(expected_parameters),
                                   check_params_callback));
}

void ViewMock::View::expect_process_broadcast(UI::BroadcastEventID event_id,
                                              bool expect_parameters)
{
    expectations_->add(Expectation(MemberFn::process_broadcast, event_id, expect_parameters));
}

void ViewMock::View::expect_process_broadcast_with_callback(UI::BroadcastEventID event_id,
                                                            std::unique_ptr<const UI::Parameters> expected_parameters,
                                                            CheckBroadcastEventParametersFn check_params_callback)
{
    expectations_->add(Expectation(MemberFn::process_broadcast, event_id,
                                   std::move(expected_parameters),
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

void ViewMock::View::expect_write_xml_begin(bool retval, bool is_full_view)
{
    expectations_->add(Expectation(MemberFn::write_xml_begin, retval, is_full_view));
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

ViewIface::InputResult ViewMock::View::process_event(UI::ViewEventID event_id,
                                                     std::unique_ptr<const UI::Parameters> parameters)
{
    if(ignore_all_)
        return ViewIface::InputResult::OK;

    const auto &expect(expectations_->get_next_expectation(__func__));

    cppcut_assert_equal(expect.function_id_, MemberFn::process_event);
    cppcut_assert_equal(int(expect.arg_view_event_id_), int(event_id));

    if(expect.check_view_event_parameters_fn_ != nullptr)
        expect.check_view_event_parameters_fn_(
            std::move(const_cast<Expectation &>(expect).expected_parameters_),
            std::move(parameters));
    else if(expect.expect_parameters_)
        cppcut_assert_not_null(parameters.get());
    else
        cppcut_assert_null(parameters.get());

    return expect.retval_input_;
}

void ViewMock::View::process_broadcast(UI::BroadcastEventID event_id, const UI::Parameters *parameters)
{
    if(ignore_all_)
        return;

    const auto &expect(expectations_->get_next_expectation(__func__));

    cppcut_assert_equal(expect.function_id_, MemberFn::process_broadcast);
    cppcut_assert_equal(int(expect.arg_broadcast_event_id_), int(event_id));

    if(expect.check_broadcast_event_parameters_fn_ != nullptr)
        expect.check_broadcast_event_parameters_fn_(
            std::move(const_cast<Expectation &>(expect).expected_parameters_),
            std::move(parameters));
    else if(expect.expect_parameters_)
        cppcut_assert_not_null(parameters);
    else
        cppcut_assert_null(parameters);
}

bool ViewMock::View::write_xml_begin(std::ostream &os, uint32_t bits,
                                     const DCP::Queue::Data &data)
{
    if(ignore_all_)
        return true;

    const auto &expect(expectations_->get_next_expectation(__func__));

    cppcut_assert_equal(expect.function_id_, MemberFn::write_xml_begin);
    cppcut_assert_equal(expect.arg_is_full_view_, data.is_full_serialize_);

    return expect.retval_bool_;
}

bool ViewMock::View::write_xml(std::ostream &os, uint32_t bits,
                               const DCP::Queue::Data &data)
{
    /* don't emit anything to keep tests simple */
    cppcut_assert_equal(0U, bits);
    return true;
}

bool ViewMock::View::write_xml_end(std::ostream &os, uint32_t bits,
                                   const DCP::Queue::Data &data)
{
    /* don't emit anything to keep tests simple */
    cppcut_assert_equal(0U, bits);
    return true;
}

void ViewMock::View::serialize(DCP::Queue &queue, DCP::Queue::Mode mode,
                               std::ostream *debug_os)
{
    if(!ignore_all_)
    {
        const auto &expect(expectations_->get_next_expectation(__func__));

        cppcut_assert_equal(expect.function_id_, MemberFn::serialize);
    }

    const bool succeeded =
        InternalDoSerialize::do_serialize(*this, queue, true);

    cut_assert_true(succeeded);
}

void ViewMock::View::update(DCP::Queue &queue, DCP::Queue::Mode mode,
                            std::ostream *debug_os)
{
    if(!ignore_all_)
    {
        const auto &expect(expectations_->get_next_expectation(__func__));

        cppcut_assert_equal(expect.function_id_, MemberFn::update);
    }

    const bool was_idle = queue.get_introspection_iface().is_idle();
    const bool succeeded =
        InternalDoSerialize::do_serialize(*this, queue, false);

    cppcut_assert_equal(was_idle, succeeded);
}
