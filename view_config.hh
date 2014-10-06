#ifndef VIEW_CONFIG_HH
#define VIEW_CONFIG_HH

#include "view.hh"

namespace ViewConfig
{

class View: public ViewIface
{
  private:
    View(const View &);
    View &operator=(const View &);

  public:
    explicit View() {}

    bool init() override;

    void focus() override;
    void defocus() override;

    InputResult input(DrcpCommand command) override;

    void serialize(std::ostream &os) override;
    void update(std::ostream &os) override;
};

};

#endif /* !VIEW_CONFIG_HH */
