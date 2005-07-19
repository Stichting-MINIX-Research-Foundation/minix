# Makefile for lib/util.

CFLAGS	= -O -D_MINIX -D_POSIX_SOURCE
CC1	= $(CC) $(CFLAGS) -c

LIBUTIL	= ../libutil.a
all:	$(LIBUTIL)

OBJECTS	= \
	$(LIBUTIL)(openpty.o) 
	 

$(LIBUTIL):	$(OBJECTS)
	aal cr $@ *.o
	rm *.o

$(LIBUTIL)(openpty.o):	openpty.c
	$(CC1) openpty.c

