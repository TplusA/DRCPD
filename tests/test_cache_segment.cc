/*
 * Copyright (C) 2016, 2019  T+A elektroakustik GmbH & Co. KG
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
    List::CacheSegment a(0, 5);
    List::CacheSegment b(5, 1);
    List::CacheSegment c(6, 1);

    static constexpr const unsigned int expected_size = 0;
    unsigned int size;

    size = UINT_MAX;
    cppcut_assert_equal(List::CacheSegment::DISJOINT, a.intersection(b, size));
    cppcut_assert_equal(expected_size, size);
    size = UINT_MAX;
    cppcut_assert_equal(List::CacheSegment::DISJOINT, b.intersection(a, size));
    cppcut_assert_equal(expected_size, size);

    size = UINT_MAX;
    cppcut_assert_equal(List::CacheSegment::DISJOINT, a.intersection(c, size));
    cppcut_assert_equal(expected_size, size);
    size = UINT_MAX;
    cppcut_assert_equal(List::CacheSegment::DISJOINT, c.intersection(a, size));
    cppcut_assert_equal(expected_size, size);

    size = UINT_MAX;
    cppcut_assert_equal(List::CacheSegment::DISJOINT, b.intersection(c, size));
    cppcut_assert_equal(expected_size, size);
    size = UINT_MAX;
    cppcut_assert_equal(List::CacheSegment::DISJOINT, c.intersection(b, size));
    cppcut_assert_equal(expected_size, size);
}

void test_intersection_of_equal_segments()
{
    List::CacheSegment a(6, 2);
    List::CacheSegment b(6, 2);

    static constexpr const unsigned int expected_size = 2;
    unsigned int size;

    size = 0;
    cppcut_assert_equal(List::CacheSegment::EQUAL, a.intersection(b, size));
    cppcut_assert_equal(expected_size, size);

    size = 0;
    cppcut_assert_equal(List::CacheSegment::EQUAL, b.intersection(a, size));
    cppcut_assert_equal(expected_size, size);
}

void test_intersection_of_properly_overlapping_segments()
{
    List::CacheSegment a(10, 20);
    List::CacheSegment b(15, 18);

    static constexpr const unsigned int expected_size = 15;
    unsigned int size;

    size = 0;
    cppcut_assert_equal(List::CacheSegment::BOTTOM_REMAINS, a.intersection(b, size));
    cppcut_assert_equal(expected_size, size);

    size = 0;
    cppcut_assert_equal(List::CacheSegment::TOP_REMAINS, b.intersection(a, size));
    cppcut_assert_equal(expected_size, size);
}

void test_intersection_of_overlapping_segments_with_same_start_line()
{
    List::CacheSegment a(5, 9);
    List::CacheSegment b(5, 10);

    static constexpr const unsigned int expected_size = 9;
    unsigned int size;

    size = 0;
    cppcut_assert_equal(List::CacheSegment::INCLUDED_IN_OTHER, a.intersection(b, size));
    cppcut_assert_equal(expected_size, size);

    size = 0;
    cppcut_assert_equal(List::CacheSegment::TOP_REMAINS, b.intersection(a, size));
    cppcut_assert_equal(expected_size, size);
}

void test_intersection_of_overlapping_segments_with_same_end_line()
{
    List::CacheSegment a(17, 3);
    List::CacheSegment b(15, 5);

    static constexpr const unsigned int expected_size = 3;
    unsigned int size;

    size = 0;
    cppcut_assert_equal(List::CacheSegment::INCLUDED_IN_OTHER, a.intersection(b, size));
    cppcut_assert_equal(expected_size, size);

    size = 0;
    cppcut_assert_equal(List::CacheSegment::BOTTOM_REMAINS, b.intersection(a, size));
    cppcut_assert_equal(expected_size, size);
}

void test_intersection_of_embedded_segments()
{
    List::CacheSegment a(11, 10);
    List::CacheSegment b(14, 5);

    static constexpr const unsigned int expected_size = 5;
    unsigned int size;

    size = 0;
    cppcut_assert_equal(List::CacheSegment::CENTER_REMAINS, a.intersection(b, size));
    cppcut_assert_equal(expected_size, size);

    size = 0;
    cppcut_assert_equal(List::CacheSegment::INCLUDED_IN_OTHER, b.intersection(a, size));
    cppcut_assert_equal(expected_size, size);
}

void test_intersection_of_empty_segments()
{
    List::CacheSegment a(1, 0);
    List::CacheSegment b(2, 0);

    static constexpr const unsigned int expected_size = 0;
    unsigned int size;

    size = UINT_MAX;
    cppcut_assert_equal(List::CacheSegment::DISJOINT, a.intersection(b, size));
    cppcut_assert_equal(expected_size, size);

    size = UINT_MAX;
    cppcut_assert_equal(List::CacheSegment::DISJOINT, b.intersection(a, size));
    cppcut_assert_equal(expected_size, size);

    size = UINT_MAX;
    cppcut_assert_equal(List::CacheSegment::EQUAL, a.intersection(a, size));
    cppcut_assert_equal(expected_size, size);
}

void test_intersection_with_one_empty_segment()
{
    List::CacheSegment a(5, 10);
    List::CacheSegment empty_a(4, 0);
    List::CacheSegment empty_b(15, 0);
    List::CacheSegment empty_c(5, 0);
    List::CacheSegment empty_d(10, 0);
    List::CacheSegment empty_e(14, 0);

    static constexpr const unsigned int expected_size = 0;
    unsigned int size;

    size = UINT_MAX;
    cppcut_assert_equal(List::CacheSegment::DISJOINT, a.intersection(empty_a, size));
    cppcut_assert_equal(expected_size, size);

    size = UINT_MAX;
    cppcut_assert_equal(List::CacheSegment::DISJOINT, a.intersection(empty_b, size));
    cppcut_assert_equal(expected_size, size);

    size = UINT_MAX;
    cppcut_assert_equal(List::CacheSegment::CENTER_REMAINS, a.intersection(empty_c, size));
    cppcut_assert_equal(expected_size, size);

    size = UINT_MAX;
    cppcut_assert_equal(List::CacheSegment::CENTER_REMAINS, a.intersection(empty_d, size));
    cppcut_assert_equal(expected_size, size);

    size = UINT_MAX;
    cppcut_assert_equal(List::CacheSegment::CENTER_REMAINS, a.intersection(empty_e, size));
    cppcut_assert_equal(expected_size, size);

    size = UINT_MAX;
    cppcut_assert_equal(List::CacheSegment::DISJOINT, empty_a.intersection(a, size));
    cppcut_assert_equal(expected_size, size);

    size = UINT_MAX;
    cppcut_assert_equal(List::CacheSegment::DISJOINT, empty_b.intersection(a, size));
    cppcut_assert_equal(expected_size, size);

    size = UINT_MAX;
    cppcut_assert_equal(List::CacheSegment::INCLUDED_IN_OTHER, empty_c.intersection(a, size));
    cppcut_assert_equal(expected_size, size);

    size = UINT_MAX;
    cppcut_assert_equal(List::CacheSegment::INCLUDED_IN_OTHER, empty_d.intersection(a, size));
    cppcut_assert_equal(expected_size, size);

    size = UINT_MAX;
    cppcut_assert_equal(List::CacheSegment::INCLUDED_IN_OTHER, empty_e.intersection(a, size));
    cppcut_assert_equal(expected_size, size);
}

}
