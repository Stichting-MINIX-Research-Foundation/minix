# Makefile for the tests

CC = exec cc
CFLAGS = -Wall -D_MINIX -D_POSIX_SOURCE -I../lib/

PROG = semctl01 semctl02 semctl03 semctl04 semctl05 semctl06 semctl07

all: $(PROG)

$(PROG): tst_res.o libipc.o tst_sig.o parse_opts.o tst_tmpdir.o rmobj.o
	$(CC) $(CFLAGS) -o $@ $@.c tst_res.o libipc.o tst_sig.o parse_opts.o tst_tmpdir.o rmobj.o

rmobj.o: ../lib/rmobj.c
	$(CC) $(CFLAGS) -c -o rmobj.o ../lib/rmobj.c

tst_res.o: ../lib/tst_res.c
	$(CC) $(CFLAGS) -c -o tst_res.o ../lib/tst_res.c

tst_sig.o: ../lib/tst_sig.c
	$(CC) $(CFLAGS) -c -o tst_sig.o ../lib/tst_sig.c

tst_tmpdir.o: ../lib/tst_tmpdir.c
	$(CC) $(CFLAGS) -c -o tst_tmpdir.o ../lib/tst_tmpdir.c

parse_opts.o: ../lib/parse_opts.c
	$(CC) $(CFLAGS) -c -o parse_opts.o ../lib/parse_opts.c

libipc.o: ../lib/libipc.c
	$(CC) $(CFLAGS) -c -o libipc.o ../lib/libipc.c

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

