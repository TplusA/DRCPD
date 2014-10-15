#ifndef MOCK_VIEW_MANAGER_HH
#define MOCK_VIEW_MANAGER_HH

#include "view_manager.hh"
#include "mock_expectation.hh"

class MockViewManager: public ViewManagerIface
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

    void expect_input(DrcpCommand command);
    void expect_input_set_fast_wind_factor(double factor);
    void expect_activate_view_by_name(const char *view_name);

    void input(DrcpCommand command) override;
    void input_set_fast_wind_factor(double factor) override;
    void activate_view_by_name(const char *view_name) override;
};

#endif /* !MOCK_VIEW_MANAGER_HH */
