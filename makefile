# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2026 thunderbird1400@gmx.de

CFLAGS ?= -O2

all: type-a type-b

type-a: task.c
	$(CC) $(CFLAGS) -Wall -Wextra -o $@ task.c

type-b: task.c
	$(CC) $(CFLAGS) -Wall -Wextra -DMOCK_TYPE_B -o $@ task.c

clean:
	rm -f type-a type-b

.PHONY: all clean
