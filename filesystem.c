/*
 * filesystem.c -- demo driver.
 * COM S 3520 Project 2 (Fall 2026)
 *
 * Requests N writes (./filesystem N, default NUM_WRITES), then waits until
 * every one of them has been reported complete.
 */
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include "journal.h"

#define NUM_WRITES 10

/* Counts finished writes: write_complete posts it once for each write, and
 * main waits on it once for each request. It starts at 0. */
static sem_t writes_done;

/* Called by journal.c when a write is fully journaled and checkpointed. */
void write_complete(int write_id) {
    printf("write %d complete\n", write_id);
    sem_post(&writes_done);
}

int main(int argc, char *argv[]) {
    int n = (argc > 1) ? atoi(argv[1]) : NUM_WRITES;
    sem_init(&writes_done, 0, 0);
    init_journal();
    for (int i = 1; i <= n; i++) {
        printf("request write %d\n", i);
        request_write(i);
    }
    for (int i = 1; i <= n; i++)
        sem_wait(&writes_done);
    printf("all %d writes complete\n", n);
    return 0;
}
