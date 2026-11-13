# COM S 3520 Project 2 (Fall 2026)
#
#   make                 builds ./filesystem, the demo: filesystem.c,
#                        blockservice.c and journal.c. Run ./filesystem 10.
#   make tests           builds ./tests: tests.c, testlib.c, blockservice.c
#                        and journal.c. Run ./tests (see tests.c).
#   make simple          builds ./tests_simple with simple_journal.c (the
#                        simple version) in place of journal.c, and runs every
#                        test once. Each required test should fail on it.
#                        Run one test: ./tests_simple NAME
#   make tsan            builds ./tests_tsan with ThreadSanitizer and runs
#                        every test once; a data race fails the test and
#                        prints a report. Run one test:
#                        setarch $(uname -m) -R ./tests_tsan NAME
#   make public          builds your tests against each public buggy build
#                        (Section 2.8) and runs every test 5 times on each.
#   make tests JOURNAL=other.c                  test another journal.c (or .o)
#   make clean
#
# make simple and make public expect some tests to fail, so they always
# finish with exit status 0: read the PASS and FAIL lines.
#
# Extra .c files your tests need: list them in EXTRA, e.g. EXTRA = helpers.c
# (the autograder compiles every .c file you submit except journal.c,
# blockservice.c and simple_journal.c into your tests).
CC      = gcc
CFLAGS  ?= -Wall -Wextra -g
JOURNAL ?= journal.c
EXTRA   ?=
ARCH    := $(shell uname -m | sed -e 's/arm64/aarch64/')
HEADERS := journal.h blockservice.h testlib.h
TESTSRC := tests.c testlib.c blockservice.c $(EXTRA)

filesystem: filesystem.c blockservice.c $(JOURNAL) journal.h
	$(CC) $(CFLAGS) -I. -o $@ filesystem.c blockservice.c $(JOURNAL) -pthread

tests: $(TESTSRC) $(JOURNAL) $(HEADERS)
	$(CC) $(CFLAGS) -I. -o $@ $(TESTSRC) $(JOURNAL) -pthread

simple: $(TESTSRC) simple_journal.c $(HEADERS)
	$(CC) $(CFLAGS) -I. -o tests_simple $(TESTSRC) simple_journal.c -pthread
	./tests_simple || true

# ThreadSanitizer needs address randomization off (setarch -R) on some
# kernels; the dev container allows it.
tsan: $(TESTSRC) $(JOURNAL) $(HEADERS)
	$(CC) $(CFLAGS) -O1 -fsanitize=thread -I. -o tests_tsan $(TESTSRC) $(JOURNAL) -pthread
	TSAN_OPTIONS="halt_on_error=1 $$TSAN_OPTIONS" setarch $$(uname -m) -R ./tests_tsan

public: $(TESTSRC) $(HEADERS)
	@test -d public/$(ARCH) || { echo "no public builds for $(ARCH)"; exit 1; }
	@for o in public/$(ARCH)/B*.o; do \
	  b=$$(basename $$o .o); \
	  $(CC) $(CFLAGS) -I. -o tests_$$b $(TESTSRC) $$o -pthread || exit 1; \
	  echo "==== $$b ===="; \
	  ./tests_$$b all 1 5; \
	done; true

clean:
	rm -f filesystem tests tests_B* tests_simple tests_tsan
.PHONY: filesystem tests simple tsan public clean
