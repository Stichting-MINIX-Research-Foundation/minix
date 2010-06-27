# Makefile for ash.

.include <bsd.own.mk>

YHEADER=1
PROG=	sh

SHSRCS=	alias.c arith.y arith_lex.l cd.c echo.c error.c eval.c exec.c expand.c \
	histedit.c input.c jobs.c mail.c main.c memalloc.c miscbltin.c \
	mystring.c options.c output.c parser.c redir.c show.c \
	trap.c var.c setmode.c expr.c regexp.c
GENSRCS= builtins.c init.c nodes.c syntax.c operators.c signames.c
GENHDRS= builtins.h nodes.h syntax.h token.h operators.h signames.h
SRCS= ${SHSRCS} ${GENSRCS}
DPSRCS+=${GENHDRS}
BINDIR=	/bin
MAN=

DPADD+= ${LIBL} ${LIBEDIT}
LDADD+= -ll -ledit

LFLAGS=	-8	# 8-bit lex scanner for arithmetic
YFLAGS=	-d

# The .depend file can get references to these temporary files
.OPTIONAL: lex.yy.c y.tab.c

# Enable this line to disable command line editing
#EDIT=-DNO_HISTORY
# Enable this line to use the editline library instead of libedit
EDIT=-DEDITLINE

# Enable this line if your system does not have a <paths.h>
NO_PATHS_H=-DNO_PATHS_H 

# Enable this if you don't want job control
NO_JOBS=-DJOBS=0
MKB_NO_JOBS=-j

CPPFLAGS+=-DSHELL -I. -I${.CURDIR}
CPPFLAGS+=${EDIT} ${NO_PATHS_H} ${NO_JOBS}

.PATH:	${.CURDIR}/bltin

CLEANFILES+= mkinit mkinit.o mknodes mknodes.o \
	mksyntax mksyntax.o mksignames mksignames.o
CLEANFILES+= ${GENSRCS} ${GENHDRS} y.tab.h

build-tools: mkinit mknodes mksyntax mksignames

.ORDER: builtins.c builtins.h
builtins.c builtins.h: mkbuiltins builtins.def shell.h
	cd ${.CURDIR}; sh mkbuiltins ${MKB_NO_JOBS} ${.OBJDIR} shell.h builtins.def

init.c: mkinit alias.c eval.c exec.c input.c jobs.c options.c parser.c \
	redir.c trap.c var.c
	./mkinit ${.ALLSRC:S/^mkinit$//}

mkinit: mkinit.o
mknodes: mknodes.o
mksyntax: mksyntax.o
mksignames: mksignames.o

.ORDER: nodes.c nodes.h
nodes.c nodes.h: mknodes nodetypes nodes.c.pat
	./mknodes ${.CURDIR}/nodetypes ${.CURDIR}/nodes.c.pat

.ORDER: syntax.c syntax.h
syntax.c syntax.h: mksyntax
	./mksyntax

token.h: mktokens
	sh ${.CURDIR}/mktokens

.ORDER: signames.c signames.h
signames.c signames.h: mksignames
	./mksignames

.ORDER: operators.c operators.h
operators.c operators.h: mkexpr unary_op binary_op
	sh ${.CURDIR}/bltin/mkexpr ${.CURDIR}/bltin/unary_op ${.CURDIR}/bltin/binary_op

arith.h: arith.c
arith.c: arith.y

.include <bsd.prog.mk>
