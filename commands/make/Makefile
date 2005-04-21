# Makefile for make (!)

CFLAGS = -O -Dunix -D_MINIX -D_POSIX_SOURCE

OBJ =	check.o input.o macro.o main.o make.o reader.o rules.o archive.o

all:	make

make :	$(OBJ)
	$(CC) -i -o make $(OBJ)
	install -S 96k make

install:	/usr/bin/make

/usr/bin/make:	make
	install -cs -o bin make $@
 
$(OBJ): h.h

clean:
	rm -f *.o *.bak core make
