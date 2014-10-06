#ifndef LIST_HH
#define LIST_HH

#include <string>
#include <vector>

/*!
 * \addtogroup list List data model.
 */
/*!@{*/

namespace List
{

class ListIface;

class Item
{
  private:
    Item(const Item &);
    Item &operator=(const Item &);

    std::string text_;
    bool text_is_translatable_;

    unsigned int flags_;
    ListIface *child_list_;

  public:
    explicit Item(Item &&) = default;

    explicit Item(unsigned int flags):
        text_is_translatable_(false),
        flags_(flags),
        child_list_(nullptr)
    {}

    explicit Item(const char *text, bool text_is_translatable,
                  unsigned int flags):
        text_(text),
        text_is_translatable_(text_is_translatable),
        flags_(flags),
        child_list_(nullptr)
    {}

    const ListIface *down() const
    {
        return child_list_;
    }

    const char *get_text() const;
};

class ListIface
{
  private:
    ListIface(const ListIface &);
    ListIface &operator=(const ListIface &);

  protected:
    explicit ListIface() {}

  public:
    virtual ~ListIface() {}

    virtual unsigned int get_number_of_items() const = 0;

    virtual const Item *get_item(unsigned int line) = 0;

    virtual const ListIface *up() const = 0;
    virtual const ListIface *down(unsigned int line) const = 0;
};

};

/*!@}*/

#endif /* !LIST_HH */
