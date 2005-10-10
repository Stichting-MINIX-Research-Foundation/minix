CC=gcc
AR=gar
AS=gas

VPATH=$(SRCDIR)/gnu/rts

#Makefile for lib/gcc/mach/minix.i386/libsys.

LIBRARY	= ../../libc.a

OBJECTS	= \
	_longjmp.o \
	_setjmp.o \
	longjmp.o \
	setjmp.o \

all: $(LIBRARY)

$(LIBRARY):	$(OBJECTS)
	$(AR) cr $@ *.o
	
_longjmp.o:	_longjmp.s
_setjmp.o:	_setjmp.s
longjmp.o:	longjmp.s
setjmp.o:	setjmp.s

# $PchId: Makefile,v 1.4 1996/02/22 21:54:11 philip Exp $
