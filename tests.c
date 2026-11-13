/*
 * tests.c -- your tests for journal.c. COM S 3520 Project 2 (Fall 2026).
 *
 * Build and run:  make tests        then  ./tests
 *                 ./tests NAME      run one test
 *                 ./tests all 7 100 run every test 100 times, seeds 7 to 106
 *
 * Each test is a function. Add it, with its name and a one-line description,
 * to the tests table above main. Above every test, write a comment that names
 * the rule it checks. Section 2.6 of the handout lists the tests your tests.c
 * must contain, and testlib.h lists the helpers.
 */
#include <pthread.h>
#include "testlib.h"

/* Shorthand for BUFFER_SIZE (journal.h), handy when a test checks a count
 * that depends on the buffer size, as fill_count does. */
#define B BUFFER_SIZE

/* Example. Rule: when every write completes immediately, every request
 * finishes (write_complete is called once for each write). */
static void t_example_immediate(void) {
    bs_mode(BS_IMMEDIATE);          /* before init_journal */
    init_journal();
    for (int id = 1; id <= 10; id++)
        request(id);                /* notes the request, then calls request_write */
    wait_for_writes(10);
    check_protocol(BS_CHECK_ALL_DONE | BS_CHECK_FIFO);
}

static const test_t tests[] = {
    { "example_immediate", t_example_immediate, "immediate completion: 10 requests finish" },
};

int main(int argc, char **argv) {
    return run_tests(tests, (int)(sizeof tests / sizeof tests[0]), argc, argv);
}
