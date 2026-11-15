/*
 * simple_journal.c -- the simple version, kept for testing. DO NOT EDIT.
 * COM S 3520 Project 2 (Fall 2026)
 *
 * This is a copy of the starter journal.c. `make simple` builds your tests
 * with this file in place of journal.c, so you can check, at any point,
 * that each required test still fails on the simple version (Section 3.3)
 * while your own journal.c changes. The autograder does not use this file.
 *
 * This version issues every write in the correct order, but it assumes
 * that each issued write completes immediately (before the issue call
 * returns). It does not wait for anything, so it is wrong as soon as writes
 * complete later or out of order.
 */
#include <stdio.h>
#include "journal.h"

int is_write_data_complete;
int is_journal_txb_complete;
int is_journal_bitmap_complete;
int is_journal_inode_complete;
int is_journal_txe_complete;
int is_write_bitmap_complete;
int is_write_inode_complete;

/* This function can be used to initialize the semaphores, buffers and
 * threads. */
void init_journal(void) {
    // initialize semaphores, buffers and threads here
}

/* Called by the file system to request writing data to persistent storage.
 * This simple version does not correctly deal with concurrency. It issues
 * all writes in the correct order, but it assumes issued writes always
 * complete immediately and therefore it doesn't wait for each phase to
 * complete. */
void request_write(int write_id) {
    // write data and journal metadata
    is_write_data_complete = 0;
    is_journal_txb_complete = 0;
    is_journal_bitmap_complete = 0;
    is_journal_inode_complete = 0;
    issue_write_data(write_id);
    issue_journal_txb(write_id);
    issue_journal_bitmap(write_id);
    issue_journal_inode(write_id);
    // normally we should wait here for the 4 issues to complete, but this
    // simple implementation doesn't have concurrency, so instead we will
    // just call it an error if anything isn't complete in this version
    if (!is_write_data_complete || !is_journal_txb_complete
        || !is_journal_bitmap_complete || !is_journal_inode_complete) {
        printf("ERROR: writing data or journal failed!\n");
        return;
    }

    // commit transaction by writing txe
    is_journal_txe_complete = 0;
    issue_journal_txe(write_id);
    // normally we should wait here for the issue to complete
    if (!is_journal_txe_complete) {
        printf("ERROR: writing txe to journal failed!\n");
        return;
    }

    // checkpoint by writing metadata
    is_write_bitmap_complete = 0;
    is_write_inode_complete = 0;
    issue_write_bitmap(write_id);
    issue_write_inode(write_id);
    // normally we should wait here for the 2 issues to complete
    if (!is_write_bitmap_complete || !is_write_inode_complete) {
        printf("ERROR: writing metadata failed!\n");
        return;
    }

    // tell the file system that the write is complete
    write_complete(write_id);
}

/* Completion functions. The block service calls each one when the matching
 * write has reached persistent storage (e.g., it is physically written to
 * disk). In this simple version each one only sets its flag.
 * (void)write_id tells the compiler that write_id is unused on purpose;
 * your version will need it. */
void journal_txb_complete(int write_id) {
    (void)write_id;
    is_journal_txb_complete = 1;
}

void journal_bitmap_complete(int write_id) {
    (void)write_id;
    is_journal_bitmap_complete = 1;
}

void journal_inode_complete(int write_id) {
    (void)write_id;
    is_journal_inode_complete = 1;
}

void write_data_complete(int write_id) {
    (void)write_id;
    is_write_data_complete = 1;
}

void journal_txe_complete(int write_id) {
    (void)write_id;
    is_journal_txe_complete = 1;
}

void write_bitmap_complete(int write_id) {
    (void)write_id;
    is_write_bitmap_complete = 1;
}

void write_inode_complete(int write_id) {
    (void)write_id;
    is_write_inode_complete = 1;
}
