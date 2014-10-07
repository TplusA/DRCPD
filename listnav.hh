#ifndef LISTNAV_HH
#define LISTNAV_HH

class ListNav
{
  private:
    ListNav(const ListNav &);
    ListNav &operator=(const ListNav &);

    unsigned int cursor_;
    unsigned int first_displayed_line_;

    const unsigned int first_selectable_line_;
    const unsigned int number_of_lines_;
    const unsigned int maximum_number_of_displayed_lines_;

  public:
    constexpr explicit ListNav(unsigned int first_selectable_line,
                               unsigned int first_line_on_display,
                               unsigned int number_of_lines,
                               unsigned int max_display_lines):
        cursor_(first_selectable_line),
        first_displayed_line_(first_line_on_display),
        first_selectable_line_(first_selectable_line),
        number_of_lines_(number_of_lines),
        maximum_number_of_displayed_lines_(max_display_lines)
    {}

    bool down()
    {
        if(cursor_ >= number_of_lines_ - 1)
            return false;

        ++cursor_;

        if(cursor_ + 1 - first_displayed_line_ > maximum_number_of_displayed_lines_)
            first_displayed_line_ = cursor_ + 1 - maximum_number_of_displayed_lines_;

        return true;

    }

    bool up()
    {
        if(cursor_ <= first_selectable_line_)
            return false;

        --cursor_;

        if(cursor_ == first_selectable_line_)
            first_displayed_line_ = 0;
        else if(cursor_ < first_displayed_line_)
            first_displayed_line_ = cursor_;

        return true;
    }

    unsigned int get_cursor() const
    {
        return cursor_;
    }

    class const_iterator
    {
      private:
        const_iterator &operator=(const const_iterator &);

        unsigned int line_;

      public:
        explicit const_iterator(unsigned int line): line_(line) {}

        unsigned int operator*() const
        {
            return line_;
        }

        const_iterator &operator++()
        {
            ++line_;
            return *this;
        }

        bool operator!=(const const_iterator &other) const
        {
            return line_ != other.line_;
        }
    };

    const_iterator begin() const
    {
        return const_iterator(first_displayed_line_);
    }

    const_iterator end() const
    {
        return const_iterator(get_last_line());
    }

  private:
    constexpr unsigned int get_last_line() const
    {
        /* no, we cannot use std::min() because then we cannot use constexpr
         * anymore */
        return 
            (first_displayed_line_ + maximum_number_of_displayed_lines_ < number_of_lines_)
            ? first_displayed_line_ + maximum_number_of_displayed_lines_
            : number_of_lines_;
    }
};

#endif /* !LISTNAV_HH */
