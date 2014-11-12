#include <cppcutter.h>

#include "dcp_transaction.hh"

namespace dcp_transaction_tests
{

static void dcp_transaction_observer(DcpTransaction::state)
{
    /* nothing */
}

static const std::function<void(DcpTransaction::state)> transaction_observer(dcp_transaction_observer);

static DcpTransaction *dt;
static std::ostringstream *captured;

void cut_setup(void)
{
    dt = new DcpTransaction(transaction_observer);
    cppcut_assert_not_null(dt);

    captured = new std::ostringstream;
    cppcut_assert_not_null(captured);

    dt->set_output_stream(captured);
}

void cut_teardown(void)
{
    cut_assert_true(captured->str().empty());

    delete dt;
    delete captured;

    dt = nullptr;
    captured = nullptr;
}

static void clear_ostream(std::ostringstream *ss)
{
    ss->str("");
    ss->clear();
}

static void check_and_clear_ostream(std::ostringstream *ss, const char *expected)
{
    cppcut_assert_equal(expected, ss->str().c_str());
    clear_ostream(ss);
}

/*!\test
 * One simple transaction, nothing special.
 */
void test_one_transaction()
{
    cut_assert_false(dt->is_in_progress());
    cut_assert_true(dt->start());
    cut_assert_true(dt->is_in_progress());
    cppcut_assert_not_null(dt->stream());
    *dt->stream() << "Simple!";
    cut_assert_true(dt->commit());
    check_and_clear_ostream(captured, "Size: 7\nSimple!");
    cut_assert_true(dt->is_in_progress());
    cut_assert_true(dt->done());
    cut_assert_false(dt->is_in_progress());
}

/*!\test
 * Nothing is sent for empty transactions.
 */
void test_empty_transaction()
{
    cut_assert_false(dt->is_in_progress());
    cut_assert_true(dt->start());
    cut_assert_true(dt->is_in_progress());
    cut_assert_true(dt->commit());
    cut_assert_true(dt->is_in_progress());
    cut_assert_true(dt->done());
    cut_assert_false(dt->is_in_progress());
}

/*!\test
 * Two simple transaction, nothing special.
 *
 * Makes sure that the done() of the first transaction actually reverts the
 * internal state of the reused transaction object.
 */
void test_two_transactions()
{
    cut_assert_true(dt->start());
    cppcut_assert_not_null(dt->stream());
    *dt->stream() << "First";
    cut_assert_true(dt->commit());
    cut_assert_true(dt->done());

    cut_assert_true(dt->start());
    cppcut_assert_not_null(dt->stream());
    *dt->stream() << "Second";
    cut_assert_true(dt->commit());
    cut_assert_true(dt->done());

    check_and_clear_ostream(captured, "Size: 5\nFirstSize: 6\nSecond");
}

/*!\test
 * Two transactions, first aborted.
 *
 * Makes sure that the abort() of the first transaction actually reverts the
 * internal state of the reused transaction object.
 */
void test_transaction_after_aborted_transaction()
{
    cut_assert_true(dt->start());
    cppcut_assert_not_null(dt->stream());
    *dt->stream() << "Aborted";
    cut_assert_true(dt->abort());

    cut_assert_true(dt->start());
    cppcut_assert_not_null(dt->stream());
    *dt->stream() << "Sent";
    cut_assert_true(dt->commit());
    check_and_clear_ostream(captured, "Size: 4\nSent");
    cut_assert_true(dt->done());
}

/*!\test
 * Aborting a transaction sends nothing.
 */
void test_abort_transaction_writes_nothing()
{
    cut_assert_true(dt->start());
    cppcut_assert_not_null(dt->stream());
    *dt->stream() << "Should be aborted";
    cut_assert_true(dt->abort());
}

/*!\test
 * Aborting a committed transaction is the same as ending it nicely.
 */
void test_abort_committed_transaction_does_not_unsend()
{
    cut_assert_true(dt->start());
    cppcut_assert_not_null(dt->stream());
    *dt->stream() << "Already sent";
    cut_assert_true(dt->commit());
    check_and_clear_ostream(captured, "Size: 12\nAlready sent");
    cut_assert_true(dt->abort());
}

/*!\test
 * Starting a transaction twice is blocked.
 */
void test_starting_twice_fails()
{
    cut_assert_true(dt->start());
    cut_assert_false(dt->start());
    cut_assert_true(dt->is_in_progress());
}

/*!\test
 * Starting a transaction after commit is blocked.
 */
void test_starting_after_commit_fails()
{
    cut_assert_true(dt->start());
    cut_assert_true(dt->commit());
    cut_assert_false(dt->start());
    cut_assert_true(dt->is_in_progress());
}

/*!\test
 * Attempting to get internal string stream without prior start gives nullptr.
 */
void test_get_stream_without_start_yields_nullptr()
{
    cppcut_assert_null(dt->stream());
}

/*!\test
 * Attempting to get internal string stream after commit gives nullptr.
 */
void test_get_stream_after_commit_yields_nullptr()
{
    cut_assert_true(dt->start());
    cut_assert_true(dt->commit());
    cppcut_assert_null(dt->stream());
    cut_assert_true(dt->is_in_progress());
}

/*!\test
 * Attempting to commit without prior start fails.
 */
void test_commit_without_start_fails()
{
    cut_assert_false(dt->commit());
}

/*!\test
 * Attempting to end without prior start fails.
 */
void test_done_without_start_fails()
{
    cut_assert_false(dt->done());
}

/*!\test
 * Attempting to end without prior commit fails.
 */
void test_done_without_commit_fails()
{
    cut_assert_true(dt->start());
    cut_assert_false(dt->done());
    cut_assert_true(dt->is_in_progress());
}

/*!\test
 * Attempting to abort without prior start fails.
 */
void test_abort_without_start_fails()
{
    cut_assert_false(dt->abort());
}

/*!\test
 * Transactions can be done without an output stream.
 */
void test_set_null_output_stream()
{
    dt->set_output_stream(nullptr);

    cut_assert_true(dt->start());
    cppcut_assert_not_null(dt->stream());
    *dt->stream() << "Nothing should be written";
    cut_assert_true(dt->commit());
    cut_assert_true(dt->done());
}

};


namespace dcp_transaction_tests_observer
{

static DcpTransaction::state expected_state;
static unsigned int expected_number_of_transitions;
static unsigned int number_of_transitions;

static void dcp_transaction_observer(DcpTransaction::state state)
{
    cppcut_assert_equal(expected_state, state);
    ++number_of_transitions;
    cppcut_assert_operator(expected_number_of_transitions, >=, number_of_transitions);
}

static const std::function<void(DcpTransaction::state)> transaction_observer(dcp_transaction_observer);

static DcpTransaction *dt;

void cut_setup(void)
{
    dt = new DcpTransaction(transaction_observer);
    cppcut_assert_not_null(dt);

    expected_state = DcpTransaction::WAIT_FOR_ANSWER;
    expected_number_of_transitions = 0;
    number_of_transitions = 0;
}

void cut_teardown(void)
{
    cppcut_assert_equal(expected_number_of_transitions, number_of_transitions);

    delete dt;
    dt = nullptr;
}

/*!\test
 * Starting a transaction causes a single state change.
 */
void test_start(void)
{
    expected_number_of_transitions = 1;
    expected_state = DcpTransaction::WAIT_FOR_COMMIT;
    dt->start();
}

/*!\test
 * Erroneously committing an idle transaction has no effect and is not seen by
 * the observer.
 *
 * In other words, the observer only gets to see successful state changes.
 */
void test_commit_without_start_does_not_invoke_observer(void)
{
    dt->commit();
}

/*!\test
 * Start, commit, done cause three state changes.
 */
void test_full_transaction(void)
{
    expected_number_of_transitions = 3;
    expected_state = DcpTransaction::WAIT_FOR_COMMIT;
    dt->start();
    expected_state = DcpTransaction::WAIT_FOR_ANSWER;
    dt->commit();
    expected_state = DcpTransaction::IDLE;
    dt->done();
}


/*!\test
 * Start, abort cause two state changes.
 */
void test_abort_after_start(void)
{
    expected_number_of_transitions = 2;
    expected_state = DcpTransaction::WAIT_FOR_COMMIT;
    dt->start();
    expected_state = DcpTransaction::IDLE;
    dt->abort();
}


/*!\test
 * Start, commit, abort cause three state changes.
 */
void test_abort_after_commit(void)
{
    expected_number_of_transitions = 3;
    expected_state = DcpTransaction::WAIT_FOR_COMMIT;
    dt->start();
    expected_state = DcpTransaction::WAIT_FOR_ANSWER;
    dt->commit();
    expected_state = DcpTransaction::IDLE;
    dt->abort();
}

};
