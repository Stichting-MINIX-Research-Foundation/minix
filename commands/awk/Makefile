# Makefile for awk.

CFLAGS	= -D_MINIX -D_POSIX_SOURCE -wo -w
LDFLAGS	= -i -f

OBJS = m.o e.o n.o l.o r.o v.o y.o regexp.o k.o

all:	awk

awk:	$(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) #-lm
	install -S 32kw $@

install:	/usr/bin/awk

/usr/bin/awk:	awk
	install -cs -o bin $? $@

clean:
	rm -f awk *.o a.out *.bak core

e.o:	awk.h regexp.h
l.o:	awk.h
m.o:	awk.h
n.o:	awk.h
r.o:	awk.h regexp.h
regexp.o:	regexp.h
v.o:	awk.h regexp.h
y.o:	awk.h
