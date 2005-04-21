# Makefile for lib/i386/head.

CC1	= $(CC) -Was-ack -c

LIBRARY	= ../../libe.a
all:	$(LIBRARY)

$(LIBRARY):	$(LIBRARY)(em_head.o)
	aal cr $@ *.o
	rm *.o

$(LIBRARY)(em_head.o):	em_head.s
	$(CC1) -I. em_head.s
