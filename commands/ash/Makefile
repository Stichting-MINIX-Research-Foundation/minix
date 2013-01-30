# Makefile for ash.

.include <bsd.own.mk>

PROG=	sh
BINDIR=	/bin
MAN=

# Enable this line to disable command line editing
#EDIT=-DNO_HISTORY

# Enable this line if your system does not have a <paths.h>
#NO_PATHS_H=-DNO_PATHS_H

# Enable this if you don't want job control
NO_JOBS=-DJOBS=0
MKB_NO_JOBS=-j

SRCS=	alias.c arith.y arith_lex.l cd.c complete.c eval.c exec.c expand.c \
	histedit.c input.c jobs.c mail.c main.c memalloc.c miscbltin.c \
	mystring.c options.c output.c parser.c redir.c show.c \
	trap.c var.c setmode.c

.include "${.CURDIR}/bltin/Makefile.inc"

GENSRCS= builtins.c init.c nodes.c syntax.c operators.c signames.c
GENHDRS= builtins.h nodes.h syntax.h token.h operators.h signames.h

SRCS+= ${GENSRCS}

CLEANFILES+=${GENSRCS} ${GENHDRS}

DPADD+= ${LIBL} ${LIBEDIT}
LDADD+= -ll -ledit

CPPFLAGS+= -DSHELL -D__NBSD_LIBC
CPPFLAGS+=${EDIT} ${NO_PATHS_H} ${NO_JOBS}

CPPFLAGS+= -I. -I${.CURDIR}

# A. Generate C tools used to build ash
.for tool in init nodes signames syntax
${.OBJDIR}/mk${tool}: ${.CURDIR}/mk${tool}.c
	${HOST_CC} ${.ALLSRC} -o ${.TARGET}
.endfor

# B. Generates C sources from C tools
NODES_ARGS:= ${.CURDIR}/nodetypes ${.CURDIR}/nodes.c.pat
INIT_ARGS:= alias.c eval.c exec.c input.c jobs.c options.c parser.c \
            redir.c trap.c var.c

.for tool in nodes signames syntax
${tool}.c ${tool}.h: ${.OBJDIR}/mk${tool}
	${.OBJDIR}/mk${tool} ${${tool:tu}_ARGS}
.endfor

init.c: ${.OBJDIR}/mkinit \
        alias.c eval.c exec.c input.c jobs.c options.c parser.c \
        redir.c trap.c var.c
	${.OBJDIR}/mkinit ${.ALLSRC:S,^${.OBJDIR}/mkinit$,,}

# C. Generates C sources from shell scripts
token.h:
	${.CURDIR}/mktokens.sh

builtins.c builtins.h:
	${.CURDIR}/mkbuiltins.sh ${MKB_NO_JOBS} . ${.CURDIR}/shell.h ${.CURDIR}/builtins.def

operators.c operators.h:
	${.CURDIR}/bltin/mkexpr.sh ${.CURDIR}/bltin/unary_op ${.CURDIR}/bltin/binary_op

# D. Generates sources from yacc/lex
LFLAGS=	-8	# 8-bit lex scanner for arithmetic

YFLAGS:= -d
CLEANFILES+= arith.h arith.y.o

parser.c: token.h
y.tab.h: arith.y
arith.h: y.tab.h
arith_lex.l: arith.h

# Explicit dependencies to ensure creation when needed
# LSC FIXME Under MINIX, the build system curiously needs more help.
# is it because of the missing order tools?
expand.c: arith.h
trap.c: signames.h
cd.c complete.c eval.c exec.c expand.c jobs.c main.c options.c parser.c redir.c show.c trap.c var.c: nodes.h
eval.c exec.c expand.c input.c input.h jobs.c mystring.c output.c parser.c trap.c var.c: syntax.h
cd.c eval.c exec.c histedit.cjobs.c main.c miscbltin.c options.c trap.c var.c: builtins.h

# LSC: Seems that this file is implicitly taken into account by NetBSD's make, 
# still seems to be ignored / not found currently.
# It's a sad story, as it has default rules to manage yacc / lex files. So for
# a happy ending here it is explicitly included:
.include <sys.mk>

.include <bsd.prog.mk>

