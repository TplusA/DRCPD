/*
 * Copyright (C) 2015--2019  T+A elektroakustik GmbH & Co. KG
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

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <cppcutter.h>

#include <vector>
#include <string>
#include <typeinfo>
#include <functional>
#include <array>

#include "mock_view_manager.hh"
#include "ui_parameters_predefined.hh"

enum class MemberFn
{
    store_event,
    serialization_result,
    input_bounce,
    get_view_by_name,
    get_view_by_dbus_proxy,
    activate_view_by_name,
    toggle_views_by_name,

    first_valid_member_fn_id = store_event,
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
      case MemberFn::store_event:
        os << "store_event";
        break;

      case MemberFn::serialization_result:
        os << "serialization_result";
        break;

      case MemberFn::input_bounce:
        os << "input_bounce";
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
        UI::EventID arg_event_id_;
        UI::ViewEventID arg_view_event_id_;
        UI::ViewEventID bounce_xform_event_id_;
        CheckParametersFn check_parameters_fn_;
        std::unique_ptr<const UI::Parameters> expected_parameters_;
        std::string arg_view_name_;
        std::string arg_view_name_b_;
        bool arg_enforce_view_reactivation_;
        DCP::Transaction::Result arg_dcp_result_;

        Data(const Data &) = delete;
        Data(Data &&) = default;
        Data &operator=(const Data &) = delete;

        explicit Data(MemberFn fn):
            function_id_(fn),
            ret_result_(ViewIface::InputResult::OK),
            arg_event_id_(UI::EventID::NOP),
            arg_view_event_id_(UI::ViewEventID::NOP),
            bounce_xform_event_id_(UI::ViewEventID::NOP),
            check_parameters_fn_(nullptr),
            expected_parameters_(nullptr),
            arg_enforce_view_reactivation_(false),
            arg_dcp_result_(DCP::Transaction::Result::OK)
        {}
    };

    Data d;

  private:
    /* writable reference for simple ctor code */
    Data &data_ = *const_cast<Data *>(&d);

  public:
    Expectation(const Expectation &) = delete;
    Expectation(Expectation &&) = default;
    Expectation &operator=(const Expectation &) = delete;

    explicit Expectation(MemberFn id, DCP::Transaction::Result result):
        d(id)
    {
        data_.arg_dcp_result_ = result;
    }

    explicit Expectation(MemberFn id, UI::EventID event_id,
                         std::unique_ptr<const UI::Parameters> expected_parameters,
                         CheckParametersFn check_params_callback):
        d(id)
    {
        data_.arg_event_id_ = event_id;
        data_.check_parameters_fn_ = check_params_callback;
        data_.expected_parameters_ = std::move(expected_parameters);
    }

    explicit Expectation(MemberFn id, const char *view_name, bool enforce_reactivation):
        d(id)
    {
        data_.arg_view_name_ = view_name;
        data_.arg_enforce_view_reactivation_ = enforce_reactivation;
    }

    explicit Expectation(MemberFn id,
                         const char *view_name_a, const char *view_name_b,
                         bool enforce_reactivation):
        d(id)
    {
        data_.arg_view_name_ = view_name_a;
        data_.arg_view_name_b_ = view_name_b;
        data_.arg_enforce_view_reactivation_ = enforce_reactivation;
    }

    explicit Expectation(MemberFn id, ViewIface::InputResult retval,
                         UI::ViewEventID event_id,
                         std::unique_ptr<const UI::Parameters> expected_parameters,
                         CheckParametersFn check_params_callback,
                         UI::ViewEventID xform_event_id, const char *view_name):
        d(id)
    {
        data_.ret_result_ = retval;
        data_.arg_view_event_id_ = event_id;
        data_.bounce_xform_event_id_ = xform_event_id;
        data_.check_parameters_fn_ = check_params_callback;
        data_.expected_parameters_ = std::move(expected_parameters);

        if(view_name != nullptr)
            data_.arg_view_name_ = view_name;
    }

    explicit Expectation(MemberFn id):
        d(id)
    {}
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

template <UI::EventID E, typename Traits = ::UI::Events::ParamTraits<E>>
struct ParamCompareTraits
{
    static void expect_equal(const typename Traits::PType *expected,
                             const typename Traits::PType *actual)
    {
        cut_assert_true(expected->get_specific() == actual->get_specific());
    }
};

template <>
struct ParamCompareTraits<UI::EventID::PLAYBACK_FAST_WIND_SET_SPEED>
{
    using Traits = ::UI::Events::ParamTraits<UI::EventID::PLAYBACK_FAST_WIND_SET_SPEED>;

    static void expect_equal(const typename Traits::PType *expected,
                             const typename Traits::PType *actual)
    {
        cut_assert_false(expected->get_specific() < actual->get_specific() ||
                         expected->get_specific() > actual->get_specific());
    }
};

template <>
struct ParamCompareTraits<UI::EventID::VIEW_SEARCH_STORE_PARAMETERS>
{
    using Traits = ::UI::Events::ParamTraits<UI::EventID::VIEW_SEARCH_STORE_PARAMETERS>;

    static void expect_equal(const typename Traits::PType *expected,
                             const typename Traits::PType *actual)
    {
        cut_fail("Not implemented");
    }
};

template <UI::EventID E, typename Traits = ::UI::Events::ParamTraits<E>>
static bool check_equality(const std::unique_ptr<const UI::Parameters> &expected,
                           const std::unique_ptr<const UI::Parameters> &actual)
{
    const auto *expected_ptr = dynamic_cast<const typename Traits::PType *>(expected.get());
    const auto *actual_ptr   = dynamic_cast<const typename Traits::PType *>(actual.get());

    if(expected_ptr == nullptr)
    {
        cppcut_assert_null(actual_ptr);
        return false;
    }

    cppcut_assert_not_null(actual_ptr);

    ParamCompareTraits<E>::expect_equal(expected_ptr, actual_ptr);

    return true;
}

static void check_ui_parameters_equality(std::unique_ptr<const UI::Parameters> expected,
                                         std::unique_ptr<const UI::Parameters> actual)
{
    if(expected == nullptr)
    {
        cppcut_assert_null(actual.get());
        return;
    }
    else
        cppcut_assert_not_null(actual.get());

    const std::type_info &type_expected(typeid(*expected.get()));
    const std::type_info &type_actual(typeid(*actual.get()));

    cppcut_assert_equal(type_expected.hash_code(), type_actual.hash_code());
    cppcut_assert_equal(type_expected.name(), type_actual.name());

    using Checker = std::function<bool(const std::unique_ptr<const UI::Parameters> &,
                                       const std::unique_ptr<const UI::Parameters> &)>;

    static const std::array<const Checker, 15> checkers
    {
        check_equality<UI::EventID::PLAYBACK_FAST_WIND_SET_SPEED>,
        check_equality<UI::EventID::PLAYBACK_SEEK_STREAM_POS>,
        check_equality<UI::EventID::NAV_SCROLL_LINES>,
        check_equality<UI::EventID::NAV_SCROLL_PAGES>,
        check_equality<UI::EventID::VIEW_OPEN>,
        check_equality<UI::EventID::VIEW_TOGGLE>,
        check_equality<UI::EventID::VIEWMAN_INVALIDATE_LIST_ID>,
        check_equality<UI::EventID::VIEW_PLAYER_NOW_PLAYING>,
        check_equality<UI::EventID::VIEW_PLAYER_STORE_PRELOADED_META_DATA>,
        check_equality<UI::EventID::VIEW_PLAYER_STORE_STREAM_META_DATA>,
        check_equality<UI::EventID::VIEW_PLAYER_STREAM_STOPPED>,
        check_equality<UI::EventID::VIEW_PLAYER_STREAM_PAUSED>,
        check_equality<UI::EventID::VIEW_PLAYER_STREAM_POSITION>,
        check_equality<UI::EventID::VIEW_SEARCH_STORE_PARAMETERS>,
        check_equality<UI::EventID::VIEW_AIRABLE_SERVICE_LOGIN_STATUS_UPDATE>,
    };

    for(const auto &checker : checkers)
    {
        if(checker(expected, actual))
            return;
    }

    cut_fail("No equality checker for %s", type_expected.name());
}

void MockViewManager::expect_store_event(UI::EventID event_id,
                                         std::unique_ptr<const UI::Parameters> parameters)
{
    expectations_->add(Expectation(MemberFn::store_event, event_id, std::move(parameters), check_ui_parameters_equality));
}

void MockViewManager::expect_store_event_with_callback(UI::EventID event_id,
                                                       std::unique_ptr<const UI::Parameters> parameters,
                                                       CheckParametersFn check_params_callback)
{
    expectations_->add(Expectation(MemberFn::store_event, event_id, std::move(parameters), check_params_callback));
}

void MockViewManager::expect_serialization_result(DCP::Transaction::Result result)
{
    expectations_->add(Expectation(MemberFn::serialization_result, result));
}

void MockViewManager::expect_input_bounce(ViewIface::InputResult retval, UI::ViewEventID event_id, std::unique_ptr<const UI::Parameters> parameters, UI::ViewEventID xform_event_id, const char *view_name)
{
    expectations_->add(Expectation(MemberFn::input_bounce, retval, event_id, std::move(parameters), check_ui_parameters_equality, xform_event_id, view_name));
}

void MockViewManager::expect_get_view_by_name(const char *view_name)
{
    expectations_->add(Expectation(MemberFn::get_view_by_name));
}

void MockViewManager::expect_get_view_by_dbus_proxy(const void *dbus_proxy)
{
    expectations_->add(Expectation(MemberFn::get_view_by_dbus_proxy));
}

void MockViewManager::expect_sync_activate_view_by_name(const char *view_name,
                                                        bool enforce_reactivation)
{
    expectations_->add(Expectation(MemberFn::activate_view_by_name,
                                   view_name, enforce_reactivation));
}

void MockViewManager::expect_sync_toggle_views_by_name(const char *view_name_a,
                                                       const char *view_name_b,
                                                       bool enforce_reactivation)
{
    expectations_->add(Expectation(MemberFn::toggle_views_by_name,
                                   view_name_a, view_name_b, enforce_reactivation));
}


bool MockViewManager::add_view(ViewIface *view)
{
    cut_fail("Not implemented");
    return false;
}

bool MockViewManager::invoke_late_init_functions()
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

void MockViewManager::set_resume_playback_configuration_file(const char *filename)
{
    cut_fail("Not implemented");
}

void MockViewManager::deselected_notification()
{
    cut_fail("Not implemented");
}

void MockViewManager::shutdown()
{
    cut_fail("Not implemented");
}

const char *MockViewManager::get_resume_url_by_audio_source_id(const std::string &id) const
{
    cut_fail("Not implemented");
    return nullptr;
}

std::string MockViewManager::move_resume_url_by_audio_source_id(const std::string &id)
{
    cut_fail("Not implemented");
    return "";
}

void MockViewManager::store_event(UI::EventID event_id, std::unique_ptr<const UI::Parameters> parameters)
{
    const auto &expect(expectations_->get_next_expectation(__func__));

    cppcut_assert_equal(expect.d.function_id_, MemberFn::store_event);
    cppcut_assert_equal(int(expect.d.arg_event_id_), int(event_id));

    if(expect.d.check_parameters_fn_ != nullptr)
        expect.d.check_parameters_fn_(std::move(const_cast<Expectation &>(expect).d.expected_parameters_),
                                      std::move(parameters));
    else if(expect.d.expected_parameters_ != nullptr)
        cppcut_assert_not_null(parameters.get());
    else
        cppcut_assert_null(parameters.get());
}

void MockViewManager::serialization_result(DCP::Transaction::Result result)
{
    const auto &expect(expectations_->get_next_expectation(__func__));

    cppcut_assert_equal(expect.d.function_id_, MemberFn::serialization_result);
    cppcut_assert_equal(int(expect.d.arg_dcp_result_), int(result));
}

ViewIface::InputResult MockViewManager::input_bounce(const ViewManager::InputBouncer &bouncer, UI::ViewEventID event_id, std::unique_ptr<const UI::Parameters> parameters)
{
    const auto &expect(expectations_->get_next_expectation(__func__));

    cppcut_assert_equal(expect.d.function_id_, MemberFn::input_bounce);
    cppcut_assert_equal(int(expect.d.arg_view_event_id_), int(event_id));

    if(expect.d.check_parameters_fn_ != nullptr)
        expect.d.check_parameters_fn_(std::move(const_cast<Expectation &>(expect).d.expected_parameters_),
                                      std::move(parameters));
    else if(expect.d.expected_parameters_ != nullptr)
        cppcut_assert_not_null(parameters.get());
    else
        cppcut_assert_null(parameters.get());

    const auto *item = bouncer.find(event_id);

    if(item == nullptr)
    {
        cppcut_assert_equal(int(ViewIface::InputResult::OK), int(expect.d.ret_result_));
        cppcut_assert_equal(int(UI::ViewEventID::NOP), int(expect.d.bounce_xform_event_id_));
        cut_assert_true(expect.d.arg_view_name_.empty());
    }
    else
    {
        cppcut_assert_equal(int(expect.d.bounce_xform_event_id_), int(item->xform_event_id_));
        cppcut_assert_equal(expect.d.arg_view_name_.c_str(), item->view_name_);
        cut_assert_false(expect.d.arg_view_name_.empty());
    }

    return expect.d.ret_result_;
}

ViewIface *MockViewManager::get_view_by_name(const char *view_name)
{
    const auto &expect(expectations_->get_next_expectation(__func__));

    cppcut_assert_equal(expect.d.function_id_, MemberFn::get_view_by_name);
    return nullptr;
}

void MockViewManager::sync_activate_view_by_name(const char *view_name,
                                                 bool enforce_reactivation)
{
    const auto &expect(expectations_->get_next_expectation(__func__));

    cppcut_assert_equal(expect.d.function_id_, MemberFn::activate_view_by_name);
    cppcut_assert_equal(expect.d.arg_view_name_, std::string(view_name));
    cppcut_assert_equal(expect.d.arg_enforce_view_reactivation_, enforce_reactivation);
}

void MockViewManager::sync_toggle_views_by_name(const char *view_name_a,
                                                const char *view_name_b,
                                                bool enforce_reactivation)
{
    const auto &expect(expectations_->get_next_expectation(__func__));

    cppcut_assert_equal(expect.d.function_id_, MemberFn::toggle_views_by_name);
    cppcut_assert_equal(expect.d.arg_view_name_, std::string(view_name_a));
    cppcut_assert_equal(expect.d.arg_view_name_b_, std::string(view_name_b));
    cppcut_assert_equal(expect.d.arg_enforce_view_reactivation_, enforce_reactivation);
}

bool MockViewManager::is_active_view(const ViewIface *view) const
{
    cut_fail("Not implemented");
    return false;
}

void MockViewManager::serialize_view_if_active(const ViewIface *view, DCP::Queue::Mode mode) const
{
    cut_fail("Not implemented");
}

void MockViewManager::serialize_view_forced(const ViewIface *view, DCP::Queue::Mode mode) const
{
    cut_fail("Not implemented");
}

void MockViewManager::update_view_if_active(const ViewIface *view, DCP::Queue::Mode mode) const
{
    cut_fail("Not implemented");
}

void MockViewManager::hide_view_if_active(const ViewIface *view)
{
    cut_fail("Not implemented");
}

Configuration::ConfigChangedIface &MockViewManager::get_config_changer() const
{
    cut_fail("Not implemented");
    return *static_cast<Configuration::ConfigChangedIface *>(nullptr);
}

const Configuration::DrcpdValues &MockViewManager::get_configuration() const
{
    cut_fail("Not implemented");
    return *static_cast<Configuration::DrcpdValues *>(nullptr);
}
