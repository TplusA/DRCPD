/*
 * Copyright (C) 2016, 2019, 2020  T+A elektroakustik GmbH & Co. KG
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

#include <cppcutter.h>

#include "dbuslist.hh"

namespace cache_segment_tests
{

void test_intersection_of_disjoint_segments()
{
    List::Segment a(0, 5);
    List::Segment b(5, 1);
    List::Segment c(6, 1);

    static constexpr const unsigned int expected_size = 0;
    unsigned int size;

    size = UINT_MAX;
    cppcut_assert_equal(int(List::SegmentIntersection::DISJOINT),
                        int(a.intersection(b, size)));
    cppcut_assert_equal(expected_size, size);
    size = UINT_MAX;
    cppcut_assert_equal(int(List::SegmentIntersection::DISJOINT),
                        int(b.intersection(a, size)));
    cppcut_assert_equal(expected_size, size);

    size = UINT_MAX;
    cppcut_assert_equal(int(List::SegmentIntersection::DISJOINT),
                        int(a.intersection(c, size)));
    cppcut_assert_equal(expected_size, size);
    size = UINT_MAX;
    cppcut_assert_equal(int(List::SegmentIntersection::DISJOINT),
                        int(c.intersection(a, size)));
    cppcut_assert_equal(expected_size, size);

    size = UINT_MAX;
    cppcut_assert_equal(int(List::SegmentIntersection::DISJOINT),
                        int(b.intersection(c, size)));
    cppcut_assert_equal(expected_size, size);
    size = UINT_MAX;
    cppcut_assert_equal(int(List::SegmentIntersection::DISJOINT),
                        int(c.intersection(b, size)));
    cppcut_assert_equal(expected_size, size);
}

void test_intersection_of_equal_segments()
{
    List::Segment a(6, 2);
    List::Segment b(6, 2);

    static constexpr const unsigned int expected_size = 2;
    unsigned int size;

    size = 0;
    cppcut_assert_equal(int(List::SegmentIntersection::EQUAL),
                        int(a.intersection(b, size)));
    cppcut_assert_equal(expected_size, size);

    size = 0;
    cppcut_assert_equal(int(List::SegmentIntersection::EQUAL),
                        int(b.intersection(a, size)));
    cppcut_assert_equal(expected_size, size);
}

void test_intersection_of_properly_overlapping_segments()
{
    List::Segment a(10, 20);
    List::Segment b(15, 18);

    static constexpr const unsigned int expected_size = 15;
    unsigned int size;

    size = 0;
    cppcut_assert_equal(int(List::SegmentIntersection::BOTTOM_REMAINS),
                        int(a.intersection(b, size)));
    cppcut_assert_equal(expected_size, size);

    size = 0;
    cppcut_assert_equal(int(List::SegmentIntersection::TOP_REMAINS),
                        int(b.intersection(a, size)));
    cppcut_assert_equal(expected_size, size);
}

void test_intersection_of_overlapping_segments_with_same_start_line()
{
    List::Segment a(5, 9);
    List::Segment b(5, 10);

    static constexpr const unsigned int expected_size = 9;
    unsigned int size;

    size = 0;
    cppcut_assert_equal(int(List::SegmentIntersection::INCLUDED_IN_OTHER),
                        int(a.intersection(b, size)));
    cppcut_assert_equal(expected_size, size);

    size = 0;
    cppcut_assert_equal(int(List::SegmentIntersection::TOP_REMAINS),
                        int(b.intersection(a, size)));
    cppcut_assert_equal(expected_size, size);
}

void test_intersection_of_overlapping_segments_with_same_end_line()
{
    List::Segment a(17, 3);
    List::Segment b(15, 5);

    static constexpr const unsigned int expected_size = 3;
    unsigned int size;

    size = 0;
    cppcut_assert_equal(int(List::SegmentIntersection::INCLUDED_IN_OTHER),
                        int(a.intersection(b, size)));
    cppcut_assert_equal(expected_size, size);

    size = 0;
    cppcut_assert_equal(int(List::SegmentIntersection::BOTTOM_REMAINS),
                        int(b.intersection(a, size)));
    cppcut_assert_equal(expected_size, size);
}

void test_intersection_of_embedded_segments()
{
    List::Segment a(11, 10);
    List::Segment b(14, 5);

    static constexpr const unsigned int expected_size = 5;
    unsigned int size;

    size = 0;
    cppcut_assert_equal(int(List::SegmentIntersection::CENTER_REMAINS),
                        int(a.intersection(b, size)));
    cppcut_assert_equal(expected_size, size);

    size = 0;
    cppcut_assert_equal(int(List::SegmentIntersection::INCLUDED_IN_OTHER),
                        int(b.intersection(a, size)));
    cppcut_assert_equal(expected_size, size);
}

void test_intersection_of_empty_segments()
{
    List::Segment a(1, 0);
    List::Segment b(2, 0);

    static constexpr const unsigned int expected_size = 0;
    unsigned int size;

    size = UINT_MAX;
    cppcut_assert_equal(int(List::SegmentIntersection::DISJOINT),
                        int(a.intersection(b, size)));
    cppcut_assert_equal(expected_size, size);

    size = UINT_MAX;
    cppcut_assert_equal(int(List::SegmentIntersection::DISJOINT),
                        int(b.intersection(a, size)));
    cppcut_assert_equal(expected_size, size);

    size = UINT_MAX;
    cppcut_assert_equal(int(List::SegmentIntersection::EQUAL),
                        int(a.intersection(a, size)));
    cppcut_assert_equal(expected_size, size);
}

void test_intersection_with_one_empty_segment()
{
    List::Segment a(5, 10);
    List::Segment empty_a(4, 0);
    List::Segment empty_b(15, 0);
    List::Segment empty_c(5, 0);
    List::Segment empty_d(10, 0);
    List::Segment empty_e(14, 0);

    static constexpr const unsigned int expected_size = 0;
    unsigned int size;

    size = UINT_MAX;
    cppcut_assert_equal(int(List::SegmentIntersection::DISJOINT),
                        int(a.intersection(empty_a, size)));
    cppcut_assert_equal(expected_size, size);

    size = UINT_MAX;
    cppcut_assert_equal(int(List::SegmentIntersection::DISJOINT),
                        int(a.intersection(empty_b, size)));
    cppcut_assert_equal(expected_size, size);

    size = UINT_MAX;
    cppcut_assert_equal(int(List::SegmentIntersection::CENTER_REMAINS),
                        int(a.intersection(empty_c, size)));
    cppcut_assert_equal(expected_size, size);

    size = UINT_MAX;
    cppcut_assert_equal(int(List::SegmentIntersection::CENTER_REMAINS),
                        int(a.intersection(empty_d, size)));
    cppcut_assert_equal(expected_size, size);

    size = UINT_MAX;
    cppcut_assert_equal(int(List::SegmentIntersection::CENTER_REMAINS),
                        int(a.intersection(empty_e, size)));
    cppcut_assert_equal(expected_size, size);

    size = UINT_MAX;
    cppcut_assert_equal(int(List::SegmentIntersection::DISJOINT),
                        int(empty_a.intersection(a, size)));
    cppcut_assert_equal(expected_size, size);

    size = UINT_MAX;
    cppcut_assert_equal(int(List::SegmentIntersection::DISJOINT),
                        int(empty_b.intersection(a, size)));
    cppcut_assert_equal(expected_size, size);

    size = UINT_MAX;
    cppcut_assert_equal(int(List::SegmentIntersection::INCLUDED_IN_OTHER),
                        int(empty_c.intersection(a, size)));
    cppcut_assert_equal(expected_size, size);

    size = UINT_MAX;
    cppcut_assert_equal(int(List::SegmentIntersection::INCLUDED_IN_OTHER),
                        int(empty_d.intersection(a, size)));
    cppcut_assert_equal(expected_size, size);

    size = UINT_MAX;
    cppcut_assert_equal(int(List::SegmentIntersection::INCLUDED_IN_OTHER),
                        int(empty_e.intersection(a, size)));
    cppcut_assert_equal(expected_size, size);
}

}
