# Makefile for the tests

CC = exec cc
CFLAGS = -Wall -I../lib/

PROG =	shmdt01 shmdt02

all: $(PROG)

$(PROG): ../libipc.a
	$(CC) $(CFLAGS) -o $@ $@.c ../libipc.a

clean:
	rm -f *.o $(PROG)

test:
	sh ./test.sh

shmdt01: shmdt01.c
shmdt02: shmdt02.c
