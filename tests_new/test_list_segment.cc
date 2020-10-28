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

#include <doctest.h>
#include <climits>

#include "cache_segment.hh"

TEST_SUITE_BEGIN("List segment");

TEST_CASE("Intersection of disjoint segments is empty")
{
    List::Segment a(0, 5);
    List::Segment b(5, 1);
    List::Segment c(6, 1);

    static constexpr const unsigned int expected_size = 0;
    unsigned int size;

    size = UINT_MAX;
    CHECK(a.intersection(b, size) == List::SegmentIntersection::DISJOINT);
    CHECK(size == expected_size);
    size = UINT_MAX;
    CHECK(b.intersection(a, size) == List::SegmentIntersection::DISJOINT);
    CHECK(size == expected_size);

    size = UINT_MAX;
    CHECK(a.intersection(c, size) == List::SegmentIntersection::DISJOINT);
    CHECK(size == expected_size);
    size = UINT_MAX;
    CHECK(c.intersection(a, size) == List::SegmentIntersection::DISJOINT);
    CHECK(size == expected_size);

    size = UINT_MAX;
    CHECK(b.intersection(c, size) == List::SegmentIntersection::DISJOINT);
    CHECK(size == expected_size);
    size = UINT_MAX;
    CHECK(c.intersection(b, size) == List::SegmentIntersection::DISJOINT);
    CHECK(size == expected_size);
}

TEST_CASE("Intersection of equal segments")
{
    List::Segment a(6, 2);
    List::Segment b(6, 2);

    static constexpr const unsigned int expected_size = 2;
    unsigned int size;

    size = 0;
    CHECK(a.intersection(b, size) == List::SegmentIntersection::EQUAL);
    CHECK(size == expected_size);

    size = 0;
    CHECK(b.intersection(a, size) == List::SegmentIntersection::EQUAL);
    CHECK(size == expected_size);
}

TEST_CASE("Intersection of properly overlapping segments is non-empty")
{
    List::Segment a(10, 20);
    List::Segment b(15, 18);

    static constexpr const unsigned int expected_size = 15;
    unsigned int size;

    size = 0;
    CHECK(a.intersection(b, size) == List::SegmentIntersection::BOTTOM_REMAINS);
    CHECK(size == expected_size);

    size = 0;
    CHECK(b.intersection(a, size) == List::SegmentIntersection::TOP_REMAINS);
    CHECK(size == expected_size);
}

TEST_CASE("Intersection of overlapping segments with same start line is non-empty")
{
    List::Segment a(5, 9);
    List::Segment b(5, 10);

    static constexpr const unsigned int expected_size = 9;
    unsigned int size;

    size = 0;
    CHECK(a.intersection(b, size) == List::SegmentIntersection::INCLUDED_IN_OTHER);
    CHECK(size == expected_size);

    size = 0;
    CHECK(b.intersection(a, size) == List::SegmentIntersection::TOP_REMAINS);
    CHECK(size == expected_size);
}

TEST_CASE("Intersection of overlapping segments with same end line is non-empty")
{
    List::Segment a(17, 3);
    List::Segment b(15, 5);

    static constexpr const unsigned int expected_size = 3;
    unsigned int size;

    size = 0;
    CHECK(a.intersection(b, size) == List::SegmentIntersection::INCLUDED_IN_OTHER);
    CHECK(size == expected_size);

    size = 0;
    CHECK(b.intersection(a, size) == List::SegmentIntersection::BOTTOM_REMAINS);
    CHECK(size == expected_size);
}

TEST_CASE("Intersection of embedded segments is non-empty")
{
    List::Segment a(11, 10);
    List::Segment b(14, 5);

    static constexpr const unsigned int expected_size = 5;
    unsigned int size;

    size = 0;
    CHECK(a.intersection(b, size) == List::SegmentIntersection::CENTER_REMAINS);
    CHECK(size == expected_size);

    size = 0;
    CHECK(b.intersection(a, size) == List::SegmentIntersection::INCLUDED_IN_OTHER);
    CHECK(size == expected_size);
}

TEST_CASE("Intersection of empty segments with different start lines is empty and disjoint")
{
    List::Segment a(1, 0);
    List::Segment b(2, 0);

    static constexpr const unsigned int expected_size = 0;
    unsigned int size;

    size = UINT_MAX;
    CHECK(a.intersection(b, size) == List::SegmentIntersection::DISJOINT);
    CHECK(size == expected_size);

    size = UINT_MAX;
    CHECK(b.intersection(a, size) == List::SegmentIntersection::DISJOINT);
    CHECK(size == expected_size);
}

TEST_CASE("Intersection of empty segments with equal start lines is empty and equal")
{
    List::Segment a(1, 0);

    static constexpr const unsigned int expected_size = 0;
    unsigned int size;

    size = UINT_MAX;
    CHECK(a.intersection(a, size) == List::SegmentIntersection::EQUAL);
    CHECK(size == expected_size);
}

TEST_CASE("Intersection with one empty segment is empty, intersection depends on start line")
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
    CHECK(a.intersection(empty_a, size) == List::SegmentIntersection::DISJOINT);
    CHECK(size == expected_size);

    size = UINT_MAX;
    CHECK(a.intersection(empty_b, size) == List::SegmentIntersection::DISJOINT);
    CHECK(size == expected_size);

    size = UINT_MAX;
    CHECK(a.intersection(empty_c, size) == List::SegmentIntersection::CENTER_REMAINS);
    CHECK(size == expected_size);

    size = UINT_MAX;
    CHECK(a.intersection(empty_d, size) == List::SegmentIntersection::CENTER_REMAINS);
    CHECK(size == expected_size);

    size = UINT_MAX;
    CHECK(a.intersection(empty_e, size) == List::SegmentIntersection::CENTER_REMAINS);
    CHECK(size == expected_size);

    size = UINT_MAX;
    CHECK(empty_a.intersection(a, size) == List::SegmentIntersection::DISJOINT);
    CHECK(size == expected_size);

    size = UINT_MAX;
    CHECK(empty_b.intersection(a, size) == List::SegmentIntersection::DISJOINT);
    CHECK(size == expected_size);

    size = UINT_MAX;
    CHECK(empty_c.intersection(a, size) == List::SegmentIntersection::INCLUDED_IN_OTHER);
    CHECK(size == expected_size);

    size = UINT_MAX;
    CHECK(empty_d.intersection(a, size) == List::SegmentIntersection::INCLUDED_IN_OTHER);
    CHECK(size == expected_size);

    size = UINT_MAX;
    CHECK(empty_e.intersection(a, size) == List::SegmentIntersection::INCLUDED_IN_OTHER);
    CHECK(size == expected_size);
}

TEST_SUITE_END();
