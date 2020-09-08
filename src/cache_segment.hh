/*
 * Copyright (C) 2019, 2020  T+A elektroakustik GmbH & Co. KG
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

#ifndef CACHE_SEGMENT_HH
#define CACHE_SEGMENT_HH

namespace List
{

enum class SegmentIntersection
{
    DISJOINT,
    EQUAL,
    TOP_REMAINS,
    BOTTOM_REMAINS,
    CENTER_REMAINS,
    INCLUDED_IN_OTHER,
};

class Segment
{
  private:
    unsigned int line_;
    unsigned int count_;

  public:
    Segment(Segment &&src):
        line_(src.line_),
        count_(src.count_)
    {
        src.count_ = 0;
    }

    Segment &operator=(const Segment &) = default;
    explicit Segment(const Segment &src) = default;

    explicit Segment(): Segment(0, 0) {}

    explicit Segment(unsigned int seg_line, unsigned int seg_size):
        line_(seg_line),
        count_(seg_size)
    {}

    bool operator==(const Segment &other) const
    {
        return
            (line_ == other.line_ && count_ == other.count_) ||
            (count_ == 0 && other.count_ == 0);
    }

    unsigned int line() const { return line_; }
    unsigned int beyond() const { return line_ + count_; }
    unsigned int size() const { return count_; }
    bool empty() const { return count_ == 0; }

    void clear() { count_ = 0; }

    void shrink_up(unsigned int s) { count_ -= s; }

    void shrink_down(unsigned int s)
    {
        line_ += s;
        count_ -= s;
    }

    SegmentIntersection
    intersection(const Segment &other, unsigned int &isize) const
    {
        /* special cases for empty intervals */
        if(count_ == 0)
        {
            isize = 0;

            if(other.count_ == 0)
                return (line_ == other.line_)
                    ? SegmentIntersection::EQUAL
                    : SegmentIntersection::DISJOINT;
            else
                return other.contains_line(line_)
                    ? SegmentIntersection::INCLUDED_IN_OTHER
                    : SegmentIntersection::DISJOINT;
        }
        else if(other.count_ == 0)
        {
            isize = 0;
            return contains_line(other.line_)
                ? SegmentIntersection::CENTER_REMAINS
                : SegmentIntersection::DISJOINT;
        }

        /* neither interval is empty, i.e., both counts are positive */
        if(line_ == other.line_)
        {
            /* equal start lines */
            if(count_ < other.count_)
            {
                isize = count_;
                return SegmentIntersection::INCLUDED_IN_OTHER;
            }
            else if(count_ > other.count_)
            {
                isize = other.count_;
                return SegmentIntersection::TOP_REMAINS;
            }
            else
            {
                isize = count_;
                return SegmentIntersection::EQUAL;
            }
        }

        /* have two non-empty intervals with different start lines */
        const unsigned int beyond_this_end = line_ + count_;
        const unsigned int beyond_other_end = other.line_ + other.count_;

        if(line_ < other.line_)
        {
            /* this interval starts before the other interval */
            if(beyond_this_end <= other.line_)
            {
                isize = 0;
                return SegmentIntersection::DISJOINT;
            }
            else if(beyond_this_end <= beyond_other_end)
            {
                isize = beyond_this_end - other.line_;
                return SegmentIntersection::BOTTOM_REMAINS;
            }
            else
            {
                isize = other.count_;
                return SegmentIntersection::CENTER_REMAINS;
            }
        }
        else
        {
            /* this interval starts after the other interval */
            if(beyond_other_end <= line_)
            {
                isize = 0;
                return SegmentIntersection::DISJOINT;
            }
            else if(beyond_other_end < beyond_this_end)
            {
                isize = beyond_other_end - line_;
                return SegmentIntersection::TOP_REMAINS;
            }
            else
            {
                isize = count_;
                return SegmentIntersection::INCLUDED_IN_OTHER;
            }
        }
    }

    bool contains_line(unsigned int n) const
    {
        return n >= line_ && n < line_ + count_;
    }
};

enum class CacheSegmentState
{
    /*! Nothing in cache yet, nothing loading. */
    EMPTY,

    /*! The whole segment is being loaded, nothing cached yet. */
    LOADING,

    /*! Top segment is loading, bottom half is empty. */
    LOADING_TOP_EMPTY_BOTTOM,

    /*! Bottom segment is loading, top half is empty. */
    LOADING_BOTTOM_EMPTY_TOP,

    /*! Loading in center, mix of other states at top and bottom. */
    LOADING_CENTER,

    /*! Segment is completely in cache. */
    CACHED,

    /*! Only top of segment is cached, bottom half is already loading. */
    CACHED_TOP_LOADING_BOTTOM,

    /*! Only bottom of segment is cached, top half is already loading. */
    CACHED_BOTTOM_LOADING_TOP,

    /*! Top segment is cached, bottom half is empty. */
    CACHED_TOP_EMPTY_BOTTOM,

    /*! Bottom segment is cached, top half is empty. */
    CACHED_BOTTOM_EMPTY_TOP,

    /*! Top segment is cached, center is loading, bottom half is empty. */
    CACHED_TOP_LOADING_CENTER_EMPTY_BOTTOM,

    /*! Bottom segment is cached, center is loading, top half is empty. */
    CACHED_BOTTOM_LOADING_CENTER_EMPTY_TOP,

    /*! Cached in center, mix of other states at top and bottom. */
    CACHED_CENTER,
};

}

#endif /* !CACHE_SEGMENT_HH */
