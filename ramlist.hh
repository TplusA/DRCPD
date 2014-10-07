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

    const ListIface *parent_list_;
    std::vector<Item *> items_;

    Item *get_nonconst_item(unsigned int line);

  public:
    explicit RamList(): parent_list_(this) {}
    ~RamList();

    unsigned int get_number_of_items() const override;

    const Item *get_item(unsigned int line) const override;
    void set_parent_list(const ListIface *parent) override;
    bool set_child_list(unsigned int line,
                        const std::shared_ptr<ListIface> &list) override;

    const ListIface &up() const override;
    const ListIface *down(unsigned int line) const override;

    unsigned int append(Item *item);
};

template <typename T>
static unsigned int append(RamList *list, T &&item)
{
    return list->append(new T(std::move(item)));
}

};

/*!@}*/

#endif /* !RAMLIST_HH */
