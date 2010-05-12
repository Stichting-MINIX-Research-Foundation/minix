# Makefile for the tests

CC = exec cc
CFLAGS = -Wall -D_MINIX -D_POSIX_SOURCE -I../lib/

PROG =	shmctl01 shmctl02 shmctl03 shmctl04

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

shmctl01: shmctl01.c
shmctl02: shmctl02.c
shmctl03: shmctl03.c
shmctl04: shmctl04.c
