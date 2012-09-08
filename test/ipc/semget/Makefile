# Makefile for the tests

CC = exec cc
CFLAGS = -Wall -D_MINIX -D_POSIX_SOURCE -D_NETBSD_SOURCE=1 -I../lib/

PROG =	semget01 semget02 semget03 semget05 semget06

all: $(PROG)

$(PROG): ../libipc.a
	$(CC) $(CFLAGS) -o $@ $@.c ../libipc.a

clean:
	rm -f *.o $(PROG)

test:
	sh ./test.sh

semget01: semget01.c
semget02: semget02.c
semget03: semget03.c
semget05: semget05.c
semget06: semget06.c
