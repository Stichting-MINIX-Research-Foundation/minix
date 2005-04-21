# Makefile for sh

CFLAGS	= -O -D_MINIX -D_POSIX_SOURCE -wa
LDFLAGS	= -i

OBJ	= sh1.o sh2.o sh3.o sh4.o sh5.o sh6.o

all:	sh

sh:	$(OBJ)
	cc $(LDFLAGS) -o $@ $(OBJ)
	install -S 11kw sh

install:	/usr/bin/msh

/usr/bin/msh:	sh
	install -cs -o bin $? $@

#/usr/bin/sh:	/usr/bin/msh
#	install -l $? $@
#
#/bin/sh:	/usr/bin/msh
#	install -lcs $? $@

$(OBJ):	sh.h

clean:
	rm -f sh *.o *.bak core
