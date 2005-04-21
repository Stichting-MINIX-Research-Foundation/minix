# Makefile for lib/rts.

CFLAGS	= -O -D_MINIX -D_POSIX_SOURCE
CC1	= $(CC) $(CFLAGS) -c

LIBRARY	= ../libc.a

all:	\
	$(LIBRARY)

OBJECTS	= \
	$(LIBRARY)(setjmp.o) \

$(LIBRARY):	$(OBJECTS)
	aal cr $@ *.o
	rm *.o

$(LIBRARY)(setjmp.o):	setjmp.e
	$(CC1) setjmp.e
