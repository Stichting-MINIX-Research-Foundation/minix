# Makefile for M4

#	-DEXTENDED	#if you like to get paste & spaste macros.
#	-DVOID 		#if your C compiler does NOT support void.
#	-DGETOPT	#if you STILL do not have getopt in your library.
#	-DDUFFCP	#if you do not have fast memcpy in your library.
#

CFLAGS = -DEXTENDED -O -D_POSIX_SOURCE -D_MINIX

OBJ =  main.o eval.o serv.o look.o misc.o expr.o
INCL =  mdef.h extr.h patchlevel.h

all:	m4

m4: $(OBJ) $(INCL)
	cc -i -o m4 $(OBJ)
	install -S 4kw m4

install:	/usr/bin/m4

/usr/bin/m4:	m4
	install -cs -o bin m4 $@

clean:	
	rm -f *.o m4 core *bak
