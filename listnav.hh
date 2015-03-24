/*
 * Copyright (C) 2015  T+A elektroakustik GmbH & Co. KG
 *
 * This file is part of DRCPD.
 *
 * DRCPD is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 3 as
 * published by the Free Software Foundation.
 *
 * DRCPD is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with DRCPD.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LISTNAV_HH
#define LISTNAV_HH

#include <climits>

#include "list.hh"
#include "messages.h"

/*!
 * \addtogroup list_navigation List navigation
 * \ingroup list
 */
/*!@{*/

namespace List
{

/*!
 * Filter interface used by #List::Nav for hiding list contents.
 */
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
    bool is_list_nonempty() const { return is_tied() && !list_->empty(); }

    virtual void list_content_changed() = 0;

    virtual bool ensure_consistency() const = 0;

    virtual bool is_visible(unsigned int flags) const = 0;
    virtual bool is_selectable(unsigned int flags) const = 0;

    virtual unsigned int get_first_selectable_item() const = 0;
    virtual unsigned int get_last_selectable_item() const = 0;
    virtual unsigned int get_first_visible_item() const = 0;
    virtual unsigned int get_last_visible_item() const = 0;
    virtual unsigned int get_total_number_of_visible_items() const = 0;
    virtual unsigned int get_flags_for_item(unsigned int item) const = 0;
    virtual bool map_line_number_to_item(unsigned int line_number,
                                         unsigned int &item) const = 0;
    virtual bool map_item_to_line_number(unsigned int item,
                                         unsigned int &line_number) const = 0;
};

/*!
 * A filter that allows any list item to pass through.
 */
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
    unsigned int get_first_selectable_item() const override { return 0; }
    unsigned int get_last_selectable_item() const override { return number_of_items_minus_1_; }
    unsigned int get_first_visible_item() const override { return 0; }
    unsigned int get_last_visible_item() const override { return number_of_items_minus_1_; }

    unsigned int get_total_number_of_visible_items() const override
    {
        return contains_items_ ? number_of_items_minus_1_ + 1 : 0;
    }

    unsigned int get_flags_for_item(unsigned int item) const override { return 0; }

    bool map_line_number_to_item(unsigned int line_number,
                                 unsigned int &item) const override
    {
        if(!contains_items_ || line_number > number_of_items_minus_1_)
            return false;

        item = line_number;
        return true;
    }

    bool map_item_to_line_number(unsigned int item,
                                 unsigned int &line_number) const override
    {
        if(!contains_items_ || item > number_of_items_minus_1_)
            return false;

        line_number = item;
        return true;
    }
};

/*!
 * Navigational state on a list with custom filtering.
 *
 * Objects of this class maintain the state of incremental, interactive
 * read-only list accesses in form of a cursor that points to the currently
 * selected list item, and a list item that is "on top" for displaying
 * purposes.
 *
 * These list accesses are usually initiated by a user who is browsing through
 * some list using a line-oriented graphical user interface.
 *
 * A #List::Nav object does not access the list content itself (though its
 * associated filter may do), it only operates on properties of the browsed
 * list, such as number of elements in that list. Therefore, browsing is
 * possible even if the list contents are not or only partially available.
 */
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

  public:
    const unsigned int maximum_number_of_displayed_lines_;

  private:
    const NavItemFilterIface &item_filter_;

  public:
    Nav(const Nav &) = delete;
    Nav &operator=(const Nav &) = delete;

    explicit Nav(unsigned int max_display_lines,
                 const NavItemFilterIface &item_filter):
        first_displayed_item_(0),
        maximum_number_of_displayed_lines_(max_display_lines),
        item_filter_(item_filter)
    {
        recover_cursor_and_selection();
    }

    void check_selection()
    {
        (void)item_filter_.ensure_consistency();

        if(!is_selectable(cursor_))
            recover_cursor_and_selection();
    }

    unsigned int distance_to_top() const
    {
        return get_total_number_of_visible_items() > 0 ? selected_line_number_ : 0;
    }

    unsigned int distance_to_bottom() const
    {
        const unsigned int max_items = get_total_number_of_visible_items();
        if(max_items > 0)
            return std::min(maximum_number_of_displayed_lines_, max_items) - selected_line_number_ - 1;
        else
            return 0;
    }

    bool down(unsigned int count = 1)
    {
        if(count == 0)
            return false;

        const bool full_update_required = item_filter_.ensure_consistency();

        if(!is_selectable(cursor_))
            recover_cursor_and_selection();

        const unsigned int last_selectable = item_filter_.get_last_selectable_item();
        const bool moved = (cursor_ < last_selectable);

        if(!moved && !full_update_required)
            return false;

        if(moved)
        {
            for(unsigned int i = 0; i < count && cursor_ < last_selectable; ++i)
            {
                cursor_ = step_forward_selection(cursor_);

                if(selected_line_number_ < maximum_number_of_displayed_lines_ - 1)
                    ++selected_line_number_;
            }
        }

        if(cursor_ == item_filter_.get_last_selectable_item())
            selected_line_number_ -=
                item_filter_.get_last_visible_item() - item_filter_.get_last_selectable_item();

        recover_first_displayed_item_by_cursor();

        return moved;
    }

    bool up(unsigned int count = 1)
    {
        if(count == 0)
            return false;

        const bool full_update_required = item_filter_.ensure_consistency();

        if(!is_selectable(cursor_))
            recover_cursor_and_selection();

        const unsigned int first_selectable = item_filter_.get_first_selectable_item();
        const bool moved = (cursor_ > first_selectable);

        if(!moved && !full_update_required)
            return false;

        if(moved)
        {
            for(unsigned int i = 0; i < count && cursor_ > first_selectable; ++i)
            {
                cursor_ = step_back_selection(cursor_);

                if(selected_line_number_ > 0)
                    --selected_line_number_;
            }
        }

        if(cursor_ == item_filter_.get_first_selectable_item())
            selected_line_number_ +=
                item_filter_.get_first_selectable_item() - item_filter_.get_first_visible_item();

        recover_first_displayed_item_by_cursor();

        return moved;
    }

    unsigned int get_cursor()
    {
        check_selection();
        return cursor_;
    }

    unsigned int get_total_number_of_visible_items() const
    {
        return item_filter_.get_total_number_of_visible_items();
    }

    void set_cursor_by_line_number(unsigned int line_number)
    {
        if(line_number == 0 ||
           !item_filter_.map_line_number_to_item(line_number, cursor_) ||
           !item_filter_.is_selectable(item_filter_.get_flags_for_item(cursor_)))
        {
            recover_cursor_and_selection();
            return;
        }

        log_assert(maximum_number_of_displayed_lines_ > 0);

        const unsigned int max_items = get_total_number_of_visible_items();
        log_assert(line_number < max_items);

        if(max_items < maximum_number_of_displayed_lines_)
        {
            /* very short list, always displayed in full length */
            selected_line_number_ = line_number;
        }
        else
        {
            /* attempt to center the whole list around the selected line */
            selected_line_number_ = (maximum_number_of_displayed_lines_ + 1) / 2 - 1;

            const unsigned int distance_to_end_of_list = max_items - line_number - 1;

            if(distance_to_end_of_list < maximum_number_of_displayed_lines_ - selected_line_number_)
                selected_line_number_ = maximum_number_of_displayed_lines_ -
                                        distance_to_end_of_list - 1;
        }

        log_assert(selected_line_number_ < maximum_number_of_displayed_lines_);

        recover_first_displayed_item_by_cursor();
    }

    int get_line_number_by_item(unsigned int item) const
    {
        unsigned int line_number;

        if(item_filter_.map_item_to_line_number(item, line_number))
            return line_number <= INT_MAX ? int(line_number) : -1;

        return -1;
    }

    int get_line_number_by_cursor()
    {
        return get_line_number_by_item(get_cursor());
    }

    /*!
     * Forward iterator over list items.
     *
     * The iterator enumerates indices of list items that are not filtered out
     * by the filter associated with the #List::Nav object, from top to bottom.
     * As long as the associated filter does not access any list contents, the
     * iterator only operates on the list indices.
     */
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

            const unsigned int last = nav_.item_filter_.get_last_visible_item();

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
    bool is_visible(unsigned int item) const
    {
        if(item_filter_.is_list_nonempty())
            return item_filter_.is_visible(item_filter_.get_flags_for_item(item));
        else
            return false;
    }

    bool is_selectable(unsigned int item) const
    {
        if(item_filter_.is_list_nonempty())
            return item_filter_.is_selectable(item_filter_.get_flags_for_item(item));
        else
            return false;
    }

    unsigned int step_forward_selection(unsigned int item) const
    {
        while(!is_selectable(++item))
        {
            /* nothing */
        }

        return item;
    }

    unsigned int step_back_selection(unsigned int item) const
    {
        while(!is_selectable(--item))
        {
            /* nothing */
        }

        return item;
    }

    unsigned int step_back_visible(unsigned int item) const
    {
        while(!is_visible(--item))
        {
            /* nothing */
        }

        return item;
    }

    unsigned int step_forward_visible(unsigned int item) const
    {
        while(!is_visible(++item))
        {
            /* nothing */
        }

        return item;
    }

    void recover_first_displayed_item_by_cursor()
    {
        first_displayed_item_ = cursor_;

        for(unsigned int line = 0;
            line < selected_line_number_ && first_displayed_item_ > item_filter_.get_first_visible_item();
            ++line)
        {
            first_displayed_item_ = step_back_visible(first_displayed_item_);
        }
    }

    void recover_cursor_and_selection()
    {
        if(!item_filter_.is_list_nonempty())
        {
            cursor_ = 0;
            first_displayed_item_ = 0;
            selected_line_number_ = 0;
            return;
        }

        cursor_ = item_filter_.get_first_selectable_item();
        selected_line_number_ = 0;

        for(unsigned int item = item_filter_.get_first_visible_item();
            item < cursor_ && selected_line_number_ < maximum_number_of_displayed_lines_;
            item = step_forward_visible(item))
        {
            ++selected_line_number_;
        }

        recover_first_displayed_item_by_cursor();
    }
};

};

/*!@}*/

#endif /* !LISTNAV_HH */
