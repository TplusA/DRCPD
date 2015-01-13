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

    void expect_focus();
    void expect_defocus();
    void expect_input_return(DrcpCommand command, InputResult retval);
    void expect_serialize(std::ostream &os);
    void expect_update(std::ostream &os);

    bool init() override;
    void focus() override;
    void defocus() override;
    InputResult input(DrcpCommand command) override;
    void serialize(DcpTransaction &dcpd, std::ostream *debug_os) override;
    void update(DcpTransaction &dcpd, std::ostream *debug_os) override;
};

};

#endif /* !VIEW_MOCK_HH */
