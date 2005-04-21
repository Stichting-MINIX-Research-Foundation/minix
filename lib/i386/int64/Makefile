# Makefile for lib/i386/int64.

CFLAGS	= -O -D_MINIX -D_POSIX_SOURCE -Was-ack
CC1	= $(CC) $(CFLAGS) -c

LIBRARY	= ../../libc.a
all:	$(LIBRARY)

OBJECTS	= \
	$(LIBRARY)(add64.o) \
	$(LIBRARY)(add64u.o) \
	$(LIBRARY)(cmp64.o) \
	$(LIBRARY)(cv64u.o) \
	$(LIBRARY)(cvu64.o) \
	$(LIBRARY)(diff64.o) \
	$(LIBRARY)(div64u.o) \
	$(LIBRARY)(ex64.o) \
	$(LIBRARY)(make64.o) \
	$(LIBRARY)(mul64u.o) \
	$(LIBRARY)(sub64.o) \
	$(LIBRARY)(sub64u.o) \

$(LIBRARY):	$(OBJECTS)
	aal cr $@ *.o
	rm *.o

$(LIBRARY)(add64.o):	add64.s
	$(CC1) add64.s

$(LIBRARY)(add64u.o):	add64u.s
	$(CC1) add64u.s

$(LIBRARY)(cmp64.o):	cmp64.s
	$(CC1) cmp64.s

$(LIBRARY)(cv64u.o):	cv64u.s
	$(CC1) cv64u.s

$(LIBRARY)(cvu64.o):	cvu64.s
	$(CC1) cvu64.s

$(LIBRARY)(diff64.o):	diff64.s
	$(CC1) diff64.s

$(LIBRARY)(div64u.o):	div64u.s
	$(CC1) div64u.s

$(LIBRARY)(ex64.o):	ex64.s
	$(CC1) ex64.s

$(LIBRARY)(make64.o):	make64.s
	$(CC1) make64.s

$(LIBRARY)(mul64u.o):	mul64u.s
	$(CC1) mul64u.s

$(LIBRARY)(sub64.o):	sub64.s
	$(CC1) sub64.s

$(LIBRARY)(sub64u.o):	sub64u.s
	$(CC1) sub64u.s
