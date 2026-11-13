/*
 * journal.h -- the API for the journaling layer
 * COM S 3520 Project 2 (Fall 2026)
 *
 * Metadata journaling layer. Do NOT modify this file!
 * The autograder ignores any modifications you make and replaces it with
 * this exact copy.
 *
 * BUFFER_SIZE is the capacity of each of the three request buffers.
 */
#ifndef JOURNAL_H
#define JOURNAL_H

#define BUFFER_SIZE 4

/* filesystem.c (you are NOT implementing this) */
void write_complete(int write_id);

/* journal.c (you are implementing this) */
void init_journal(void);
void request_write(int write_id);
void journal_txb_complete(int write_id);
void journal_bitmap_complete(int write_id);
void journal_inode_complete(int write_id);
void write_data_complete(int write_id);
void journal_txe_complete(int write_id);
void write_bitmap_complete(int write_id);
void write_inode_complete(int write_id);

/* blockservice.c (you are NOT implementing this) */
void issue_journal_txb(int write_id);
void issue_journal_bitmap(int write_id);
void issue_journal_inode(int write_id);
void issue_write_data(int write_id);
void issue_journal_txe(int write_id);
void issue_write_bitmap(int write_id);
void issue_write_inode(int write_id);

#endif
