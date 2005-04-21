# Makefile for lib/end.

CC1	= $(CC) -c

LIBRARY	= ../end.a
all:	$(LIBRARY)

OBJECTS	= \
	$(LIBRARY)(edata.o) \
	$(LIBRARY)(em_end.o) \
	$(LIBRARY)(end.o) \
	$(LIBRARY)(etext.o) \

$(LIBRARY):	$(OBJECTS)
	aal cr $@ *.o
	rm *.o

$(LIBRARY)(edata.o):	edata.s
	$(CC1) edata.s

$(LIBRARY)(em_end.o):	em_end.s
	$(CC1) em_end.s

$(LIBRARY)(end.o):	end.s
	$(CC1) end.s

$(LIBRARY)(etext.o):	etext.s
	$(CC1) etext.s
