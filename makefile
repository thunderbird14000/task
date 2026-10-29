# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2026 thunderbird1400@gmx.de

CFLAGS ?= -O2

all: type-a type-b task-stub

type-a: task-mock.c
	$(CC) $(CFLAGS) -Wall -Wextra -pthread -o $@ task-mock.c

type-b: task-mock.c
	$(CC) $(CFLAGS) -Wall -Wextra -pthread -DMOCK_TYPE_B -o $@ task-mock.c

task-stub: task-stub.c
	$(CC) $(CFLAGS) -Wall -Wextra -pthread -o $@ task-stub.c

clean:
	rm -f type-a type-b task-stub

.PHONY: all clean
