FROM ubuntu:24.04

# Project 2 needs only the native C toolchain: gcc, make, and the pthread and
# semaphore libraries (build-essential), plus gdb for debugging. gcc's
# ThreadSanitizer runtime (make tests CFLAGS="-g -fsanitize=thread") comes
# with build-essential.
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        build-essential \
        gdb \
        git \
        nano \
        vim \
        less \
        tree \
        file \
        curl \
        ca-certificates \
        && rm -rf /var/lib/apt/lists/*

# The test runner (testlib.c) runs each test in a child process made with
# fork, so gdb must follow the child for "gdb ./tests" then "run NAME" to stop
# at breakpoints in journal.c.
RUN mkdir -p /etc/gdb && echo 'set follow-fork-mode child' >> /etc/gdb/gdbinit
