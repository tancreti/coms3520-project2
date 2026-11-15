/*
 * tests.c -- your tests for journal.c. COM S 3520 Project 2 (Fall 2026).
 *
 * Build and run:  make tests        then  ./tests
 *                 ./tests NAME      run one test
 *                 ./tests all 7 100 run every test 100 times, seeds 7 to 106
 *
 *                 make simple       run every test on the simple version
 *
 * Each test is a function. Add it, with its name and a one-line description,
 * to the tests table above main. Above every test, write a comment that names
 * the rule it checks. Section 2.6 of the handout lists the tests your tests.c
 * must contain, and testlib.h and blockservice.h list the helpers.
 *
 * Rules for every test (Section 2.6):
 *   - Call bs_mode (if needed) and then init_journal once, at the start.
 *   - Use each write_id once, from 1 to 100000.
 *   - Use only the functions in journal.h, blockservice.h and testlib.h:
 *     the autograder builds your tests with its own blockservice.c and
 *     testlib.c.
 *   - When writes are held, make the requests with start_client, not
 *     request: a test thread stuck in request_write never reaches bs_release.
 *   - End every test by releasing any holds, waiting for every request with
 *     wait_for_writes, and calling check_protocol(BS_CHECK_ALL_DONE), adding
 *     BS_CHECK_FIFO when one client makes all the requests. A test that
 *     returns with writes still pending checks less than it seems, and can
 *     pass on one machine and fail on another.
 */
#include <pthread.h>
#include <stdio.h>
#include "testlib.h"

/* Project path. GenAI path: leave this as 0.
 * No-GenAI path: change it to 1 (handout Appendix A, Getting started).
 * The autograder reads this line to choose your rubric, and ./tests prints
 * the path it says on its first line. */
#define NO_GENAI_PATH 0

/* Shorthand for BUFFER_SIZE (journal.h), handy when a test checks a count
 * that depends on the buffer size, as fill_count does. */
#define B BUFFER_SIZE

/* Example. Rule: when every write completes immediately, every request
 * finishes (write_complete is called once for each write) and commits
 * exactly once (one TxE per request). */
static void t_example_immediate(void) {
    bs_mode(BS_IMMEDIATE);          /* before init_journal */
    init_journal();
    for (int id = 1; id <= 10; id++)
        request(id);                /* notes the request, then calls request_write */
    wait_for_writes(10);
    int txe = bs_issued(BS_TXE, BS_ANY);
    TEST_ASSERT(txe == 10, "%d txe writes issued for 10 requests, expected 10", txe);
    check_protocol(BS_CHECK_ALL_DONE | BS_CHECK_FIFO);
}

static const test_t tests[] = {
    { "example_immediate", t_example_immediate, "immediate completion: 10 requests finish" },
};

int main(int argc, char **argv) {
    printf("Path: %s (NO_GENAI_PATH is %d in tests.c)\n",
           NO_GENAI_PATH ? "no-GenAI" : "GenAI", NO_GENAI_PATH);
    return run_tests(tests, (int)(sizeof tests / sizeof tests[0]), argc, argv);
}
