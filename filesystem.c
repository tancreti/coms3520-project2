/*
 * filesystem.c -- demo driver.
 * COM S 3520 Project 2 (Fall 2026)
 *
 * Requests N writes (./filesystem N, default NUM_WRITES), then waits until
 * every one of them has been reported complete.
 */
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include "journal.h"

#define NUM_WRITES 10

static pthread_mutex_t done_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  done_cv   = PTHREAD_COND_INITIALIZER;
static int completed = 0;

/* Called by journal.c when a write is fully journaled and checkpointed. */
void write_complete(int write_id) {
    printf("write %d complete\n", write_id);
    pthread_mutex_lock(&done_lock);
    completed++;
    pthread_cond_broadcast(&done_cv);
    pthread_mutex_unlock(&done_lock);
}

int main(int argc, char *argv[]) {
    int n = (argc > 1) ? atoi(argv[1]) : NUM_WRITES;
    init_journal();
    for (int i = 1; i <= n; i++) {
        printf("request write %d\n", i);
        request_write(i);
    }
    pthread_mutex_lock(&done_lock);
    while (completed < n)
        pthread_cond_wait(&done_cv, &done_lock);
    pthread_mutex_unlock(&done_lock);
    printf("all %d writes complete\n", n);
    return 0;
}
