# Makefile for mined

CFLAGS	= -O -wo -D_MINIX -D_POSIX_SOURCE

OBJ = mined1.o mined2.o

all:	mined

mined:	$(OBJ)
	$(CC) -i -o $@ $(OBJ)
	install -S 64k $@

install:	/usr/bin/mined

/usr/bin/mined:	mined
	install -cs -o bin mined $@

$(OBJ):	mined.h

clean:
	rm -f mined *.o *.s core *.bak
