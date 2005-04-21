# Makefile for bc

# A makefile for bc.  This is part of the bc/sbc distribution.
#
#
#  Make sure these have the correct directories for your machine.
#
#  LIBDIR and BINDIR are where bc and libmath.b will be put.
#
PREFIX = /usr
LIBDIR = $(PREFIX)/lib
BINDIR = $(PREFIX)/bin
#
# Programs definitions for use by make.
#
SHELL = /bin/sh
YACC = yacc
#YACC = bison -y
LEX = flex -I8
#LEX = lex
CC = cc
CFLAGS = -D_POSIX_SOURCE
LDFLAGS = -i
#
#
OFILES = scan.o util.o main.o number.o storage.o load.o execute.o 
#
SUBDIRS = Examples Test
#


all: bc
bc: $& config.h bc.o $(OFILES) global.o
	$(CC) -o bc $(LDFLAGS) bc.o $(OFILES) global.o

sbc: sbc.o $(OFILES) global.o
	$(CC) -o sbc $(LDFLAGS) sbc.o $(OFILES) global.o

math.h: libmath.b
	$(MAKE) -$(MAKEFLAGS) fbc
	./fbc -c libmath.b </dev/null >math.h
	./fix_math.h
	rm -f ./fbc

fbc: $(OFILES) bc.o
	echo \"\" > math.h
	$(CC) -c $(CFLAGS) global.c
	$(CC) -o fbc $(LDFLAGS) bc.o $(OFILES) global.o

install:	$(BINDIR)/bc $(LIBDIR)/libmath.b

$(BINDIR)/bc:	bc
	install -cs -o bin $? $@

$(LIBDIR)/libmath.b:	libmath.b
	install -c -o bin $? $@

clean:
	rm -f *.o *.bak core math.h bc sbc

scan.c: scan.l
	$(LEX) scan.l
	mv lex.yy.c scan.c

scan.o:	scan.c
	$(CC) -c $(CFLAGS) -wa scan.c

y.tab.h bc.c: bc.y
	@echo "expect 1 shift/reduce conflict"
	$(YACC) -d bc.y
	mv y.tab.c bc.c

sbc.c: sbc.y
	$(YACC) -d sbc.y
	mv y.tab.c sbc.c

global.o: bcdefs.h global.h math.h
bc.o:	bcdefs.h global.h 
execute.o: bcdefs.h global.h
load.o: bcdefs.h global.h 
main.o: bcdefs.h global.h version.h
number.o: bcdefs.h
sbc.o: bcdefs.h global.h 
scan.o: y.tab.h bcdefs.h global.h
storage.o: bcdefs.h global.h
util.o: bcdefs.h global.h version.h

bcdefs.h: number.h const.h config.h
	touch bcdefs.h
