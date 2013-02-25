# Makefile for the tests

CC = exec cc
CFLAGS = -Wall -I../lib/

PROG =	shmget01 shmget02 shmget04 shmget05

all: $(PROG)

$(PROG): ../libipc.a
	$(CC) $(CFLAGS) -o $@ $@.c ../libipc.a

clean:
	rm -f *.o $(PROG)

test:
	sh ./test.sh

shmget01: shmget01.c
shmget02: shmget02.c
shmget04: shmget04.c
shmget05: shmget05.c
