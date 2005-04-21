# Makefile for de

CC	= cc
CFLAGS	= -O -D_MINIX -D_POSIX_SOURCE -D_POSIX_1_SOURCE=2

all:	de

OBJS	= de.o de_stdin.o de_stdout.o de_diskio.o de_recover.o

de:	$(OBJS)
	$(CC) -i $(OBJS) -o de
	install -S 4kw de

install:	/usr/bin/de

/usr/bin/de:	de
	install -cs -o bin de $@

$(OBJS): de.h

clean:
	rm -f *.bak *.o de
