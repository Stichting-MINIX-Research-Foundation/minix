# Makefile for rlogind.

CFLAGS	= $(OPT) -D_MINIX
LDFLAGS	=

all:	rlogind

OBJ=	rlogind.o setup.o

rlogind:	$(OBJ)
	$(CC) $(LDFLAGS) -o $@ $(OBJ)
	install -S 4kw $@

install:	/usr/bin/in.rlogind

/usr/bin/in.rlogind:	rlogind
	install -c $? $@

clean:
	rm -f *.o rlogind core a.out

# Dependencies.
$(OBJ):		rlogind.h
