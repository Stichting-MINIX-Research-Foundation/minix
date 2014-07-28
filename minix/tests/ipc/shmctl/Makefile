# Makefile for the tests

CC = exec cc
CFLAGS = -Wall -I../lib/

PROG =	shmctl01 shmctl02 shmctl03 shmctl04

all: $(PROG)

$(PROG): ../libipc.a
	$(CC) $(CFLAGS) -o $@ $@.c ../libipc.a

clean:
	rm -f *.o $(PROG)

test:
	sh ./test.sh

shmctl01: shmctl01.c
shmctl02: shmctl02.c
shmctl03: shmctl03.c
shmctl04: shmctl04.c
