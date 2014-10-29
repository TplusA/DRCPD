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

    const DrcpCommand arg_command_;
    InputResult retval_input_;

    explicit Expectation(MemberFn id):
        function_id_(id),
        arg_command_(DrcpCommand::UNDEFINED_COMMAND),
        retval_input_(InputResult::OK)
    {}

    explicit Expectation(MemberFn id, DrcpCommand command, InputResult retval):
        function_id_(id),
        arg_command_(command),
        retval_input_(retval)
    {}

    Expectation(Expectation &&) = default;
};

ViewMock::View::View(const std::string &name):
    ViewIface(name),
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

void ViewMock::View::expect_input_return(DrcpCommand command, InputResult retval)
{
    expectations_->add(Expectation(MemberFn::input, command, retval));
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

ViewIface::InputResult ViewMock::View::input(DrcpCommand command)
{
    if(ignore_all_)
        return ViewIface::InputResult::OK;

    const auto &expect(expectations_->get_next_expectation(__func__));

    cppcut_assert_equal(expect.function_id_, MemberFn::input);
    cppcut_assert_equal(int(expect.arg_command_), int(command));

    return expect.retval_input_;
}

void ViewMock::View::serialize(std::ostream &os)
{
    if(ignore_all_)
        return;

    const auto &expect(expectations_->get_next_expectation(__func__));

    cppcut_assert_equal(expect.function_id_, MemberFn::serialize);
}

void ViewMock::View::update(std::ostream &os)
{
    if(ignore_all_)
        return;

    const auto &expect(expectations_->get_next_expectation(__func__));

    cppcut_assert_equal(expect.function_id_, MemberFn::update);
}