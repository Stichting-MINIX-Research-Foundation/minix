# Makefile for the tests

CC = exec cc
CFLAGS = -Wall -I../lib/

PROG =	shmt02 shmt03 shmt04 shmt05 shmt06 \
	shmt07 shmt08 shmt10 shmt01

all: $(PROG)

$(PROG): ../libipc.a
	$(CC) $(CFLAGS) -o $@ $@.c ../libipc.a

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
