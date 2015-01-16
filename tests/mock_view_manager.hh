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

    void expect_serialization_result(DcpTransaction::Result result);
    void expect_input(DrcpCommand command);
    void expect_input_set_fast_wind_factor(double factor);
    void expect_input_move_cursor_by_line(int lines);
    void expect_input_move_cursor_by_page(int pages);
    void expect_get_view_by_name(const char *view_name);
    void expect_activate_view_by_name(const char *view_name);
    void expect_toggle_views_by_name(const char *view_name_a,
                                     const char *view_name_b);

    bool add_view(ViewIface *view) override;
    void set_output_stream(std::ostream &os) override;
    void set_debug_stream(std::ostream &os) override;
    void serialization_result(DcpTransaction::Result result) override;
    void input(DrcpCommand command) override;
    void input_set_fast_wind_factor(double factor) override;
    void input_move_cursor_by_line(int lines) override;
    void input_move_cursor_by_page(int pages) override;
    ViewIface *get_view_by_name(const char *view_name) override;
    void activate_view_by_name(const char *view_name) override;
    void toggle_views_by_name(const char *view_name_a,
                              const char *view_name_b) override;
    bool is_active_view(const ViewIface *view) const override;
    bool serialize_view_if_active(const ViewIface *view) const override;
    bool update_view_if_active(const ViewIface *view) const override;
    void hide_view_if_active(const ViewIface *view) override;
};

#endif /* !MOCK_VIEW_MANAGER_HH */
