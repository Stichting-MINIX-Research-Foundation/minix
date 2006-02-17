# Makefile for pax

CC = exec cc
CFLAGS = -O -D_POSIX_SOURCE -DNET2_STAT=1 -D_MINIX=1
LDFLAGS= -i

all:	pax

OBJ = ar_io.o ar_subs.o buf_subs.o cache.o cpio.o file_subs.o ftree.o \
gen_subs.o getoldopt.o options.o pat_rep.o pax.o sel_subs.o \
tables.o tar.o tty_subs.o fgetln.o

pax:	$(OBJ)
	$(CC) $(LDFLAGS) -o $@ $(OBJ)
	install -S 256k $@

install: pax
	install -cs -o bin pax /usr/bin/pax

clean:	
	rm -f *.o *.bak core pax
