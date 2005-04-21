# Makefile for ash.

SRCS=	builtins.c cd.c dirent.c error.c eval.c exec.c expand.c input.c \
	jobs.c mail.c main.c memalloc.c miscbltin.c mystring.c nodes.c \
	options.c parser.c redir.c show.c signames.c syntax.c trap.c \
	output.c var.c

OBJS=	builtins.o cd.o dirent.o error.o eval.o exec.o expand.o input.o \
	jobs.o mail.o main.o memalloc.o miscbltin.o mystring.o nodes.o \
	options.o parser.o redir.o show.o signames.o syntax.o trap.o \
	output.o var.o init.o \
	bltin/echo.o bltin/expr.o bltin/operators.o bltin/regexp.o

#
# Set READLINE in shell.h and add -ledit to LIBS if you want to use the
# editline package by Simmule Turner and Rich Salz.  (The big, bloated
# and GPL contaminated FSF readline should work too.)
#
CPPFLAGS= -DSHELL -I. -D_MINIX -D_POSIX_SOURCE
CFLAGS=	-wo -i $(CPPFLAGS)
LIBS=	-ledit

CLEANFILES= $(OBJS) \
	builtins.c builtins.h init.c mkinit mknodes mksignames mksyntax \
	nodes.c nodes.h signames.c signames.h syntax.c syntax.h token.def \
	bltin/operators.h bltin/operators.c

all:	sh

sh:	$(OBJS)
	$(CC) $(CFLAGS) -o sh $(OBJS) $(LIBS)
	install -S 12kw sh

install:	/usr/bin/ash /usr/bin/sh /bin/sh

/usr/bin/ash:	sh
	install -cs -o bin $? $@

/usr/bin/sh:	/usr/bin/ash
	install -l $? $@

/bin/sh:	/usr/bin/ash
	install -lcs $? $@

clean:
	rm -f $(CLEANFILES) sh core

parser.o: token.def

token.def: mktokens
	sh mktokens

builtins.c builtins.h: builtins.table shell.h
	sh mkbuiltins shell.h builtins.table

init.o: mkinit $(SRCS)
	./mkinit '$(CC) -c $(CFLAGS) init.c' $(SRCS)

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

bltin/operators.h:	bltin/mkexpr bltin/binary_op bltin/unary_op
	cd bltin && sh mkexpr

bltin/echo.o:	bltin/echo.c
	cd bltin && $(CC) -I.. $(CFLAGS) -c echo.c

bltin/expr.o:	bltin/expr.c
	cd bltin && $(CC) -I.. $(CFLAGS) -c expr.c

bltin/operators.o:	bltin/operators.c
	cd bltin && $(CC) -I.. $(CFLAGS) -c operators.c

bltin/regexp.o:	bltin/regexp.c
	cd bltin && $(CC) -I.. $(CFLAGS) -c regexp.c

# Dependencies you say?  This will have to do.
$(OBJS): error.h eval.h exec.h expand.h init.h input.h \
	jobs.h machdep.h mail.h main.h memalloc.h mystring.h options.h \
	output.h parser.h redir.h shell.h trap.h var.h \
	builtins.h nodes.h signames.h syntax.h

bltin/expr.o bltin/operators.o:	bltin/operators.h
