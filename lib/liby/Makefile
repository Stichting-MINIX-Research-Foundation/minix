# Makefile for lib/liby.

CFLAGS	= -O -D_MINIX -D_POSIX_SOURCE -wo
CC1	= $(CC) $(CFLAGS) -c

LIBRARY	= ../liby.a
all:	$(LIBRARY)

OBJECTS	= \
	$(LIBRARY)(main.o) \
	$(LIBRARY)(yyerror.o) \

$(LIBRARY):	$(OBJECTS)
	aal cr $@ *.o
	rm *.o

$(LIBRARY)(main.o):	main.c
	$(CC1) main.c

$(LIBRARY)(yyerror.o):	yyerror.c
	$(CC1) yyerror.c
