# Makefile for lib/regex.

CFLAGS	= -O -D_MINIX -D_POSIX_SOURCE
CC1	= $(CC) $(CFLAGS) -c

LIBRARY	= ../libc.a

all:	$(LIBRARY)

OBJ	= \
	$(LIBRARY)(regcomp.o) \
	$(LIBRARY)(regerror.o) \
	$(LIBRARY)(regexec.o) \
	$(LIBRARY)(regfree.o) \

$(LIBRARY):	$(OBJ)
	aal cr $@ *.o
	rm *.o

$(LIBRARY)(regcomp.o):	regcomp.c
	$(CC1) regcomp.c

$(LIBRARY)(regerror.o):	regerror.c
	$(CC1) regerror.c

$(LIBRARY)(regexec.o):	regexec.c
	$(CC1) regexec.c

$(LIBRARY)(regfree.o):	regfree.c
	$(CC1) regfree.c
