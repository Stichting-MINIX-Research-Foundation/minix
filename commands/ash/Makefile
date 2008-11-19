# Makefile for ash.

SRCS=	alias.c builtins.c cd.c error.c eval.c exec.c expand.c histedit.c \
	input.c \
	jobs.c mail.c main.c memalloc.c miscbltin.c mystring.c nodes.c \
	options.c parser.c redir.c setmode.c show.c signames.c syntax.c \
	trap.c \
	output.c var.c

OBJS=	alias.o builtins.o cd.o error.o eval.o exec.o expand.o histedit.o \
	input.o \
	jobs.o mail.o main.o memalloc.o miscbltin.o mystring.o nodes.o \
	options.o parser.o redir.o setmode.o show.o signames.o syntax.o \
	trap.o \
	output.o var.o init.o \
	bltin/echo.o bltin/expr.o bltin/operators.o bltin/regexp.o \
	arith.o arith_lex.o

LEX=flex
YACC=/usr/bin/yacc
.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

# Enable this line to disable command line editing
#EDIT=-DNO_HISTORY
# Enable this line to use the editline library instead of libedit
EDIT=-DEDITLINE
EDITLIB=-ledit

FLEXLIB=-lfl

# Enable this line if your system does not have a <paths.h>
NO_PATHS_H=-DNO_PATHS_H 

# Enable this if you don't want job control
NO_JOBS=-DJOBS=0
MKB_NO_JOBS=-j

CPPFLAGS= -DSHELL -I. -D_MINIX $(EDIT) $(NO_PATHS_H) $(NO_JOBS)
CFLAGS=	$(OPT) $(CPPFLAGS)
LIBS=	$(EDITLIB) $(FLEXLIB)

CLEANFILES= $(OBJS) \
	arith.c arith_y.h arith_lex.c builtins.c builtins.h init.c \
	mkinit mknodes mksignames mksyntax \
	nodes.c nodes.h signames.c signames.h syntax.c syntax.h token.h \
	bltin/operators.h bltin/operators.c

all:	sh

sh:	$(OBJS)
	$(CC) $(CFLAGS) -fnone -o sh $(OBJS) $(LIBS)
	install -S 136k sh

install:	/usr/bin/ash /usr/bin/sh /bin/sh /bin/bigsh

/usr/bin/ash:	sh
	install -cs -o bin $< $@

/usr/bin/sh:	/usr/bin/ash
	install -l $< $@

/bin/sh:	/usr/bin/ash
	install -lcs $< $@

/bin/bigsh:	/usr/bin/ash
	install -S 6600k -lcs $< $@

clean:
	rm -f $(CLEANFILES) sh core

parser.o: token.def

token.def: mktokens
	sh mktokens

arith.c:	arith.y
	$(YACC) -d $<
	mv y.tab.c $@
	mv y.tab.h arith_y.h

arith_lex.c:	arith_lex.l

builtins.c builtins.h: builtins.def shell.h
	sh mkbuiltins $(MKB_NO_JOBS) . shell.h builtins.def

init.c: mkinit $(SRCS)
	./mkinit $(SRCS)

mkinit: mkinit.c
	$(CC) $(CFLAGS) mkinit.c -o $@

nodes.c nodes.h: mknodes nodetypes nodes.c.pat
	./mknodes nodetypes nodes.c.pat

mknodes: mknodes.c
	$(CC) $(CFLAGS) mknodes.c -o $@

signames.c signames.h: mksignames
	./mksignames

mksignames: mksignames.c
	$(CC) $(CFLAGS) mksignames.c -o $@

syntax.c syntax.h: mksyntax
	./mksyntax

mksyntax: mksyntax.c parser.h
	$(CC) $(CFLAGS) mksyntax.c -o $@

bltin/operators.h:	bltin/mkexpr bltin/unary_op bltin/binary_op
	cd bltin && sh mkexpr unary_op binary_op

# Dependencies you say?  This will have to do.
$(OBJS): error.h eval.h exec.h expand.h init.h input.h \
	jobs.h machdep.h mail.h main.h memalloc.h mystring.h options.h \
	output.h parser.h redir.h shell.h trap.h var.h \
	builtins.h nodes.h signames.h syntax.h

bltin/expr.o bltin/operators.o:	bltin/operators.h

#
# $PchId: Makefile,v 1.4 2006/05/22 12:40:46 philip Exp $
