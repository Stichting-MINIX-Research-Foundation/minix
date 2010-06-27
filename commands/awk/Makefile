.include <bsd.own.mk>

PROG=	awk
SRCS=	awkgram.y b.c lex.c lib.c main.c parse.c proctab.c run.c tran.c
YHEADER=	yes
QUIET_YACC=	yes

CPPFLAGS+= -I. -I${.CURDIR}

build-tools: maketab

proctab.c: maketab
	./maketab > proctab.c

maketab: awkgram.h maketab.c

CLEANFILES= maketab proctab.c

.include <bsd.prog.mk>
