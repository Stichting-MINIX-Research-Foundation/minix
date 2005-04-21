# Makefile for lib/tmrslib.

CFLAGS	= -O -D_MINIX -D_POSIX_SOURCE
CC1	= $(CC) $(CFLAGS) -c

LIBTMRS	= ../libtimers.a
all:	$(LIBTMRS)

OBJECTS	= \
	$(LIBTMRS)(tmrs_set.o) \
	$(LIBTMRS)(tmrs_clr.o) \
	$(LIBTMRS)(tmrs_exp.o) \
	 

$(LIBTMRS):	$(OBJECTS)
	aal cr $@ *.o
	rm *.o

$(LIBTMRS)(tmrs_set.o):		tmrs_set.c
	$(CC1) tmrs_set.c

$(LIBTMRS)(tmrs_clr.o):		tmrs_clr.c
	$(CC1) tmrs_clr.c

$(LIBTMRS)(tmrs_exp.o):		tmrs_exp.c
	$(CC1) tmrs_exp.c


