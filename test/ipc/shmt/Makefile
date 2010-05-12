# Makefile for the tests

CC = exec cc
CFLAGS = -Wall -D_MINIX -D_POSIX_SOURCE -I../lib/

PROG =	shmt02 shmt03 shmt04 shmt05 shmt06 \
	shmt07 shmt08 shmt10 shmt01

all: $(PROG)

$(PROG): tst_res.o
	$(CC) $(CFLAGS) -o $@ $@.c tst_res.o

tst_res.o: ../lib/tst_res.c
	$(CC) $(CFLAGS) -c -o tst_res.o ../lib/tst_res.c

test:
	sh testshm.sh

clean:
	rm -f *.o $(PROG)

shmt01: shmt01.c
shmt02: shmt02.c
shmt03: shmt03.c
shmt04: shmt04.c
shmt05: shmt05.c
shmt06: shmt06.c
shmt07: shmt07.c
shmt08: shmt08.c
shmt10: shmt10.c
