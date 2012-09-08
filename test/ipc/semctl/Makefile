# Makefile for the tests

CC = exec cc
CFLAGS = -Wall -D_MINIX -D_POSIX_SOURCE -D_NETBSD_SOURCE=1 -I../lib/

PROG = semctl01 semctl02 semctl03 semctl04 semctl05 semctl06 semctl07

all: $(PROG)

$(PROG): ../libipc.a
	$(CC) $(CFLAGS) -o $@ $@.c ../libipc.a

clean:
	rm -f *.o $(PROG)

test:
	sh ./test.sh

semctl01: semctl01.c
semctl02: semctl02.c
semctl03: semctl03.c
semctl04: semctl04.c
semctl05: semctl05.c
semctl06: semctl06.c
semctl07: semctl07.c

