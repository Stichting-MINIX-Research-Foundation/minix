# Makefile for lib/math.

CFLAGS	= -O -D_MINIX -D_POSIX_SOURCE
CC1	= $(CC) $(CFLAGS) -c

LIBRARY	= ../libc.a
all:	$(LIBRARY)

OBJECTS	= \
	$(LIBRARY)(asin.o) \
	$(LIBRARY)(atan.o) \
	$(LIBRARY)(atan2.o) \
	$(LIBRARY)(ceil.o) \
	$(LIBRARY)(exp.o) \
	$(LIBRARY)(fabs.o) \
	$(LIBRARY)(floor.o) \
	$(LIBRARY)(fmod.o) \
	$(LIBRARY)(frexp.o) \
	$(LIBRARY)(hugeval.o) \
	$(LIBRARY)(isnan.o) \
	$(LIBRARY)(ldexp.o) \
	$(LIBRARY)(log.o) \
	$(LIBRARY)(log10.o) \
	$(LIBRARY)(modf.o) \
	$(LIBRARY)(pow.o) \
	$(LIBRARY)(sin.o) \
	$(LIBRARY)(sinh.o) \
	$(LIBRARY)(sqrt.o) \
	$(LIBRARY)(tan.o) \
	$(LIBRARY)(tanh.o) \

$(LIBRARY):	$(OBJECTS)
	aal cr $@ *.o
	rm *.o

$(LIBRARY)(asin.o):	asin.c
	$(CC1) asin.c

$(LIBRARY)(atan.o):	atan.c
	$(CC1) atan.c

$(LIBRARY)(atan2.o):	atan2.c
	$(CC1) atan2.c

$(LIBRARY)(ceil.o):	ceil.c
	$(CC1) ceil.c

$(LIBRARY)(exp.o):	exp.c
	$(CC1) exp.c

$(LIBRARY)(fabs.o):	fabs.c
	$(CC1) fabs.c

$(LIBRARY)(floor.o):	floor.c
	$(CC1) floor.c

$(LIBRARY)(fmod.o):	fmod.c
	$(CC1) fmod.c

$(LIBRARY)(frexp.o):	frexp.s
	$(CC1) frexp.s

$(LIBRARY)(hugeval.o):	hugeval.c
	$(CC1) hugeval.c

$(LIBRARY)(isnan.o):	isnan.c
	$(CC1) isnan.c

$(LIBRARY)(ldexp.o):	ldexp.c
	$(CC1) ldexp.c

$(LIBRARY)(log.o):	log.c
	$(CC1) log.c

$(LIBRARY)(log10.o):	log10.c
	$(CC1) log10.c

$(LIBRARY)(modf.o):	modf.s
	$(CC1) modf.s

$(LIBRARY)(pow.o):	pow.c
	$(CC1) pow.c

$(LIBRARY)(sin.o):	sin.c
	$(CC1) sin.c

$(LIBRARY)(sinh.o):	sinh.c
	$(CC1) sinh.c

$(LIBRARY)(sqrt.o):	sqrt.c
	$(CC1) sqrt.c

$(LIBRARY)(tan.o):	tan.c
	$(CC1) tan.c

$(LIBRARY)(tanh.o):	tanh.c
	$(CC1) tanh.c
