/*
 * Copyright (C) 2016, 2019, 2020, 2022  T+A elektroakustik GmbH & Co. KG
 *
 * This file is part of DRCPD.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301, USA.
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "search_algo.hh"

#include <glib.h>
#include <string.h>

/*!
 * RAII wrapper around string allocated by GLib.
 */
class ComparedString
{
  private:
    char *string_;
    size_t number_of_utf8_chars_;

  public:
    ComparedString(const ComparedString &) = delete;
    ComparedString &operator=(const ComparedString &) = delete;

    explicit ComparedString() throw():
        string_(nullptr),
        number_of_utf8_chars_(0)
    {}

    explicit ComparedString(const char *string, size_t number_of_bytes) throw():
        string_(nullptr)
    {
        set(string, number_of_bytes);
    }

    ~ComparedString() throw() { g_free(string_); }

    void set(const char *string) throw() { set(string, strlen(string)); }

    void set(const char *string, size_t number_of_bytes) throw()
    {
        const glong len = g_utf8_strlen(string, number_of_bytes);

        g_free(string_);

        if(len > 0)
        {
            string_ = g_utf8_casefold(string, number_of_bytes);
            number_of_utf8_chars_ = len;
        }
        else
        {
            string_ = nullptr;
            number_of_utf8_chars_ = 0;
        }
    }

    const char *get() const throw() { return string_; }
    size_t length() const throw() { return number_of_utf8_chars_; }

    gunichar get_char_at(size_t idx) const throw()
    {
        msg_log_assert(idx < number_of_utf8_chars_);
        return g_utf8_get_char(g_utf8_offset_to_pointer(string_, idx));
    }
};

/*!
 * Pull string from D-Bus list, convert for case-insensitive comparison.
 */
static bool get_casefolded_string(List::DBusList &list,
                                  std::shared_ptr<List::DBusListViewport> vp,
                                  unsigned int position,
                                  ComparedString &string)
{
    const List::TextItem *const item =
        dynamic_cast<const List::TextItem *>(list.get_item(vp, position));

    if(item == nullptr)
    {
        MSG_BUG("List item %u does not exist", position);
        return false;
    }

    const char *item_text = item->get_text();

    if(item_text == nullptr)
    {
        MSG_BUG("List item %u contains nullptr text", position);
        return false;
    }

    string.set(item_text);

    return true;
}

/*!
 * Simple iterator-like wrapper around a #ComparedString.
 */
class Needle
{
  private:
    const ComparedString needle_;

    size_t next_char_index_;
    const char *next_char_;

  public:
    Needle(const Needle &) = delete;
    Needle &operator=(const Needle &) = delete;

    explicit Needle(const char *needle, size_t needle_bytes) throw():
        needle_(needle, needle_bytes),
        next_char_index_(0),
        next_char_(needle_.get())
    {}

    const char *next_utf8_char() throw()
    {
        const char *ret = next_char_;

        if(next_char_ != nullptr)
        {
            ++next_char_index_;

            next_char_ = ((next_char_index_ < needle_.length())
                          ? g_utf8_next_char(next_char_)
                          : nullptr);
        }

        return ret;
    }
};

/*!
 * How to compare strings that are proper prefixes of other strings.
 */
struct ProperPrefixPolicy
{
    static inline bool proper_prefix_is_smaller_than_whole_string() { return true; }
};

/*!
 * How to search for the top-most boundary (first match) of the partition
 * defined by the search string.
 */
struct TopMostBoundaryTraits
{
    constexpr static inline bool want_top_most_boundary() throw() { return true; }

    /*!
     * Whether or not the given character should be considered greater than the
     * search key.
     *
     * \param key
     *     The character we are trying to find the partition for.
     *
     * \param ch
     *     The character within the string currently checked during the binary
     *     search.
     */
    static inline bool is_utf8_character_greater(gunichar key, gunichar ch) throw()
    {
        return key <= ch;
    }
};

/*!
 * How to search for the bottom-most boundary (last match) of the partition
 * defined by the search string.
 *
 * \see #TopMostBoundaryTraits
 */
struct BottomMostBoundaryTraits
{
    constexpr static inline bool want_top_most_boundary() throw() { return false; }
    static inline bool is_utf8_character_greater(gunichar key, gunichar ch) throw() { return key < ch; }
};

/*!
 * Core implementation of binary search algorithm and search state.
 */
class BSearchState
{
  public:
    enum class Result
    {
        INTERNAL_FAILURE,
        SEARCHING,
        CHECKING_BOUNDARY,
        FOUND_MATCH,
        FOUND_APPROXIMATE,
    };

  private:
    /*!
     * A partition used by the binary search algorithm.
     */
    struct Partition
    {
        unsigned int top_;
        unsigned int bottom_;
        unsigned int center_;

        Partition(const Partition &) = delete;
        Partition &operator=(const Partition &) = delete;

        explicit Partition(): top_(UINT_MAX), bottom_(0), center_(0) {}

        bool is_unique() const throw() { return top_ == bottom_; }
        bool is_empty() const throw() { return top_ > bottom_; }
        unsigned int size() const throw() { return bottom_ - top_ + 1; }

        /*!
         * Move center index one towards the beginning of the partition.
         */
        void step_center_up() throw()
        {
            msg_log_assert(center_ > top_);
            --center_;
        }

        /*!
         * Move center index one towards the end of the partition.
         */
        void step_center_down() throw()
        {
            msg_log_assert(center_ < bottom_);
            ++center_;
        }

        /*!
         * Bisect such that the top half is searched next.
         */
        Result pick_top_half(unsigned int sub) throw()
        {
            bottom_ = center_ - sub;
            return update_center();
        }

        /*!
         * Bisect such that the bottom half is searched next.
         */
        Result pick_bottom_half(unsigned int add) throw()
        {
            top_ = center_ + add;
            return update_center();
        }

        /*!
         * Compute center index.
         *
         * Public function for speed in client code.
         */
        unsigned int compute_center() const throw()
        {
            return top_ + (bottom_ - top_) / 2;
        }

        /*!
         * Declare center element the only element in the partition.
         */
        void lock_at_center() throw() { top_ = bottom_ = center_; }

      private:
        Result update_center() throw()
        {
            if(!is_unique())
            {
                center_ = compute_center();
                return Result::SEARCHING;
            }
            else
            {
                center_ = top_;
                return Result::CHECKING_BOUNDARY;
            }
        }
    };

    const std::shared_ptr<List::DBusListViewport> viewport_;

    Partition upper_;
    Partition lower_;

    unsigned int all_top_;
    unsigned int all_bottom_;
    unsigned int bottom_candidate_;
    gunichar utf8_key_;
    size_t depth_;

  public:
    BSearchState(const BSearchState &) = delete;
    BSearchState &operator=(const BSearchState &) = delete;

    explicit BSearchState(const List::DBusList &list, unsigned int elements) throw():
        viewport_(list.mk_viewport(1, "binary search")),
        all_top_(elements > 0 ? 0 : UINT_MAX),
        all_bottom_(elements > 0 ? elements - 1 : 0),
        bottom_candidate_(UINT_MAX),
        utf8_key_(0),
        depth_(-1)
    {
        msg_vinfo(MESSAGE_LEVEL_DEBUG,
                  "Starting binary search in partition [%u, %u]",
                  all_top_, all_bottom_);
    }

    void prepare_next_iteration(const char *utf8_char) throw()
    {
        upper_.top_ = lower_.top_ = all_top_;
        upper_.bottom_ = lower_.bottom_ = all_bottom_;
        upper_.center_ = lower_.center_ = upper_.compute_center();
        bottom_candidate_ = UINT_MAX;
        utf8_key_ = g_utf8_get_char(utf8_char);
        ++depth_;

        msg_vinfo(MESSAGE_LEVEL_DEBUG,
                  "BSEARCH: ----------------------------------------");
        msg_vinfo(MESSAGE_LEVEL_DEBUG,
                  "BSEARCH: Partition [%u, %u], center %u, "
                  "character U+%04" G_GINT32_MODIFIER "X at depth %zu",
                  all_top_, all_bottom_, upper_.center_, utf8_key_, depth_);
    }

    ssize_t get_result() const throw()
    {
        return upper_.is_empty() ? -1 : upper_.top_;
    }

    void prepare_for_next_character() throw()
    {
        all_top_ = upper_.top_;
        all_bottom_ = lower_.bottom_;
    }

    Result bsearch_top_most(List::DBusList &list, ComparedString &temp_string)
    {
        while(true)
        {
            if(!get_casefolded_string(list, viewport_, upper_.center_, temp_string))
                return Result::INTERNAL_FAILURE;

            dump_state("before iteration", true);

            const Result result =
                bsearch_boundary<TopMostBoundaryTraits, ProperPrefixPolicy>(temp_string);

            dump_state("after iteration", true);

            switch(result)
            {
              case Result::SEARCHING:
              case Result::CHECKING_BOUNDARY:
                break;

              case Result::INTERNAL_FAILURE:
              case Result::FOUND_MATCH:
              case Result::FOUND_APPROXIMATE:
                dump_state("determined upper boundary", true);
                return result;
            }
        }
    }

    Result bsearch_bottom_most(List::DBusList &list, ComparedString &temp_string)
    {
        while(true)
        {
            if(!get_casefolded_string(list, viewport_, lower_.center_, temp_string))
                return Result::INTERNAL_FAILURE;

            dump_state("before iteration", false);

            const Result result =
                bsearch_boundary<BottomMostBoundaryTraits, ProperPrefixPolicy>(temp_string);

            dump_state("after iteration", false);

            switch(result)
            {
              case Result::SEARCHING:
              case Result::CHECKING_BOUNDARY:
                break;

              case Result::INTERNAL_FAILURE:
              case Result::FOUND_MATCH:
                dump_state("determined lower boundary", false);
                return result;

              case Result::FOUND_APPROXIMATE:
                MSG_BUG("Bogus approximate match for bottom partition boundary");
                return Result::INTERNAL_FAILURE;
            }
        }
    }

  private:
    template <typename CompareTraits, typename PrefixPolicy>
    Result bsearch_boundary(const ComparedString &center_string)
    {
        msg_vinfo(MESSAGE_LEVEL_DEBUG,
                  "BSEARCH: Center element \"%s\", length %zu",
                  center_string.get(), center_string.length());

        Partition &p(CompareTraits::want_top_most_boundary() ? upper_ : lower_);

        msg_log_assert(!p.is_empty());

        if(center_string.length() < depth_)
        {
            /* center string is a proper prefix of the search string */
            return PrefixPolicy::proper_prefix_is_smaller_than_whole_string()
                ? p.pick_top_half(0)
                : p.pick_bottom_half(0);
        }

        const gunichar ch = center_string.get_char_at(depth_);
        msg_vinfo(MESSAGE_LEVEL_DEBUG,
                  "BSEARCH: Decide on character U+%04" G_GINT32_MODIFIER "X "
                  "(ref U+%04" G_GINT32_MODIFIER "X)",
                  ch, utf8_key_);

        if(p.is_unique())
        {
            msg_vinfo(MESSAGE_LEVEL_DEBUG, "BSEARCH: Final check on last item");
            return (utf8_key_ == ch) ? Result::FOUND_MATCH : Result::FOUND_APPROXIMATE;
        }

        if(p.size() == 2)
        {
            static bool shown_bug = false;

            if(!shown_bug)
            {
                MSG_BUG("The binary search should resort to linear search once "
                        "the searched partition becomes small");
                shown_bug = true;
            }

            if(utf8_key_ == ch)
            {
                if(CompareTraits::want_top_most_boundary() || p.center_ >= p.bottom_)
                {
                    p.lock_at_center();
                    return Result::FOUND_MATCH;
                }

                if(!CompareTraits::want_top_most_boundary())
                    bottom_candidate_ = p.center_;

                p.step_center_down();

                return Result::SEARCHING;
            }

            if(utf8_key_ > ch)
            {
                if(p.center_ < p.bottom_)
                {
                    p.step_center_down();
                    return Result::SEARCHING;
                }
                else
                {
                    p.lock_at_center();
                    return Result::CHECKING_BOUNDARY;
                }
            }

            if(CompareTraits::want_top_most_boundary())
            {
                p.lock_at_center();
                return Result::FOUND_APPROXIMATE;
            }
            else if(bottom_candidate_ != UINT_MAX)
            {
                p.center_ = bottom_candidate_;
                p.lock_at_center();
                return Result::FOUND_MATCH;
            }

            throw ::Search::UnsortedException();
        }

        if(CompareTraits::want_top_most_boundary())
        {
            if(CompareTraits::is_utf8_character_greater(ch, utf8_key_))
            {
                if(lower_.top_ < p.center_)
                    lower_.pick_bottom_half(0);
            }
            else if(lower_.bottom_ > p.center_)
                lower_.pick_top_half(0);
        }

        if(CompareTraits::is_utf8_character_greater(utf8_key_, ch))
        {
            /* center string is greater than or equal to partition key */
            msg_vinfo(MESSAGE_LEVEL_DEBUG, "BSEARCH: pick top half");

            return p.pick_top_half(0);
        }
        else
        {
            /* center string is smaller than or equal to partition key */
            msg_vinfo(MESSAGE_LEVEL_DEBUG, "BSEARCH: pick bottom half");

            if(CompareTraits::want_top_most_boundary())
            {
                if(utf8_key_ < ch)
                    lower_.top_ = p.center_;
            }

            return p.pick_bottom_half(0);
        }
    }

    void dump_state(const char *what, bool is_upper) const throw()
    {
        if(!msg_is_verbose(MESSAGE_LEVEL_DEBUG))
            return;

        const char *const upper_lower(is_upper ? "UPPER" : "LOWER");

        msg_info("BSEARCH %s %s: Upper partition [%u, %u], center %u",
                 upper_lower, what, upper_.top_,
                 upper_.bottom_, upper_.center_);
        msg_info("BSEARCH %s %s: Lower partition [%u, %u], center %u",
                 upper_lower, what,
                 lower_.top_, lower_.bottom_, lower_.center_);
    }
};

ssize_t Search::binary_search_utf8(List::DBusList &list, const std::string &query)
{
    if(query.empty())
        return -1;

    if(list.empty())
        return -1;

    Needle needle(query.c_str(), query.size());

    const char *next_utf8_char = needle.next_utf8_char();

    if(next_utf8_char == nullptr)
    {
        MSG_BUG("Expected at least one UTF-8 character");
        return -1;
    }

    BSearchState state(list, list.get_number_of_items());
    BSearchState::Result result = BSearchState::Result::INTERNAL_FAILURE;
    ComparedString temp_string;

    while(next_utf8_char != nullptr)
    {
        const char *const utf8_char = next_utf8_char;
        next_utf8_char = needle.next_utf8_char();

        state.prepare_next_iteration(utf8_char);
        result = state.bsearch_top_most(list, temp_string);

        msg_vinfo(MESSAGE_LEVEL_DEBUG, "Top-most result: %d",
                  static_cast<int>(result));

        msg_log_assert(result == BSearchState::Result::FOUND_MATCH ||
                       result == BSearchState::Result::FOUND_APPROXIMATE ||
                       result == BSearchState::Result::INTERNAL_FAILURE);

        if(result == BSearchState::Result::INTERNAL_FAILURE)
            return -1;

        if(result == BSearchState::Result::FOUND_APPROXIMATE)
            break;

        if(next_utf8_char != nullptr)
        {
            result = state.bsearch_bottom_most(list, temp_string);

            msg_vinfo(MESSAGE_LEVEL_DEBUG, "Bottom-most result: %d",
                      static_cast<int>(result));

            msg_log_assert(result == BSearchState::Result::FOUND_MATCH ||
                           result == BSearchState::Result::INTERNAL_FAILURE);

            if(result == BSearchState::Result::INTERNAL_FAILURE)
                return -1;

            state.prepare_for_next_character();
        }
    }

    return ((result != BSearchState::Result::INTERNAL_FAILURE)
            ? state.get_result()
            : -1);
}
