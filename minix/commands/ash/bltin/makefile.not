# Copyright (C) 1989 by Kenneth Almquist.  All rights reserved.
# This file is part of ash, which is distributed under the terms specified
# by the Ash General Public License.  See the file named LICENSE.

LIBFILES=catfcmd.o echocmd.o exprcmd.o linecmd.o nlechocmd.o\
	operators.o regexp.o
DEBUG=-g
CFLAGS=$(DEBUG)
#CC=gcc

all:$P bltinlib.a catf echo expr line nlecho true umask

bltinlib.a:$P $(LIBFILES)
	ar rc $@ $(LIBFILES)

catf: catf.c bltin.h ../shell.h ../error.h error.o stalloc.o
	$(CC) $(CFLAGS) -o $@ catf.c error.o stalloc.o

catfcmd.o: catf.c bltin.h ../shell.h ../error.h
	$(CC) -DSHELL $(CFLAGS) -c catf.c
	mv catf.o $@

expr: expr.c bltin.h ../shell.h operators.h operators.o regexp.o error.o stalloc.o
	$(CC) $(CFLAGS) -o $@ expr.c operators.o regexp.o error.o stalloc.o
	-rm -f test '['
	ln expr test
	ln expr '['

exprcmd.o: expr.c bltin.h ../shell.h operators.h
	$(CC) -DSHELL $(CFLAGS) -c expr.c
	mv expr.o $@

operators.c operators.h: unary_op binary_op mkexpr
	./mkexpr

operators.o: ../shell.h operators.h

regexp.o: bltin.h ../shell.h

echo: echo.c bltin.h ../shell.h
	$(CC) $(CFLAGS) -o $@ echo.c

echocmd.o: echo.c bltin.h ../shell.h
	$(CC) -DSHELL $(CFLAGS) -c echo.c
	mv echo.o $@

line: line.c bltin.h ../shell.h
	$(CC) $(CFLAGS) -o $@ line.c

linecmd.o: line.c bltin.h ../shell.h
	$(CC) -DSHELL $(CFLAGS) -c line.c
	mv line.o $@

nlecho: nlecho.c bltin.h ../shell.h
	$(CC) $(CFLAGS) -o $@ nlecho.c

nlechocmd.o: nlecho.c bltin.h ../shell.h
	$(CC) -DSHELL $(CFLAGS) -c nlecho.c
	mv nlecho.o $@

umask: umask.c bltin.h
	$(CC) $(CFLAGS) -o $@ umask.c

true:
	> :
	chmod 755 :
	rm -f true
	ln : true

stalloc.o: ../shell.h

