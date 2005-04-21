# Makefile for zmodem

all: rz sz

CFLAGS= -DPOSIX -D_MINIX -D_POSIX_SOURCE -O -wo

rz: rz.c rbsb.c zm.c zmodem.h
	cc $(CFLAGS) -i -o rz rz.c
	install -S 5kw $@

sz: sz.c rbsb.c zm.c zmodem.h
	cc $(CFLAGS) -i -o sz sz.c
	install -S 5kw $@

install:	/usr/bin/rz /usr/bin/sz

/usr/bin/rz:	rz
	install -cs -o bin rz $@

/usr/bin/sz:	sz
	install -cs -o bin sz $@

clean:	
	rm -f *.bak *.o core rz sz
