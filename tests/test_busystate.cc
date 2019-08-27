/*
 * Copyright (C) 2016, 2018, 2019  T+A elektroakustik GmbH & Co. KG
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

#include "busy.hh"

namespace counted_busy_state_tests
{

static bool current_busy_state;
static unsigned int number_of_state_changes;

static void state_changed(bool is_busy)
{
    cppcut_assert_not_equal(current_busy_state, is_busy);

    ++number_of_state_changes;
    current_busy_state = is_busy;
}

void cut_setup()
{
    current_busy_state = true;
    number_of_state_changes = 0;

    Busy::init(state_changed);

    cppcut_assert_equal(1U, number_of_state_changes);
    cut_assert_false(current_busy_state);
    number_of_state_changes = 0;
}

/*!\test
 * Setting a single busy source sets the busy state.
 */
void test_set_single_flag_causes_state_change()
{
    cut_assert_true(Busy::set(Busy::Source::BUFFERING_STREAM));
    cppcut_assert_equal(1U, number_of_state_changes);
    cut_assert_true(current_busy_state);
    cut_assert_true(Busy::is_busy());
}

/*!\test
 * Setting multiple busy sources sets the busy state once.
 */
void test_set_multiple_flag_causes_single_state_change()
{
    cut_assert_true(Busy::set(Busy::Source::BUFFERING_STREAM));
    cppcut_assert_equal(1U, number_of_state_changes);
    cut_assert_true(current_busy_state);
    cut_assert_true(Busy::is_busy());

    cut_assert_false(Busy::set(Busy::Source::FILLING_PLAYER_QUEUE));
    cut_assert_false(Busy::set(Busy::Source::GETTING_LIST_ID));

    cppcut_assert_equal(1U, number_of_state_changes);
    cut_assert_true(Busy::is_busy());
}

/*!\test
 * Setting a single busy source sets the busy state, clearing it clears the
 * busy state.
 */
void test_set_and_clear_single_flag_causes_two_state_changes()
{
    cut_assert_true(Busy::set(Busy::Source::BUFFERING_STREAM));
    cppcut_assert_equal(1U, number_of_state_changes);
    cut_assert_true(current_busy_state);
    cut_assert_true(Busy::is_busy());

    cut_assert_true(Busy::clear(Busy::Source::BUFFERING_STREAM));
    cppcut_assert_equal(2U, number_of_state_changes);
    cut_assert_false(current_busy_state);
    cut_assert_false(Busy::is_busy());
}

/*!\test
 * Setting and clearing a combination of busy sources toggles the busy state
 * only if necessary.
 */
void test_set_and_clear_multiple_flag_causes_minimal_number_of_state_changes()
{
    cut_assert_true(Busy::set(Busy::Source::BUFFERING_STREAM));
    cppcut_assert_equal(1U, number_of_state_changes);
    cut_assert_true(current_busy_state);
    cut_assert_true(Busy::is_busy());

    cut_assert_false(Busy::set(Busy::Source::GETTING_LIST_ID));
    cppcut_assert_equal(1U, number_of_state_changes);

    cut_assert_false(Busy::clear(Busy::Source::BUFFERING_STREAM));
    cppcut_assert_equal(1U, number_of_state_changes);

    cut_assert_false(Busy::set(Busy::Source::WAITING_FOR_PLAYER));
    cppcut_assert_equal(1U, number_of_state_changes);

    cut_assert_false(Busy::clear(Busy::Source::WAITING_FOR_PLAYER));
    cppcut_assert_equal(1U, number_of_state_changes);

    cut_assert_true(Busy::clear(Busy::Source::GETTING_LIST_ID));
    cppcut_assert_equal(2U, number_of_state_changes);
    cut_assert_false(current_busy_state);
    cut_assert_false(Busy::is_busy());
}

/*!\test
 * Setting a flag a number of times requires the same amount of clear
 * operations to change the busy state back to idle.
 */
void test_set_single_flag_multiple_times_requires_clearing_by_same_amount()
{
    cut_assert_true(Busy::set(Busy::Source::GETTING_LIST_ID));
    cppcut_assert_equal(1U, number_of_state_changes);
    cut_assert_true(current_busy_state);
    cut_assert_true(Busy::is_busy());

    cut_assert_false(Busy::set(Busy::Source::GETTING_LIST_ID));
    cut_assert_false(Busy::set(Busy::Source::GETTING_LIST_ID));
    cut_assert_false(Busy::set(Busy::Source::GETTING_LIST_ID));

    cut_assert_false(Busy::clear(Busy::Source::GETTING_LIST_ID));
    cut_assert_false(Busy::clear(Busy::Source::GETTING_LIST_ID));
    cut_assert_false(Busy::clear(Busy::Source::GETTING_LIST_ID));

    cppcut_assert_equal(1U, number_of_state_changes);

    cut_assert_true(Busy::clear(Busy::Source::GETTING_LIST_ID));
    cppcut_assert_equal(2U, number_of_state_changes);
    cut_assert_false(current_busy_state);
    cut_assert_false(Busy::is_busy());
}

/*!\test
 * Number of times a busy source got activated is maintained for each busy
 * source.
 */
void test_set_multiple_flags_multiple_times_requires_clearing_by_same_amount()
{
    cut_assert_true(Busy::set(Busy::Source::GETTING_LIST_ID));
    cppcut_assert_equal(1U, number_of_state_changes);
    cut_assert_true(current_busy_state);
    cut_assert_true(Busy::is_busy());

    cut_assert_false(Busy::set(Busy::Source::GETTING_LIST_ID));

    cut_assert_false(Busy::set(Busy::Source::BUFFERING_STREAM));
    cut_assert_false(Busy::set(Busy::Source::BUFFERING_STREAM));

    cut_assert_false(Busy::clear(Busy::Source::GETTING_LIST_ID));
    cut_assert_false(Busy::clear(Busy::Source::BUFFERING_STREAM));
    cut_assert_false(Busy::clear(Busy::Source::GETTING_LIST_ID));

    cppcut_assert_equal(1U, number_of_state_changes);

    cut_assert_true(Busy::clear(Busy::Source::BUFFERING_STREAM));
    cppcut_assert_equal(2U, number_of_state_changes);
    cut_assert_false(current_busy_state);
    cut_assert_false(Busy::is_busy());
}

}

namespace direct_busy_state_tests
{

static bool current_busy_state;
static unsigned int number_of_state_changes;

static void state_changed(bool is_busy)
{
    cppcut_assert_not_equal(current_busy_state, is_busy);

    ++number_of_state_changes;
    current_busy_state = is_busy;
}

void cut_setup()
{
    current_busy_state = true;
    number_of_state_changes = 0;

    Busy::init(state_changed);

    cppcut_assert_equal(1U, number_of_state_changes);
    cut_assert_false(current_busy_state);
    number_of_state_changes = 0;
}

/*!\test
 * Setting a single busy source sets the busy state.
 */
void test_set_single_flag_causes_state_change()
{
    cut_assert_true(Busy::set(Busy::DirectSource::WAITING_FOR_APPLIANCE_AUDIO));
    cppcut_assert_equal(1U, number_of_state_changes);
    cut_assert_true(current_busy_state);
    cut_assert_true(Busy::is_busy());
}

/*!\test
 * Setting a single busy source sets the busy state, clearing it clears the
 * busy state.
 */
void test_set_and_clear_single_flag_causes_two_state_changes()
{
    cut_assert_true(Busy::set(Busy::DirectSource::WAITING_FOR_APPLIANCE_AUDIO));
    cppcut_assert_equal(1U, number_of_state_changes);
    cut_assert_true(current_busy_state);
    cut_assert_true(Busy::is_busy());

    cut_assert_true(Busy::clear(Busy::DirectSource::WAITING_FOR_APPLIANCE_AUDIO));
    cppcut_assert_equal(2U, number_of_state_changes);
    cut_assert_false(current_busy_state);
    cut_assert_false(Busy::is_busy());
}

/*!\test
 * Setting a flag a number of times requires only one clear operation to change
 * the busy state back to idle.
 */
void test_set_single_flag_multiple_times_can_be_cleared_immediately()
{
    cut_assert_true(Busy::set(Busy::DirectSource::WAITING_FOR_APPLIANCE_AUDIO));
    cppcut_assert_equal(1U, number_of_state_changes);
    cut_assert_true(current_busy_state);
    cut_assert_true(Busy::is_busy());

    cut_assert_false(Busy::set(Busy::DirectSource::WAITING_FOR_APPLIANCE_AUDIO));
    cut_assert_false(Busy::set(Busy::DirectSource::WAITING_FOR_APPLIANCE_AUDIO));
    cut_assert_false(Busy::set(Busy::DirectSource::WAITING_FOR_APPLIANCE_AUDIO));

    cppcut_assert_equal(1U, number_of_state_changes);

    cut_assert_true(Busy::clear(Busy::DirectSource::WAITING_FOR_APPLIANCE_AUDIO));
    cppcut_assert_equal(2U, number_of_state_changes);
    cut_assert_false(current_busy_state);
    cut_assert_false(Busy::is_busy());
}

/*!\test
 * Clearing a flag a number of times is OK and requires only one set operation
 * to change the idle state back to busy.
 */
void test_clear_single_flag_multiple_times_can_be_set_immediately()
{
    cut_assert_true(Busy::set(Busy::DirectSource::WAITING_FOR_APPLIANCE_AUDIO));
    cppcut_assert_equal(1U, number_of_state_changes);

    cut_assert_true(Busy::clear(Busy::DirectSource::WAITING_FOR_APPLIANCE_AUDIO));
    cppcut_assert_equal(2U, number_of_state_changes);
    cut_assert_false(current_busy_state);
    cut_assert_false(Busy::is_busy());

    cut_assert_false(Busy::clear(Busy::DirectSource::WAITING_FOR_APPLIANCE_AUDIO));
    cut_assert_false(Busy::clear(Busy::DirectSource::WAITING_FOR_APPLIANCE_AUDIO));
    cut_assert_false(Busy::clear(Busy::DirectSource::WAITING_FOR_APPLIANCE_AUDIO));

    cut_assert_true(Busy::set(Busy::DirectSource::WAITING_FOR_APPLIANCE_AUDIO));
    cppcut_assert_equal(3U, number_of_state_changes);
    cut_assert_true(current_busy_state);
    cut_assert_true(Busy::is_busy());
}

}

namespace mixed_busy_state_tests
{

static bool current_busy_state;
static unsigned int number_of_state_changes;

static void state_changed(bool is_busy)
{
    cppcut_assert_not_equal(current_busy_state, is_busy);

    ++number_of_state_changes;
    current_busy_state = is_busy;
}

void cut_setup()
{
    current_busy_state = true;
    number_of_state_changes = 0;

    Busy::init(state_changed);

    cppcut_assert_equal(1U, number_of_state_changes);
    cut_assert_false(current_busy_state);
    number_of_state_changes = 0;
}

/*!\test
 * Setting a counted followed by a direct busy source sets the busy state once.
 */
void test_set_counted_followed_by_direct_flag_causes_single_state_change()
{
    cut_assert_true(Busy::set(Busy::Source::BUFFERING_STREAM));
    cppcut_assert_equal(1U, number_of_state_changes);
    cut_assert_true(current_busy_state);
    cut_assert_true(Busy::is_busy());

    cut_assert_false(Busy::set(Busy::DirectSource::WAITING_FOR_APPLIANCE_AUDIO));

    cppcut_assert_equal(1U, number_of_state_changes);
    cut_assert_true(Busy::is_busy());
}

/*!\test
 * Setting a direct followed by a counted busy source sets the busy state once.
 */
void test_set_direct_followed_by_counted_flag_causes_single_state_change()
{
    cut_assert_true(Busy::set(Busy::DirectSource::WAITING_FOR_APPLIANCE_AUDIO));
    cppcut_assert_equal(1U, number_of_state_changes);
    cut_assert_true(current_busy_state);
    cut_assert_true(Busy::is_busy());

    cut_assert_false(Busy::set(Busy::Source::BUFFERING_STREAM));

    cppcut_assert_equal(1U, number_of_state_changes);
    cut_assert_true(Busy::is_busy());
}

/*!\test
 * Direct and counted flags do not collide.
 *
 * We only check the boundaries, though.
 */
void test_counted_and_direct_flags_are_disjoint()
{
    /* first direct vs last counted */
    cut_assert_true(Busy::set(Busy::DirectSource::FIRST_SOURCE));
    cut_assert_false(Busy::set(Busy::Source::LAST_SOURCE));
    cut_assert_true(Busy::is_busy());

    cut_assert_false(Busy::clear(Busy::DirectSource::FIRST_SOURCE));
    cut_assert_true(Busy::is_busy());
    cut_assert_true(Busy::clear(Busy::Source::LAST_SOURCE));
    cut_assert_false(Busy::is_busy());

    /* first counted vs last direct */
    cut_assert_true(Busy::set(Busy::Source::FIRST_SOURCE));
    cut_assert_false(Busy::set(Busy::DirectSource::LAST_SOURCE));
    cut_assert_true(Busy::is_busy());

    cut_assert_false(Busy::clear(Busy::Source::FIRST_SOURCE));
    cut_assert_true(Busy::is_busy());
    cut_assert_true(Busy::clear(Busy::DirectSource::LAST_SOURCE));
    cut_assert_false(Busy::is_busy());
}

}
