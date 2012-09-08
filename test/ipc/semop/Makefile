# Makefile for the tests

CC = exec cc
CFLAGS = -Wall -D_MINIX -D_POSIX_SOURCE -D_NETBSD_SOURCE=1 -I../lib/

PROG = semop01 semop02 semop03 semop04 semop05

all: $(PROG)

$(PROG): ../libipc.a
	$(CC) $(CFLAGS) -o $@ $@.c ../libipc.a

clean:
	rm -f *.o $(PROG)

test:
	sh ./test.sh

semop01: semop01.c
semop02: semop02.c
semop03: semop03.c
semop04: semop04.c
semop05: semop05.c

