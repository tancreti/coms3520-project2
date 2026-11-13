# COM S 3520 Project 2 (Fall 2026)
#
#   make                 builds ./filesystem, the demo: filesystem.c,
#                        blockservice.c and journal.c. Run ./filesystem 10.
#   make tests           builds ./tests: tests.c, testlib.c, blockservice.c
#                        and journal.c. Run ./tests (see tests.c).
#   make public          builds your tests against each public buggy build
#                        (Section 2.8) and runs every test 5 times on each.
#   make tests CFLAGS="-g -fsanitize=thread"    ThreadSanitizer build
#   make tests JOURNAL=other.c                  test another journal.c (or .o)
#   make clean
#
# Extra .c files your tests need: list them in EXTRA, e.g. EXTRA = helpers.c
CC      = gcc
CFLAGS  ?= -Wall -Wextra -g
JOURNAL ?= journal.c
EXTRA   ?=
ARCH    := $(shell uname -m | sed -e 's/arm64/aarch64/')
HEADERS := journal.h blockservice.h testlib.h

filesystem: filesystem.c blockservice.c $(JOURNAL) journal.h
	$(CC) $(CFLAGS) -I. -o $@ filesystem.c blockservice.c $(JOURNAL) -pthread

tests: tests.c testlib.c blockservice.c $(EXTRA) $(JOURNAL) $(HEADERS)
	$(CC) $(CFLAGS) -I. -o $@ tests.c testlib.c blockservice.c $(EXTRA) $(JOURNAL) -pthread

public: tests.c testlib.c blockservice.c $(EXTRA) $(HEADERS)
	@test -d public/$(ARCH) || { echo "no public builds for $(ARCH)"; exit 1; }
	@for o in public/$(ARCH)/B*.o; do \
	  b=$$(basename $$o .o); \
	  $(CC) $(CFLAGS) -I. -o tests_$$b tests.c testlib.c blockservice.c $(EXTRA) $$o -pthread || exit 1; \
	  echo "==== $$b ===="; \
	  ./tests_$$b all 1 5; \
	done; true

clean:
	rm -f filesystem tests tests_B*
.PHONY: filesystem tests public clean
