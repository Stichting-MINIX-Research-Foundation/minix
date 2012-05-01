.include <bsd.own.mk>

PROG=	awk
SRCS=	awkgram.y b.c lex.c lib.c main.c parse.c proctab.c run.c tran.c
LDADD= -lm
DEPEND+= ${LIBM}
YHEADER=	yes
QUIET_YACC=	yes

CPPFLAGS+= -I. -I${.CURDIR}

build-tools: maketab

proctab.c: maketab
	./maketab > proctab.c

maketab.lo: awkgram.h


maketab: maketab.lo
	${HOST_LINK.c} maketab.lo -o $@

CLEANFILES= maketab proctab.c maketab.lo

.include <bsd.prog.mk>
