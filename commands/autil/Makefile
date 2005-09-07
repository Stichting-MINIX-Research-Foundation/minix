# Makefile for commands/autil

CC = exec cc
CFLAGS	= -I. -D_MINIX -D_POSIX_SOURCE -wo
CCLD	= $(CC) -i $(CFLAGS)

all:	anm asize

anm:	anm.c rd.c rd_arhdr.c rd_bytes.c rd_unsig2.c
	$(CCLD) -o $@ $?
	install -S 32kw $@

asize:	asize.c
	$(CCLD) -o $@ $?
	install -S 4kw $@

install:	/usr/bin/anm /usr/bin/asize

/usr/bin/anm:	anm
	install -cs -o bin $? $@

/usr/bin/asize:	asize
	install -cs -o bin $? $@

clean:
	rm -f anm asize core
