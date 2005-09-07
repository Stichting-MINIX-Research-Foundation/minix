# Makefile for indent

CFLAGS= -c -O -D_MINIX -D_POSIX_SOURCE -wo -m
CC = exec cc

all:	indent

OBJ = args.o comment.o lexi.o indent.o parse.o io.o

indent: ${OBJ}
	$(CC) -o indent -i ${OBJ}
	install -S 32kw $@

install:	/usr/bin/indent

/usr/bin/indent:	indent
	install -cs -o bin indent $@

$(OBJ): globs.h codes.h

clean:
	rm -f *.bak *.o core indent
