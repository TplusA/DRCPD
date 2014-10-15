#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <cppcutter.h>

#include <vector>
#include <string>

#include "mock_view_manager.hh"

enum class MemberFn
{
    input,
    input_set_fast_wind_factor,
    activate_view_by_name,

    first_valid_member_fn_id = input,
    last_valid_member_fn_id = activate_view_by_name,
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
      case MemberFn::input:
        os << "input";
        break;

      case MemberFn::input_set_fast_wind_factor:
        os << "input_set_fast_wind_factor";
        break;

      case MemberFn::activate_view_by_name:
        os << "activate_view_by_name";
        break;
    }

    os << "()";

    return os;
}

class MockViewManager::Expectation
{
  private:
    Expectation(const Expectation &);
    Expectation &operator=(const Expectation &);

  public:
    const MemberFn function_id_;

    const DrcpCommand arg_command_;
    const double arg_factor_;
    const std::string arg_view_name_;

    explicit Expectation(MemberFn id, DrcpCommand command):
        function_id_(id),
        arg_command_(command),
        arg_factor_(0.0),
        arg_view_name_("")
    {}

    explicit Expectation(MemberFn id, double factor):
        function_id_(id),
        arg_command_(DrcpCommand::UNDEFINED_COMMAND),
        arg_factor_(factor),
        arg_view_name_("")
    {}

    explicit Expectation(MemberFn id, const char *view_name):
        function_id_(id),
        arg_command_(DrcpCommand::UNDEFINED_COMMAND),
        arg_factor_(0.0),
        arg_view_name_(view_name)
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

void MockViewManager::expect_input(DrcpCommand command)
{
    expectations_->add(Expectation(MemberFn::input, command));
}

void MockViewManager::expect_input_set_fast_wind_factor(double factor)
{
    expectations_->add(Expectation(MemberFn::input_set_fast_wind_factor, factor));
}

void MockViewManager::expect_activate_view_by_name(const char *view_name)
{
    expectations_->add(Expectation(MemberFn::activate_view_by_name, view_name));
}

void MockViewManager::input(DrcpCommand command)
{
    const auto &expect(expectations_->get_next_expectation(__func__));

    cppcut_assert_equal(expect.function_id_, MemberFn::input);
    cppcut_assert_equal((int)expect.arg_command_, (int)command);
}

void MockViewManager::input_set_fast_wind_factor(double factor)
{
    const auto &expect(expectations_->get_next_expectation(__func__));

    cppcut_assert_equal(expect.function_id_, MemberFn::input_set_fast_wind_factor);
    cppcut_assert_equal(expect.arg_factor_, factor);
}

void MockViewManager::activate_view_by_name(const char *view_name)
{
    const auto &expect(expectations_->get_next_expectation(__func__));

    cppcut_assert_equal(expect.function_id_, MemberFn::activate_view_by_name);
    cppcut_assert_equal(expect.arg_view_name_, std::string(view_name));
}
