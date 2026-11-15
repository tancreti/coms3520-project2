/*
 * blockservice.h -- controllable block service for testing journal.c
 * COM S 3520 Project 2 (Fall 2026)
 *
 * The block service stands in for the disk. In the starter blockservice.c,
 * every write completes immediately (BS_IMMEDIATE). The rest of the controls
 * are declared below: writes that complete later, in random order, on the
 * block service's own threads (BS_ASYNC); holds, which keep chosen writes
 * from completing; and perturbation. On the GenAI path you implement them in
 * blockservice.c (see the comment at its top). On the no-GenAI path, the
 * blockservice.c from project2-noai.zip already implements all of them. The
 * event log, the counts, bs_wait_quiet and the protocol checker are provided
 * on both paths.
 *
 * An example of how a test might use it:
 *     bs_mode(BS_ASYNC);            // before init_journal
 *     init_journal();
 *     bs_hold(BS_TXE, 1);           // TxE of write 1 will not complete
 *     start_client(1, 2);           // (testlib) requests 1 and 2 on a client
 *                                   // thread, so this thread never blocks
 *     bs_wait_quiet();              // let everything that can finish, finish
 *     ... check bs_issued(...) ...
 *     bs_release(BS_TXE, 1);
 *     wait_for_writes(2);           // (testlib)
 *     check_protocol(BS_CHECK_ALL_DONE | BS_CHECK_FIFO);   // (testlib)
 *
 * Request through a client thread (start_client) whenever writes are held:
 * request_write blocks while buffer 1 is full, and a test thread blocked
 * there can never reach its bs_release.
 *
 * Limits: write_ids must be 1 to 100000, and each write_id may be requested
 * only once per test. The log holds 200000 events (each request logs 16),
 * about 12000 requests per test.
 *
 * Sections below: write kinds, setup, holds, counts, waiting, checking.
 */
#ifndef BLOCKSERVICE_H
#define BLOCKSERVICE_H

/* ---- write kinds ---------------------------------------------------------
 * In journaling-step order. Every function below that takes a kind takes
 * exactly one of these; only write_id may be BS_ANY. */
enum bs_kind {
    BS_DATA,      /* step 1: data block to its final location */
    BS_TXB,       /* step 2: journal begin block              */
    BS_JBITMAP,   /* step 2: journal copy of the bitmap       */
    BS_JINODE,    /* step 2: journal copy of the inode        */
    BS_TXE,       /* step 3: journal commit block             */
    BS_BITMAP,    /* step 4: bitmap to its final location     */
    BS_INODE,     /* step 4: inode to its final location      */
    BS_NKINDS
};

/* "Every write_id" in holds, releases and counts. Never a kind. */
#define BS_ANY (-1)

/* The name of a kind, for messages: bs_kind_name(BS_TXE) is "txe". */
const char *bs_kind_name(int kind);

/* ---- setup: call these before init_journal --------------------------------
 *
 * Completion mode:
 *   BS_IMMEDIATE  (the default) each write completes inside its issue_*
 *                 call, on the caller's thread.
 *   BS_ASYNC      completer threads finish writes later, in random order. */
#define BS_IMMEDIATE 0
#define BS_ASYNC     1
void bs_mode(int mode);

/* Seed for the random completion order and delays (default 1). A seed fixes
 * these choices, not how threads are scheduled, so a run may not repeat
 * exactly. The test runner calls it for you with the run's seed. */
void bs_seed(unsigned seed);

/* Perturbation: random short sleeps after an issue and before a completion
 * function runs, in percent% of them, each 1 to max_delay_us microseconds.
 * percent 0 turns it off (the default). */
void bs_perturb(int percent, int max_delay_us);

/* Print each issue. On by default, which ./filesystem shows; the test
 * runner turns it off. */
void bs_verbose(int on);

/* ---- holds ---------------------------------------------------------------
 * A held write is issued normally but stays pending, not completed, until a
 * release covers it. The newest matching rule wins, so
 *     bs_hold(BS_TXE, BS_ANY);
 *     bs_release(BS_TXE, 2);
 * lets only TxE(2) complete. Holds apply in BS_ASYNC mode only. kind must be
 * one write kind (BS_DATA ... BS_INODE); only write_id may be BS_ANY. The
 * autograder's block service stops the test with an error if kind is not a
 * write kind. */
void bs_hold(int kind, int write_id);
void bs_release(int kind, int write_id);

/* ---- counts so far (write_id may be BS_ANY) ------------------------------ */
int bs_issued(int kind, int write_id);
int bs_completed(int kind, int write_id);
int bs_write_completes(void);           /* write_complete calls seen */

/* ---- waiting -------------------------------------------------------------
 * Wait until every write that is not held has completed, no completion
 * function is running, and nothing has been logged for about 20 ms (so
 * journal threads have had time to react). It does not wait for held
 * writes, or for a client thread that has not called request_write yet,
 * and it cannot tell a journal thread that is slow from one that is stuck:
 * "quiet" means only that nothing happened for 20 ms. */
void bs_wait_quiet(void);

/* ---- checking ------------------------------------------------------------
 * Check the whole log so far. Returns NULL if no rule is broken, otherwise a
 * description of the first broken rule. Tests usually call check_protocol
 * (testlib.h) instead, which fails the test with that description.
 *
 * Rules always checked, per write_id:
 *   - TxE is issued only after data, TxB, journal bitmap and journal inode
 *     have all completed (the commit barrier);
 *   - bitmap and inode checkpoints are issued only after TxE completed;
 *   - write_complete only after both checkpoint writes completed;
 *   - no write is issued twice; write_complete at most once;
 *   - at most one request is in each stage at a time.
 * Flags add:
 *   BS_CHECK_ALL_DONE  every noted request finished all 7 writes and
 *                      write_complete;
 *   BS_CHECK_FIFO      write_complete order equals bs_note_request order
 *                      (only meaningful with one client thread). */
#define BS_CHECK_ALL_DONE 1
#define BS_CHECK_FIFO     2
const char *bs_check_protocol(int flags);

/* ---- used by the test library only, not by your tests or journal.c ------ */
void bs_note_request(int write_id);         /* just before request_write */
void bs_note_write_complete(int write_id);  /* from write_complete */

#endif
