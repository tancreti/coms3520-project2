/*
 * blockservice.c -- a simulated block service
 * COM S 3520 Project 2 (Fall 2026)
 *
 * This is the starting point for the simulated block service. In this
 * version every issued write "completes" immediately: each issue function
 * calls the matching completion function in journal.c before it returns,
 * on the caller's thread. A real disk completes writes later and in any
 * order, so to write tests that can catch concurrency bugs you will extend
 * this file so that writes can also complete later, on the block service's
 * own threads.
 *
 * The test controls declared in blockservice.h are below the issue
 * functions. In the starter code they are only placeholders, just enough
 * for the example test in tests.c and the test library to build and run.
 * On the GenAI path you implement them (Section 2.5 and Appendix B of the
 * handout) in this order:
 *
 *   Checkpoint  Controls                           First needed by
 *   ----------  ---------------------------------  ------------------------
 *   1           bs_mode(BS_ASYNC), bs_seed,        protocol_random,
 *               the event log (bs_note_request,    txe_delay
 *               bs_note_write_complete),
 *               bs_check_protocol,
 *               bs_hold, bs_release
 *   1 or 2      bs_wait_quiet                      txe_delay, hold_each_kind
 *   2           bs_issued, bs_completed            hold_each_kind
 *   4           bs_perturb                         stress
 */
#include <pthread.h>
#include <stdio.h>
#include "journal.h"
#include "blockservice.h"

static int verbose = 1;     /* print message on each issue */

void issue_write_data(int write_id) {
    if (verbose) printf("issue write data %d\n", write_id);
    write_data_complete(write_id);
}
void issue_journal_txb(int write_id) {
    if (verbose) printf("issue journal txb %d\n", write_id);
    journal_txb_complete(write_id);
}
void issue_journal_bitmap(int write_id) {
    if (verbose) printf("issue journal bitmap %d\n", write_id);
    journal_bitmap_complete(write_id);
}
void issue_journal_inode(int write_id) {
    if (verbose) printf("issue journal inode %d\n", write_id);
    journal_inode_complete(write_id);
}
void issue_journal_txe(int write_id) {
    if (verbose) printf("issue journal txe %d\n", write_id);
    journal_txe_complete(write_id);
}
void issue_write_bitmap(int write_id) {
    if (verbose) printf("issue write bitmap %d\n", write_id);
    write_bitmap_complete(write_id);
}
void issue_write_inode(int write_id) {
    if (verbose) printf("issue write inode %d\n", write_id);
    write_inode_complete(write_id);
}

/* ---- test controls (blockservice.h) -------------------------------------
 * Placeholders. Each TODO says what the full version does; Appendix B has
 * the contract. */

static pthread_mutex_t bs_lock = PTHREAD_MUTEX_INITIALIZER;
static int write_completes;

/* TODO (Checkpoint 1): BS_ASYNC, where writes complete later, in random
 * order, on the block service's own threads. Only BS_IMMEDIATE works now. */
void bs_mode(int mode) { (void)mode; }

/* TODO (Checkpoint 1): seed the random completion order. */
void bs_seed(unsigned seed) { (void)seed; }

/* TODO (Checkpoint 4, for the stress test): random short sleeps at issue and
 * before each completion, so rare interleavings become common. */
void bs_perturb(int percent, int max_delay_us) { (void)percent; (void)max_delay_us; }

void bs_verbose(int on) { verbose = on; }

/* TODO (Checkpoint 1, needed by txe_delay): hold back writes until
 * released. */
void bs_hold(int kind, int write_id) { (void)kind; (void)write_id; }
void bs_release(int kind, int write_id) { (void)kind; (void)write_id; }

/* TODO (log: Checkpoint 1; counts: Checkpoint 2): keep a log of every issue
 * and completion, and count from it. These always return 0 for now. */
int bs_issued(int kind, int write_id) { (void)kind; (void)write_id; return 0; }
int bs_completed(int kind, int write_id) { (void)kind; (void)write_id; return 0; }

int bs_write_completes(void) {
    pthread_mutex_lock(&bs_lock);
    int n = write_completes;
    pthread_mutex_unlock(&bs_lock);
    return n;
}

/* TODO (Checkpoint 1 or 2): wait until every write that is not held has
 * completed, no completion function is running, and nothing has been logged
 * for about 20 ms. With immediate completion there is nothing to wait for. */
void bs_wait_quiet(void) { }

/* Called by the test library. TODO (Checkpoint 1): log the request order
 * (for BS_CHECK_FIFO) and each write_complete. */
void bs_note_request(int write_id) { (void)write_id; }

void bs_note_write_complete(int write_id) {
    (void)write_id;
    pthread_mutex_lock(&bs_lock);
    write_completes++;
    pthread_mutex_unlock(&bs_lock);
}

/* TODO (Checkpoint 1): check the log against the journaling protocol and
 * return a description of the first broken rule. This placeholder checks
 * nothing and always reports no problem. */
const char *bs_check_protocol(int flags) { (void)flags; return NULL; }

const char *bs_kind_name(int kind) {
    static const char *names[BS_NKINDS] = {
        "data", "txb", "journal bitmap", "journal inode", "txe", "bitmap", "inode"
    };
    return (kind >= 0 && kind < BS_NKINDS) ? names[kind] : "?";
}
