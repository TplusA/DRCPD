#ifndef LISTNAV_HH
#define LISTNAV_HH

#include "list.hh"

/*!
 * \addtogroup list_navigation List navigation
 * \ingroup list
 */
/*!@{*/

namespace List
{

class NavItemFilterIface
{
  protected:
    const List::ListIface *list_;

    constexpr explicit NavItemFilterIface(const List::ListIface *list = nullptr):
        list_(list)
    {}

  public:
    NavItemFilterIface(const NavItemFilterIface &) = delete;
    NavItemFilterIface &operator=(const NavItemFilterIface &) = delete;

    virtual ~NavItemFilterIface() {}

    virtual void tie(const List::ListIface *list)
    {
        if(list_ == list)
            return;

        list_ = list;
        list_content_changed();
    }

    bool is_tied() const { return list_ != nullptr; }

    virtual void list_content_changed() = 0;

    virtual bool ensure_consistency() const = 0;

    virtual bool is_visible(unsigned int flags) const = 0;
    virtual bool is_selectable(unsigned int flags) const = 0;

    virtual unsigned int get_first_selectable_line() const = 0;
    virtual unsigned int get_last_selectable_line() const = 0;
    virtual unsigned int get_first_visible_line() const = 0;
    virtual unsigned int get_last_visible_line() const = 0;
    virtual unsigned int get_flags_for_line(unsigned int line) const = 0;
};

class NavItemNoFilter: public NavItemFilterIface
{
  private:
    unsigned int number_of_items_minus_1_;
    bool contains_items_;

  public:
    NavItemNoFilter(const NavItemNoFilter &) = delete;
    NavItemNoFilter &operator=(const NavItemNoFilter &) = delete;

    explicit NavItemNoFilter(const List::ListIface *list):
        NavItemFilterIface::NavItemFilterIface(list)
    {
        list_content_changed();
    }

    void list_content_changed() override
    {
        const unsigned int n = (list_ != nullptr) ? list_->get_number_of_items() : 0;

        contains_items_ = (n > 0);
        number_of_items_minus_1_ = contains_items_ ? n - 1 : 0;
    }

    bool ensure_consistency() const override { return false; }
    bool is_visible(unsigned int flags) const override { return contains_items_; }
    bool is_selectable(unsigned int flags) const override { return contains_items_; }
    unsigned int get_first_selectable_line() const override { return 0; }
    unsigned int get_last_selectable_line() const override { return number_of_items_minus_1_; }
    unsigned int get_first_visible_line() const override { return 0; }
    unsigned int get_last_visible_line() const override { return number_of_items_minus_1_; }
    unsigned int get_flags_for_line(unsigned int line) const override { return 0; }
};

class Nav
{
  private:
    /*!
     * The currently selected item number in the underlying list.
     */
    unsigned int cursor_;

    /*!
     * The item number displayed in the first line.
     */
    unsigned int first_displayed_item_;

    /*!
     * Currently selected line number as is visible on the display.
     */
    unsigned int selected_line_number_;

    const unsigned int maximum_number_of_displayed_lines_;
    const NavItemFilterIface &item_filter_;

  public:
    Nav(const Nav &) = delete;
    Nav &operator=(const Nav &) = delete;

    explicit Nav(unsigned int max_display_lines,
                 const NavItemFilterIface &item_filter):
        cursor_(0),
        first_displayed_item_(0),
        selected_line_number_(0),
        maximum_number_of_displayed_lines_(max_display_lines),
        item_filter_(item_filter)
    {}

    bool down()
    {
        const bool full_update_required = item_filter_.ensure_consistency();

        if(full_update_required && !is_selectable(cursor_))
            recover_cursor_and_selection();

        const bool moved = (cursor_ < item_filter_.get_last_selectable_line());

        if(!moved && !full_update_required)
            return false;

        if(moved)
        {
            cursor_ = step_forward_selection(cursor_);

            if(selected_line_number_ < maximum_number_of_displayed_lines_ - 1)
                ++selected_line_number_;
        }

        if(cursor_ == item_filter_.get_last_selectable_line())
            selected_line_number_ -=
                item_filter_.get_last_visible_line() - item_filter_.get_last_selectable_line();

        first_displayed_item_ = cursor_;

        for(unsigned int line = 0;
            line < selected_line_number_ && first_displayed_item_ > item_filter_.get_first_visible_line();
            ++line)
        {
            first_displayed_item_ = step_back_visible(first_displayed_item_);
        }

        return moved;
    }

    bool up()
    {
        const bool full_update_required = item_filter_.ensure_consistency();

        if(full_update_required && !is_selectable(cursor_))
            recover_cursor_and_selection();

        const bool moved = (cursor_ > item_filter_.get_first_selectable_line());

        if(!moved && !full_update_required)
            return false;

        if(moved)
        {
            cursor_ = step_back_selection(cursor_);

            if(selected_line_number_ > 0)
                --selected_line_number_;
        }

        if(cursor_ == item_filter_.get_first_selectable_line())
            selected_line_number_ +=
                item_filter_.get_first_selectable_line() - item_filter_.get_first_visible_line();

        first_displayed_item_ = cursor_;

        for(unsigned int line = 0;
            line < selected_line_number_ && first_displayed_item_ > item_filter_.get_first_visible_line();
            ++line)
        {
            first_displayed_item_ = step_back_visible(first_displayed_item_);
        }

        return moved;
    }

    unsigned int get_cursor()
    {
        if(item_filter_.ensure_consistency() && !is_selectable(cursor_))
            cursor_ = item_filter_.get_first_selectable_line();

        return cursor_;
    }

    class const_iterator
    {
      private:
        const_iterator &operator=(const const_iterator &);

        const Nav &nav_;
        unsigned int item_;
        unsigned int line_number_;

      private:
        void find_next_visible_item()
        {
            if(line_number_ >= nav_.maximum_number_of_displayed_lines_)
                return;

            const unsigned int last = nav_.item_filter_.get_last_visible_line();

            if(item_ < last)
            {
                item_ = nav_.step_forward_visible(item_);
                ++line_number_;
            }
            else
                line_number_ = nav_.maximum_number_of_displayed_lines_;
        }

      public:
        explicit const_iterator(const Nav &nav, unsigned int item,
                                unsigned int line_number = 0):
            nav_(nav),
            item_(item),
            line_number_(line_number)
        {
            if(nav_.item_filter_.is_tied())
            {
                if(!nav_.is_visible(item_))
                    find_next_visible_item();
            }
            else
                line_number_ = nav_.maximum_number_of_displayed_lines_;
        }

        unsigned int operator*() const
        {
            return item_;
        }

        const_iterator &operator++()
        {
            find_next_visible_item();
            return *this;
        }

        bool operator!=(const const_iterator &other) const
        {
            return line_number_ != other.line_number_;
        }
    };

    const_iterator begin() const
    {
        return const_iterator(*this, first_displayed_item_);
    }

    const_iterator end() const
    {
        return const_iterator(*this, first_displayed_item_,
                              maximum_number_of_displayed_lines_);
    }

  private:
    bool is_visible(unsigned int line) const
    {
        return item_filter_.is_visible(item_filter_.get_flags_for_line(line));
    }

    bool is_selectable(unsigned int line) const
    {
        return item_filter_.is_selectable(item_filter_.get_flags_for_line(line));
    }

    unsigned int step_forward_selection(unsigned int line) const
    {
        while(!is_selectable(++line))
        {
            /* nothing */
        }

        return line;
    }

    unsigned int step_back_selection(unsigned int line) const
    {
        while(!is_selectable(--line))
        {
            /* nothing */
        }

        return line;
    }

    unsigned int step_back_visible(unsigned int line) const
    {
        while(!is_visible(--line))
        {
            /* nothing */
        }

        return line;
    }

    unsigned int step_forward_visible(unsigned int line) const
    {
        while(!is_visible(++line))
        {
            /* nothing */
        }

        return line;
    }

    void recover_cursor_and_selection()
    {
        cursor_ = item_filter_.get_first_selectable_line();
        selected_line_number_ = 0;

        for(unsigned int line = item_filter_.get_first_visible_line();
            line < cursor_ && selected_line_number_ < maximum_number_of_displayed_lines_;
            line = step_forward_visible(line))
        {
            ++selected_line_number_;
        }
    }
};

};

/*!@}*/

#endif /* !LISTNAV_HH */
