/*
 * blockservice.h -- controllable block service for testing journal.c
 * COM S 3520 Project 2 (Fall 2026)
 *
 * The default block service implementation you are given completes every
 * write immediately. A complete implementation can also complete writes
 * later, in random order, on its own threads, and lets a test hold back
 * chosen writes, count what was issued, and check the journaling protocol
 * from a log of every issue and completion.
 *
 * An example of how blockservice might be used in a test:
 *     bs_mode(BS_ASYNC);            // before init_journal
 *     init_journal();
 *     bs_hold(BS_TXE, 1);           // TxE of write 1 will not complete
 *     request(1); request(2);       // (testlib helper: notes, then request_write)
 *     bs_wait_quiet();              // let everything that can finish, finish
 *     ... check bs_issued(...) ...
 *     bs_release(BS_TXE, 1);
 *     wait_for_writes(2);           // (testlib helper)
 *     msg = bs_check_protocol(BS_CHECK_ALL_DONE | BS_CHECK_FIFO);
 */
#ifndef BLOCKSERVICE_H
#define BLOCKSERVICE_H

/* Write kinds, in journaling-step order. */
enum bs_kind {
    BS_DATA,      /* step 1: data block to its final location   */
    BS_TXB,       /* step 2: journal begin block                */
    BS_JBITMAP,   /* step 2: journal copy of the bitmap         */
    BS_JINODE,    /* step 2: journal copy of the inode          */
    BS_TXE,       /* step 3: journal commit block               */
    BS_BITMAP,    /* step 4: bitmap to its final location       */
    BS_INODE,     /* step 4: inode to its final location        */
    BS_NKINDS
};
#define BS_ANY (-1)          /* "every write_id" in hold/release/counts */

/* Completion modes. Call bs_mode before init_journal. */
#define BS_IMMEDIATE 0       /* default behavior: complete inside issue_*, on the caller's thread */
#define BS_ASYNC     1       /* completer threads finish writes later, in random order */
void bs_mode(int mode);
void bs_seed(unsigned seed);                    /* random order and delays (default 1) */
void bs_perturb(int percent, int max_delay_us); /* random short sleeps at issue and
                                                   before each completion; 0 = off */
void bs_verbose(int on);                        /* print each issue (default on) */

/* Holds. The newest matching rule wins, so bs_hold(BS_TXE, BS_ANY) followed by
 * bs_release(BS_TXE, 2) lets only TxE(2) complete. Held writes stay pending
 * until released. Holds apply in BS_ASYNC mode only. */
void bs_hold(int kind, int write_id);
void bs_release(int kind, int write_id);

/* Counts so far (write_id may be BS_ANY). */
int  bs_issued(int kind, int write_id);
int  bs_completed(int kind, int write_id);
int  bs_write_completes(void);                  /* write_complete calls seen */

/* Wait until every write that is not held has completed, no completion
 * function is running, and nothing has been logged for about 20 ms (so
 * journal threads have had time to react). */
void bs_wait_quiet(void);

/* Called by the test library around the journal (not by journal.c). */
void bs_note_request(int write_id);             /* just before request_write */
void bs_note_write_complete(int write_id);      /* from write_complete */

/* Check the whole log so far. Returns NULL if no rule is broken, otherwise a
 * description of the first broken rule. Rules always checked, per write_id:
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

const char *bs_kind_name(int kind);

#endif
