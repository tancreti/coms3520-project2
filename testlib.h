/*
 * testlib.h -- test runner and helpers for journal.c tests
 * COM S 3520 Project 2 (Fall 2026)
 *
 * Each test runs in its own child process (fork), so init_journal starts
 * fresh every time and a hang is caught by an alarm. Inside a test, the
 * journal's printf output goes to a capture file that output_contains()
 * searches; test results go to the real terminal.
 *
 * Usage:
 *   ./tests                 run every test once (seed 1)
 *   ./tests NAME            run one test
 *   ./tests all SEED        choose the seed for random completion order
 *   ./tests all SEED RUNS   run each test RUNS times,
 *                           seeds SEED to SEED+RUNS-1
 * Set STOP_ON_FAIL=1 to stop after the first failing test.
 * Exit status 0 if every run passed, 1 otherwise.
 *
 * Debugging: gdb ./tests, then run NAME. In the dev container gdb follows
 * the test's child process, and the 5 s hang alarm is off while gdb is
 * attached.
 */
#ifndef TESTLIB_H
#define TESTLIB_H

#include "journal.h"
#include "blockservice.h"

/* ---- running tests ------------------------------------------------------- */

#define TEST_SECONDS 5      /* a test running longer than this has hung */

typedef struct {
    const char *name;
    void (*fn)(void);
    const char *checks;     /* one line: the rule this test checks */
} test_t;

/* Run the tests named on the command line (see above). Returns the exit
 * status. */
int run_tests(const test_t *tests, int ntests, int argc, char **argv);

/* End the running test as failed, with a printf-style message. */
void test_fail(const char *fmt, ...)
    __attribute__((format(printf, 1, 2), noreturn));

/* Fail with a printf-style message unless cond is true:
 *     TEST_ASSERT(n == 4, "%d writes issued, expected 4", n); */
#define TEST_ASSERT(cond, ...) \
    do { if (!(cond)) test_fail(__VA_ARGS__); } while (0)

/* The seed of this run (also passed to bs_seed before the test starts). */
unsigned test_seed(void);

/* ---- making requests ----------------------------------------------------- */

/* Request write_id: notes the request for bs_check_protocol, then calls
 * request_write (which may block if buffer 1 is full). */
void request(int write_id);

/* Start a background client thread that requests first, first+1, ...,
 * first+n-1 in that order, and return right away. clients_returned() counts
 * how many of the newest client's request_write calls have returned so far.
 *
 * You may call start_client again while an earlier client is still running
 * (for example, several clients at once in a stress test). Give each client
 * its own range of write_ids. With more than one client running, the order
 * of requests between clients is not defined, so BS_CHECK_FIFO is not
 * meaningful. */
void start_client(int first, int n);
int  clients_returned(void);

/* ---- waiting and checking ------------------------------------------------ */

/* Wait until write_complete has been called n times in total. After
 * TEST_SECONDS - 1 seconds it gives up and fails the test, naming a broken
 * rule if there is one, otherwise the first request that did not finish. */
void wait_for_writes(int n);

/* Fail the test if bs_check_protocol(flags) reports a broken rule. */
void check_protocol(int flags);

/* 1 if the journal's output so far contains text. */
int output_contains(const char *text);

/* ---- small helpers ------------------------------------------------------- */

/* CPU time used by the whole process so far, in milliseconds. */
double cpu_ms(void);

/* Sleep this thread for ms milliseconds. */
void sleep_ms(int ms);

#endif
