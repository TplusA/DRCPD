/*
 * Copyright (C) 2016  T+A elektroakustik GmbH & Co. KG
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

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <cppcutter.h>

#include "busy.hh"

namespace busy_state_tests
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
 * Setting a single busy source sets the busy state.
 */
void test_set_multiple_flag_causes_single_state_change()
{
    cut_assert_true(Busy::set(Busy::Source::BUFFERING_STREAM));
    cppcut_assert_equal(1U, number_of_state_changes);
    cut_assert_true(current_busy_state);
    cut_assert_true(Busy::is_busy());

    cut_assert_false(Busy::set(Busy::Source::FILLING_PLAYER_QUEUE));
    cut_assert_false(Busy::set(Busy::Source::ENTERING_DIRECTORY));

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

    cut_assert_false(Busy::set(Busy::Source::ENTERING_DIRECTORY));
    cppcut_assert_equal(1U, number_of_state_changes);

    cut_assert_false(Busy::clear(Busy::Source::BUFFERING_STREAM));
    cppcut_assert_equal(1U, number_of_state_changes);

    cut_assert_false(Busy::set(Busy::Source::WAITING_FOR_PLAYER));
    cppcut_assert_equal(1U, number_of_state_changes);

    cut_assert_false(Busy::clear(Busy::Source::WAITING_FOR_PLAYER));
    cppcut_assert_equal(1U, number_of_state_changes);

    cut_assert_true(Busy::clear(Busy::Source::ENTERING_DIRECTORY));
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
    cut_assert_true(Busy::set(Busy::Source::ENTERING_DIRECTORY));
    cppcut_assert_equal(1U, number_of_state_changes);
    cut_assert_true(current_busy_state);
    cut_assert_true(Busy::is_busy());

    cut_assert_false(Busy::set(Busy::Source::ENTERING_DIRECTORY));
    cut_assert_false(Busy::set(Busy::Source::ENTERING_DIRECTORY));
    cut_assert_false(Busy::set(Busy::Source::ENTERING_DIRECTORY));

    cut_assert_false(Busy::clear(Busy::Source::ENTERING_DIRECTORY));
    cut_assert_false(Busy::clear(Busy::Source::ENTERING_DIRECTORY));
    cut_assert_false(Busy::clear(Busy::Source::ENTERING_DIRECTORY));

    cppcut_assert_equal(1U, number_of_state_changes);

    cut_assert_true(Busy::clear(Busy::Source::ENTERING_DIRECTORY));
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
    cut_assert_true(Busy::set(Busy::Source::ENTERING_DIRECTORY));
    cppcut_assert_equal(1U, number_of_state_changes);
    cut_assert_true(current_busy_state);
    cut_assert_true(Busy::is_busy());

    cut_assert_false(Busy::set(Busy::Source::ENTERING_DIRECTORY));

    cut_assert_false(Busy::set(Busy::Source::BUFFERING_STREAM));
    cut_assert_false(Busy::set(Busy::Source::BUFFERING_STREAM));

    cut_assert_false(Busy::clear(Busy::Source::ENTERING_DIRECTORY));
    cut_assert_false(Busy::clear(Busy::Source::BUFFERING_STREAM));
    cut_assert_false(Busy::clear(Busy::Source::ENTERING_DIRECTORY));

    cppcut_assert_equal(1U, number_of_state_changes);

    cut_assert_true(Busy::clear(Busy::Source::BUFFERING_STREAM));
    cppcut_assert_equal(2U, number_of_state_changes);
    cut_assert_false(current_busy_state);
    cut_assert_false(Busy::is_busy());
}

}
