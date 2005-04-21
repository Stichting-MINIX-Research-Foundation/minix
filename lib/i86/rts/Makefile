# Makefile for lib/i86/rts.

CFLAGS	= -O -D_MINIX -D_POSIX_SOURCE -Was-ncc
CC1	= $(CC) $(CFLAGS) -c

LIBRARY	= ../../libc.a

all:	\
	../../ncrtso.o \
	../../nm2rtso.o \
	../../nprtso.o \
	$(LIBRARY)

../../ncrtso.o:	ncrtso.s
	$(CC1) -c ncrtso.s
	mv ncrtso.o $@

../../nm2rtso.o:	nm2rtso.s
	$(CC1) -c nm2rtso.s
	mv nm2rtso.o $@

../../nprtso.o:	nprtso.s
	$(CC1) -c nprtso.s
	mv nprtso.o $@

OBJECTS	= \
	$(LIBRARY)(__sigreturn.o) \
	$(LIBRARY)(_sendrec.o) \
	$(LIBRARY)(brksize.o) \

$(LIBRARY):	$(OBJECTS)
	aal cr $@ *.o
	rm *.o

$(LIBRARY)(__sigreturn.o):	__sigreturn.s
	$(CC1) __sigreturn.s

$(LIBRARY)(_sendrec.o):	_sendrec.s
	$(CC1) _sendrec.s

$(LIBRARY)(brksize.o):	brksize.s
	$(CC1) brksize.s
