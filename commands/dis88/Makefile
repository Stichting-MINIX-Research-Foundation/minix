# Makefile for dis

# @(#) Makefile, Ver. 2.1 created 00:00:00 87/09/01
# Makefile for 8088 symbolic disassembler

# Copyright (C) 1987 G. M. Harding, all rights reserved.
# Permission to copy and redistribute is hereby granted,
# provided full source code, with all copyright notices,
# accompanies any redistribution.

# This Makefile  automates the process of compiling and linking
# a symbolic  object-file  disassembler  program  for the Intel
# 8088 CPU. Relatively machine-independent code is contained in
# the file dismain.c; lookup tables and handler routines, which
# are by their nature  machine-specific,  are  contained in two
# files named distabs.c and dishand.c,  respectively.  (A third
# machine-specific file, disfp.c, contains handler routines for
# floating-point  coprocessor opcodes.)  A header file,  dis.h,
# attempts to mediate between the machine-specific and machine-
# independent portions of the code. An attempt has been made to
# isolate machine  dependencies and to deal with them in fairly
# straightforward ways. Thus, it should be possible to target a
# different CPU by rewriting the handler  routines and changing
# the initialization  data in the lookup tables.  It should not
# be necessary to alter the formats of the tables.

CFLAGS =-O -wo
OBJ = disrel.o dismain.o distabs.o dishand.o disfp.o

all:	dis88

dis88:	$(OBJ)
	cc -i -o dis88 $(OBJ)
	install -S 5kw dis88

install:	/usr/bin/dis88

/usr/bin/dis88:	dis88
	install -cs -o bin dis88 $@

disrel.o:	disrel.c
dismain.o:	dismain.c dis.h
distabs.o:	distabs.c dis.h
dishand.o:	dishand.c dis.h
disfp.o:	disfp.c dis.h


clean:	
	rm -f *.bak *.o core dis88
