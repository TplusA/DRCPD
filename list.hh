#ifndef LIST_HH
#define LIST_HH

#include <string>
#include <vector>
#include <memory>

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

  protected:
    unsigned int flags_;
    std::shared_ptr<ListIface> child_list_;

    explicit Item(unsigned int flags):
        flags_(flags),
        child_list_(nullptr)
    {}

  public:
    explicit Item(Item &&) = default;

    virtual ~Item() {}

    void set_child_list(const std::shared_ptr<ListIface> &list)
    {
        child_list_ = list;
    }

    const List::ListIface *down() const
    {
        return child_list_.get();
    }
};

class TextItem: public Item
{
  private:
    TextItem(const TextItem &);
    TextItem &operator=(const TextItem &);

    std::string text_;
    bool text_is_translatable_;

  public:
    explicit TextItem(TextItem &&) = default;

    explicit TextItem(unsigned int flags):
        Item(flags),
        text_is_translatable_(false)
    {}

    explicit TextItem(const char *text, bool text_is_translatable,
                      unsigned int flags):
        Item(flags),
        text_(text),
        text_is_translatable_(text_is_translatable)
    {}

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

    virtual const Item *get_item(unsigned int line) const = 0;
    virtual void set_parent_list(const ListIface *parent) = 0;
    virtual bool set_child_list(unsigned int line,
                                const std::shared_ptr<ListIface> &list) = 0;

    virtual const ListIface &up() const = 0;
    virtual const ListIface *down(unsigned int line) const = 0;
};

};

/*!@}*/

#endif /* !LIST_HH */
