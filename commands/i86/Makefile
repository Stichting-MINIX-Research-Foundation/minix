# Makefile for commands/i86.

CFLAGS	= -D_MINIX -D_POSIX_SOURCE
CCLD	= $(CC) -i $(CFLAGS)
MAKE	= exec make -$(MAKEFLAGS)

all:	cc

cc:	cc.c
	$(CCLD) -o $@ $?
	install -S 6kb $@

install:	/usr/bin/cc /usr/bin/m2 /usr/bin/pc

/usr/bin/cc:	cc
	install -cs -o bin $? $@

/usr/bin/m2 /usr/bin/pc:	/usr/bin/cc
	install -l $? $@

clean:
	rm -rf a.out core cc
