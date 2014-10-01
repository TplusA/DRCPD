#ifndef VIEW_HH
#define VIEW_HH

#include <ostream>

#include "drcp_commands.hh"

class ViewIface
{
  private:
    ViewIface(const ViewIface &);
    ViewIface &operator=(const ViewIface &);

  protected:
    explicit ViewIface() {}

  public:
    virtual ~ViewIface() {}

    virtual void input(DrcpCommand command) = 0;
    virtual void serialize(std::ostream &os) = 0;
    virtual void update(std::ostream &os) = 0;
};

class ViewConfig: public ViewIface
{
  private:
    ViewConfig(const ViewConfig &);
    ViewConfig &operator=(const ViewConfig &);

  public:
    explicit ViewConfig() {}

    void input(DrcpCommand command) override;
    void serialize(std::ostream &os) override;
    void update(std::ostream &os) override;
};

#endif /* !VIEW_HH */
