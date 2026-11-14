/*
 * blockservice.c -- a simulated block service
 * COM S 3520 Project 2 (Fall 2026)
 *
 * The block service stands in for the disk. journal.c calls the seven
 * issue_* functions, and the block service later "completes" each write by
 * calling the matching completion function in journal.c.
 *
 * This file has two parts.
 *
 * PROVIDED, you do not need to change it: the event log, the counts
 * (bs_issued, bs_completed, bs_write_completes), bs_wait_quiet, the protocol
 * checker bs_check_protocol, and deliver(), which completes one write. In
 * the starter every write completes immediately, inside its issue_* call
 * (BS_IMMEDIATE), so the starter already runs and logs everything, and the
 * checker already works.
 *
 * YOUR PART, the functions marked TODO at the end of the file: writes that
 * complete later, in random order, on the block service's own threads
 * (BS_ASYNC); holds; and perturbation. On the GenAI path you write them in
 * this order (Section 2.5 and Appendix A of the GenAI handout):
 *
 *   Checkpoint  What                                    First needed by
 *   ----------  --------------------------------------  -----------------
 *   1           bs_mode(BS_ASYNC): async_issue, the     protocol_random
 *               completer threads, unheld_pending;
 *               bs_seed
 *   1           bs_hold, bs_release                     txe_delay
 *   4           bs_perturb, perturb_delay_us            stress
 *
 * Two semaphores coordinate the threads:
 *   bs_mutex  starts at 1 and guards all shared state: the log and the
 *             counts here, your pending writes and holds. A thread holds
 *             it from sem_wait(&bs_mutex) to sem_post(&bs_mutex).
 *   ready     counts writes that have become able to complete. Your
 *             code posts it once for each such write; each completer
 *             thread waits on it, then completes one write.
 *
 * Two rules for your part:
 *   - Touch shared state only while holding bs_mutex.
 *   - Complete a write only by calling deliver(). It logs the completion
 *     first, then releases bs_mutex while it calls journal.c, because the
 *     journal may issue more writes from inside a completion function, and
 *     issuing takes bs_mutex. Calling journal.c while holding bs_mutex
 *     would deadlock.
 */
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "journal.h"
#include "blockservice.h"

#define MAX_EVENTS   200000   /* about 16 events per request */
#define MAX_ID       100000   /* write_ids must be 1..MAX_ID */

/* ---- shared state -------------------------------------------------------- */

static sem_t bs_mutex;      /* 1 = free; guards everything below */
static sem_t ready;         /* writes able to complete; starts at 0 */
static int mode = BS_IMMEDIATE;
static unsigned rng = 1;    /* random state (bs_seed) */

/* Semaphores are set up with sem_init, which must run before any thread
 * uses them. The constructor attribute runs this once, before main. */
__attribute__((constructor)) static void init_semaphores(void) {
    sem_init(&bs_mutex, 0, 1);
    sem_init(&ready, 0, 0);
}

/* Your part, called by the provided part while holding bs_mutex. */
static void async_issue(int kind, int id);
static int  unheld_pending(void);
static int  perturb_delay_us(void);

/* A random number from the seeded generator (xorshift). Call it while
 * holding bs_mutex. */
static inline unsigned next_rand(void) {
    rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
    return rng;
}

/* =========================================================================
 * PROVIDED PART
 * ========================================================================= */

enum { EV_ISSUE, EV_DONE, EV_REQUEST, EV_WC };
typedef struct { int type, kind, id; } event_t;

static event_t events[MAX_EVENTS];
static int nevents, overflow;
static int in_callback;            /* completion functions running right now */
static int write_completes;
static int verbose = 1;

static const char *names[BS_NKINDS] = {
    "data", "txb", "journal bitmap", "journal inode",
    "txe", "bitmap", "inode" };
static const char *issue_names[BS_NKINDS] = {
    "write data", "journal txb", "journal bitmap", "journal inode",
    "journal txe", "write bitmap", "write inode" };
static void (*const callbacks[BS_NKINDS])(int) = {
    write_data_complete, journal_txb_complete, journal_bitmap_complete,
    journal_inode_complete, journal_txe_complete, write_bitmap_complete,
    write_inode_complete };

const char *bs_kind_name(int kind) {
    return (kind >= 0 && kind < BS_NKINDS) ? names[kind] : "?";
}

/* Append one event to the log. Call it while holding bs_mutex. */
static void log_event(int type, int kind, int id) {
    if (nevents < MAX_EVENTS) {
        events[nevents].type = type;
        events[nevents].kind = kind;
        events[nevents].id = id;
        nevents++;
    } else {
        overflow = 1;
    }
}

static void sleep_us(int us) {
    struct timespec ts = { us / 1000000, (long)(us % 1000000) * 1000L };
    while (nanosleep(&ts, &ts) != 0 && errno == EINTR) { }
}

/* Complete one write: log the completion, then call the matching completion
 * function in journal.c with bs_mutex released. Call it while holding
 * bs_mutex; it returns holding bs_mutex again. */
static void deliver(int kind, int id) {
    int delay = perturb_delay_us();
    in_callback++;
    log_event(EV_DONE, kind, id);
    sem_post(&bs_mutex);
    if (delay) sleep_us(delay);
    callbacks[kind](id);
    sem_wait(&bs_mutex);
    in_callback--;
}

/* Every issue is logged (and printed if verbose). In BS_IMMEDIATE mode the
 * write completes right here; in BS_ASYNC mode your async_issue takes it. */
static void issue(int kind, int id) {
    sem_wait(&bs_mutex);
    log_event(EV_ISSUE, kind, id);
    if (verbose) printf("issue %s %d\n", issue_names[kind], id);
    int delay = 0;
    if (mode == BS_IMMEDIATE) {
        deliver(kind, id);
    } else {
        async_issue(kind, id);
        delay = perturb_delay_us();
    }
    sem_post(&bs_mutex);
    if (delay) sleep_us(delay);
}

void issue_write_data(int write_id)     { issue(BS_DATA, write_id); }
void issue_journal_txb(int write_id)    { issue(BS_TXB, write_id); }
void issue_journal_bitmap(int write_id) { issue(BS_JBITMAP, write_id); }
void issue_journal_inode(int write_id)  { issue(BS_JINODE, write_id); }
void issue_journal_txe(int write_id)    { issue(BS_TXE, write_id); }
void issue_write_bitmap(int write_id)   { issue(BS_BITMAP, write_id); }
void issue_write_inode(int write_id)    { issue(BS_INODE, write_id); }

void bs_verbose(int on) {
    sem_wait(&bs_mutex);
    verbose = on;
    sem_post(&bs_mutex);
}

/* ---- notes from the test library ----------------------------------------- */

void bs_note_request(int write_id) {
    sem_wait(&bs_mutex);
    log_event(EV_REQUEST, -1, write_id);
    sem_post(&bs_mutex);
}

void bs_note_write_complete(int write_id) {
    sem_wait(&bs_mutex);
    log_event(EV_WC, -1, write_id);
    write_completes++;
    sem_post(&bs_mutex);
}

/* ---- counts -------------------------------------------------------------- */

static int count(int type, int kind, int id) {
    int n = 0;
    sem_wait(&bs_mutex);
    for (int i = 0; i < nevents; i++)
        if (events[i].type == type && events[i].kind == kind
            && (id == BS_ANY || events[i].id == id))
            n++;
    sem_post(&bs_mutex);
    return n;
}
int bs_issued(int kind, int write_id) {
    return count(EV_ISSUE, kind, write_id);
}
int bs_completed(int kind, int write_id) {
    return count(EV_DONE, kind, write_id);
}

int bs_write_completes(void) {
    sem_wait(&bs_mutex);
    int n = write_completes;
    sem_post(&bs_mutex);
    return n;
}

/* Returns once no write that is not held is pending, no completion function
 * is running, and nothing has been logged for about 20 ms. It does not wait
 * for a client thread that has not called request_write yet, and it does
 * not wait for held writes. */
void bs_wait_quiet(void) {
    int stable = 0, last = -1;
    while (stable < 4) {
        sleep_us(5000);
        sem_wait(&bs_mutex);
        int busy = in_callback > 0 || unheld_pending() > 0;
        int n = nevents;
        sem_post(&bs_mutex);
        stable = (!busy && n == last) ? stable + 1 : 0;
        last = n;
    }
}

/* ---- the protocol checker ------------------------------------------------ */

static int stage_of(int kind) {
    return kind <= BS_JINODE ? 0 : (kind == BS_TXE ? 1 : 2);
}
static const int stage_lo[3] = { BS_DATA, BS_TXE, BS_BITMAP };
static const int stage_hi[3] = { BS_JINODE, BS_TXE, BS_INODE };

/* VIOLATION(...) records a printf-style description of a broken rule and
 * jumps to the cleanup at the end of bs_check_protocol. */
static char msg[256];
#define VIOLATION(...) \
    do { \
        snprintf(msg, sizeof msg, __VA_ARGS__); \
        result = msg; \
        goto done; \
    } while (0)

/* Replays the whole log and returns NULL, or the first broken rule (see
 * blockservice.h). A request is in a stage from the issue of its first
 * write of that stage until all of its writes of that stage complete. */
const char *bs_check_protocol(int flags) {
    const char *result = NULL;
    sem_wait(&bs_mutex);
    unsigned char (*issued)[BS_NKINDS] = calloc(MAX_ID + 1, sizeof *issued);
    unsigned char (*completed)[BS_NKINDS] =
        calloc(MAX_ID + 1, sizeof *completed);
    unsigned char *wc = calloc(MAX_ID + 1, 1);
    int *req_order = malloc(sizeof(int) * (size_t)(nevents + 1));
    int nreq = 0, nwc = 0, occupant[3] = { 0, 0, 0 };
    if (!issued || !completed || !wc || !req_order)
        VIOLATION("blockservice: out of memory in bs_check_protocol");
    if (overflow)
        VIOLATION("blockservice: event log full (more than %d events)",
                  MAX_EVENTS);

    for (int e = 0; e < nevents; e++) {
        int type = events[e].type, k = events[e].kind, id = events[e].id;
        if (id < 1 || id > MAX_ID)
            VIOLATION("write_id %d out of range 1..%d", id, MAX_ID);
        if (type == EV_REQUEST) {
            req_order[nreq++] = id;
        } else if (type == EV_ISSUE) {
            int s = stage_of(k);
            if (issued[id][k])
                VIOLATION("%s issued twice for write %d", names[k], id);
            issued[id][k] = 1;
            if (k == BS_TXE)
                for (int j = BS_DATA; j <= BS_JINODE; j++)
                    if (!completed[id][j])
                        VIOLATION("txe issued for write %d before its %s "
                                  "write completed", id, names[j]);
            if (k == BS_BITMAP || k == BS_INODE)
                if (!completed[id][BS_TXE])
                    VIOLATION("%s checkpoint issued for write %d before "
                              "its txe completed", names[k], id);
            if (occupant[s] && occupant[s] != id)
                VIOLATION("%s issued for write %d while write %d is "
                          "still in the same stage",
                          names[k], id, occupant[s]);
            occupant[s] = id;
        } else if (type == EV_DONE) {
            if (!issued[id][k])
                VIOLATION("%s completed for write %d that was never "
                          "issued", names[k], id);
            completed[id][k] = 1;
            int s = stage_of(k), all = 1;
            for (int j = stage_lo[s]; j <= stage_hi[s]; j++)
                if (!completed[id][j]) all = 0;
            if (all && occupant[s] == id) occupant[s] = 0;
        } else { /* EV_WC */
            if (wc[id])
                VIOLATION("write_complete called twice for write %d", id);
            if (!completed[id][BS_BITMAP] || !completed[id][BS_INODE])
                VIOLATION("write_complete for write %d before its bitmap "
                          "and inode checkpoint writes completed", id);
            if ((flags & BS_CHECK_FIFO)) {
                if (nwc >= nreq || req_order[nwc] != id)
                    VIOLATION("write_complete order: write %d finished, "
                              "but write %d was requested before it and "
                              "has not finished",
                              id, nwc < nreq ? req_order[nwc] : -1);
            }
            wc[id] = 1;
            nwc++;
        }
    }
    if (flags & BS_CHECK_ALL_DONE) {
        for (int r = 0; r < nreq; r++) {
            int id = req_order[r];
            for (int k = 0; k < BS_NKINDS; k++)
                if (!completed[id][k])
                    VIOLATION("write %d: its %s write was %s", id, names[k],
                              issued[id][k] ? "issued but never completed"
                                            : "never issued");
            if (!wc[id])
                VIOLATION("write %d: write_complete was never called", id);
        }
    }
done:
    free(issued); free(completed); free(wc); free(req_order);
    sem_post(&bs_mutex);
    return result;
}

/* =========================================================================
 * YOUR PART
 * ========================================================================= */

/* TODO (Checkpoint 1): the pending writes, issued in BS_ASYNC mode and not
 * completed yet. A fixed-size array of (kind, id) pairs is enough: a correct
 * journal never has more than a few dozen writes pending. */

/* TODO (Checkpoint 1): called by issue() while holding bs_mutex, in
 * BS_ASYNC mode only. Add (kind, id) to the pending writes. If no hold
 * covers it, it can complete now: post ready once. Until you write this,
 * nothing completes in BS_ASYNC mode. */
static void async_issue(int kind, int id) { (void)kind; (void)id; }

/* TODO (Checkpoint 1): the completer thread. Two of them run this loop:
 *     sem_wait(&ready);           wait until some write can complete
 *     sem_wait(&bs_mutex);
 *     pick a random pending write that is not held (use next_rand),
 *     remove it from the pending writes, and call deliver(kind, id)
 *     sem_post(&bs_mutex);
 * A hold made after the post can take that write back, so now and then a
 * thread finds no write to pick. It just goes around the loop again: the
 * release that frees the write posts ready for it once more. */

/* TODO (Checkpoint 1): the number of pending writes that are not held.
 * bs_wait_quiet waits until it is 0. Called while holding bs_mutex. */
static int unheld_pending(void) { return 0; }

/* TODO (Checkpoint 1): bs_mode(BS_ASYNC) switches to asynchronous completion
 * and starts the two completer threads, once. Tests call it before
 * init_journal. bs_mode(BS_IMMEDIATE) is the starter behavior. */
void bs_mode(int m) { (void)m; }

/* TODO (Checkpoint 1): seed the random choice of which pending write
 * completes next (set rng; 0 is not a valid xorshift state, use 1). */
void bs_seed(unsigned seed) { (void)seed; }

/* TODO (Checkpoint 1, needed by txe_delay): holds. Keep a list of rules
 * (kind, write_id or BS_ANY, held or released); the newest rule that
 * matches a write wins. kind is always one write kind, never BS_ANY. A
 * held write stays pending until a release covers it. Holds apply in
 * BS_ASYNC mode only.
 * A release must post ready once for each pending write it frees, or no
 * completer thread wakes for them. One way: count unheld_pending() before
 * and after adding the rule, and post the difference. */
void bs_hold(int kind, int write_id)    { (void)kind; (void)write_id; }
void bs_release(int kind, int write_id) { (void)kind; (void)write_id; }

/* TODO (Checkpoint 4, for stress): perturbation. Remember the percentage and
 * the maximum delay; then perturb_delay_us returns 0, or (in percent% of
 * calls) a random delay from 1 to max_delay_us. The provided code sleeps
 * that long, with bs_mutex released, after an issue and before a
 * completion function runs. Called while holding bs_mutex. */
void bs_perturb(int percent, int max_delay_us) {
    (void)percent; (void)max_delay_us;
}
static int perturb_delay_us(void) { return 0; }
