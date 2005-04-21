# Makefile for telnet

CFLAGS=	-D_MINIX -D_POSIX_SOURCE
LDFLAGS=-i

SRC=	ttn.c ttn_conf.c

all:	ttn

ttn:	$(SRC)
	$(CC) $(CFLAGS) $(LDFLAGS) -o ttn $(SRC)
	install -S 4kw $@

clean:
	rm -f ttn

install:	/usr/bin/telnet

/usr/bin/telnet:	ttn
	install -cs -o bin ttn $@
