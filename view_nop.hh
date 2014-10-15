#ifndef VIEW_NOP_HH
#define VIEW_NOP_HH

#include "view.hh"

namespace ViewNop
{

class View: public ViewIface
{
  public:
    View(const View &) = delete;
    View &operator=(const View &) = delete;

    explicit View(): ViewIface("#NOP") {}

    bool init() override { return true; }
    void focus() override {}
    void defocus() override {}
    InputResult input(DrcpCommand command) override { return InputResult::SHOULD_HIDE; }
    void serialize(std::ostream &os) override {}
    void update(std::ostream &os) override {}
};

};

#endif /* !VIEW_NOP_HH */
