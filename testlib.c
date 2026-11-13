/*
 * testlib.c -- test runner and helpers (see testlib.h)
 * COM S 3520 Project 2 (Fall 2026)
 */
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "testlib.h"

static const char *cur_name = "?";
static unsigned cur_seed = 1;
static int result_fd = -1;           /* the real stdout, inside a test child */
static int capture_fd = -1;          /* read side of the journal's captured stdout */

/* write_complete is called by journal.c. */
void write_complete(int write_id) {
    bs_note_write_complete(write_id);
}

unsigned test_seed(void) { return cur_seed; }

void test_fail(const char *fmt, ...) {
    char buf[512];
    int n = snprintf(buf, sizeof buf, "FAIL %-26s seed %-4u ", cur_name, cur_seed);
    va_list ap;
    va_start(ap, fmt);
    n += vsnprintf(buf + n, sizeof buf - (size_t)n, fmt, ap);
    va_end(ap);
    if (n > (int)sizeof buf - 2) n = (int)sizeof buf - 2;
    buf[n++] = '\n';
    if (write(result_fd, buf, (size_t)n) < 0) { /* nothing to do */ }
    _exit(1);
}

static void on_alarm(int sig) {
    (void)sig;
    char buf[200];
    int n = snprintf(buf, sizeof buf, "FAIL %-26s seed %-4u did not finish within %d s "
                     "(a thread is stuck: deadlock or lost wakeup?)\n",
                     cur_name, cur_seed, TEST_SECONDS);
    if (write(result_fd, buf, (size_t)n) < 0) { /* nothing to do */ }
    _exit(1);
}

void sleep_ms(int ms) {
    struct timespec ts = { ms / 1000, (long)(ms % 1000) * 1000000L };
    while (nanosleep(&ts, &ts) != 0 && errno == EINTR) { }
}

double cpu_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

void request(int write_id) {
    bs_note_request(write_id);
    request_write(write_id);
}

/* ---- background client --------------------------------------------------- */

/* Each client thread gets its own copy of its range, so starting a new
 * client never changes what an earlier one requests. clients_returned()
 * counts only the newest client: a client counts its returns only while
 * its generation is still the current one. */
typedef struct { int first, n, gen; } client_t;

static pthread_mutex_t client_lock = PTHREAD_MUTEX_INITIALIZER;
static int client_gen;               /* generation of the newest client */
static int client_done;              /* returns counted for that client */

static void *client_main(void *arg) {
    client_t c = *(client_t *)arg;
    free(arg);
    for (int i = 0; i < c.n; i++) {
        request(c.first + i);
        pthread_mutex_lock(&client_lock);
        if (c.gen == client_gen) client_done++;
        pthread_mutex_unlock(&client_lock);
    }
    return NULL;
}

void start_client(int first, int n) {
    client_t *c = malloc(sizeof *c);
    if (!c) test_fail("start_client: out of memory");
    pthread_mutex_lock(&client_lock);
    c->first = first;
    c->n = n;
    c->gen = ++client_gen;
    client_done = 0;
    pthread_mutex_unlock(&client_lock);
    pthread_t t;
    if (pthread_create(&t, NULL, client_main, c) != 0)
        test_fail("start_client: pthread_create failed");
    pthread_detach(t);
}

int clients_returned(void) {
    pthread_mutex_lock(&client_lock);
    int n = client_done;
    pthread_mutex_unlock(&client_lock);
    return n;
}

void wait_for_writes(int n) {
    /* Give up a second before the alarm, so a stuck journal gets a useful
     * message: a protocol violation if there is one, otherwise a count. */
    double limit_ms = (TEST_SECONDS - 1) * 1000.0, waited = 0;
    while (bs_write_completes() < n) {
        if (waited >= limit_ms) {
            const char *msg = bs_check_protocol(0);
            if (msg) test_fail("protocol: %s", msg);
            test_fail("only %d of %d writes finished after %.0f s; the journal "
                      "is stuck (deadlock, lost wakeup, or waiting for a write "
                      "it never issued)", bs_write_completes(), n, limit_ms / 1000);
        }
        sleep_ms(1);
        waited += 1;
    }
}

void check_protocol(int flags) {
    const char *msg = bs_check_protocol(flags);
    if (msg) test_fail("protocol: %s", msg);
}

int output_contains(const char *text) {
    fflush(stdout);
    struct stat st;
    if (capture_fd < 0 || fstat(capture_fd, &st) != 0) return 0;
    char *buf = malloc((size_t)st.st_size + 1);
    if (!buf) return 0;
    ssize_t n = pread(capture_fd, buf, (size_t)st.st_size, 0);
    buf[n > 0 ? n : 0] = '\0';
    int found = strstr(buf, text) != NULL;
    free(buf);
    return found;
}

/* Send this process's stdout to an unlinked temporary file, kept readable
 * through capture_fd. */
static void capture_stdout(void) {
    char path[] = "/tmp/journal-test-XXXXXX";
    int fd = mkstemp(path);
    if (fd < 0) return;
    int w = open(path, O_WRONLY | O_APPEND);
    capture_fd = open(path, O_RDONLY);
    unlink(path);
    close(fd);
    if (w >= 0) { dup2(w, STDOUT_FILENO); close(w); }
}

/* ---- runner -------------------------------------------------------------- */

/* 1 if a debugger such as gdb is tracing this process (Linux only). The
 * hang alarm is skipped then, so a test is not killed while you sit at a
 * breakpoint. */
static int being_debugged(void) {
    FILE *f = fopen("/proc/self/status", "r");
    if (!f) return 0;
    char line[256];
    int tracer = 0;
    while (fgets(line, sizeof line, f))
        if (sscanf(line, "TracerPid: %d", &tracer) == 1) break;
    fclose(f);
    return tracer != 0;
}

static int run_one(const test_t *t, unsigned seed) {
    fflush(stdout);
    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return 0; }
    if (pid == 0) {
        cur_name = t->name;
        cur_seed = seed;
        result_fd = dup(STDOUT_FILENO);
        capture_stdout();
        setvbuf(stdout, NULL, _IOLBF, 0);
        signal(SIGALRM, on_alarm);
        if (!being_debugged()) alarm(TEST_SECONDS);
        bs_seed(seed);
        bs_verbose(0);
        t->fn();
        _exit(0);
    }
    int status;
    while (waitpid(pid, &status, 0) < 0 && errno == EINTR) { }
    if (WIFEXITED(status)) return WEXITSTATUS(status) == 0;
    printf("FAIL %-26s seed %-4u crashed (signal %d)\n", t->name, seed, WTERMSIG(status));
    return 0;
}

int run_tests(const test_t *tests, int ntests, int argc, char **argv) {
    const char *which = argc > 1 ? argv[1] : "all";
    unsigned seed = argc > 2 ? (unsigned)strtoul(argv[2], NULL, 10) : 1;
    int runs = argc > 3 ? atoi(argv[3]) : 1;
    if (runs < 1) runs = 1;
    int matched = 0, failed_tests = 0;
    /* STOP_ON_FAIL=1 in the environment: stop after the first failing test
     * (used to validate buggy builds, where one failure is enough). */
    const char *sof = getenv("STOP_ON_FAIL");
    int stop_on_fail = sof && *sof && strcmp(sof, "0") != 0;
    for (int i = 0; i < ntests; i++) {
        if (strcmp(which, "all") != 0 && strcmp(which, tests[i].name) != 0) continue;
        matched++;
        int passed = 0;
        for (int r = 0; r < runs; r++)
            passed += run_one(&tests[i], seed + (unsigned)r);
        if (passed < runs && stop_on_fail) {
            printf("stopped at the first failing test (STOP_ON_FAIL)\n");
            return 1;
        }
        if (passed == runs)
            printf("PASS %-26s %d/%d  %s\n", tests[i].name, passed, runs, tests[i].checks);
        else {
            printf("     %-26s %d/%d passed\n", tests[i].name, passed, runs);
            failed_tests++;
        }
    }
    if (!matched) {
        printf("no test named %s; tests are:\n", which);
        for (int i = 0; i < ntests; i++) printf("  %s\n", tests[i].name);
        return 2;
    }
    printf("%d of %d tests passed every run\n", matched - failed_tests, matched);
    return failed_tests ? 1 : 0;
}
