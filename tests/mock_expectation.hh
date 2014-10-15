#ifndef MOCK_EXPECTATION_HH
#define MOCK_EXPECTATION_HH

#include <vector>

template <typename E>
class MockExpectationsTemplate
{
  private:
    MockExpectationsTemplate(const MockExpectationsTemplate &);
    MockExpectationsTemplate &operator=(const MockExpectationsTemplate &);

    std::vector<E> expectations_;
    size_t next_checked_expectation_;

  public:
    explicit MockExpectationsTemplate() {}

    void init()
    {
        expectations_.clear();
        next_checked_expectation_ = 0;
    }

    void check() const
    {
        cppcut_assert_equal(next_checked_expectation_, expectations_.size(),
                            cppcut_message() << "Have " << expectations_.size()
                            << " expectation" << (expectations_.size() == 1 ? "" : "s")
                            << ", but only "
                            << next_checked_expectation_
                            << " " << (next_checked_expectation_ == 1 ? "was" : "were")
                            << " checked");
    }

    void add(E &&expectation)
    {
        expectations_.push_back(std::move(expectation));
    }

    const E &get_next_expectation(const char *string)
    {
        cppcut_assert_operator(next_checked_expectation_, <, expectations_.size(),
                               cppcut_message() << "Missing expectation for \""
                                                << string << "\"");

        return expectations_[next_checked_expectation_++];
    }
};


#endif /* !MOCK_EXPECTATION_HH */
