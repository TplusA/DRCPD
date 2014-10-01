#ifndef RAMLIST_HH
#define RAMLIST_HH

#include "list.hh"

/*!
 * \addtogroup ram_list Lists with contents held in RAM.
 * \ingroup list
 */
/*!@{*/

namespace List
{

/*!
 * A list with all list items stored in RAM.
 */
class RamList: public ListIface
{
  private:
    RamList(const RamList &);
    RamList &operator=(const RamList &);

  public:
    explicit RamList() {}

    unsigned int get_number_of_items() const override;

    const Item *get_item(unsigned int line) override;

    const ListIface *up() const override;
    const ListIface *down(unsigned int line) const override;
};

};

/*!@}*/

#endif /* !RAMLIST_HH */
