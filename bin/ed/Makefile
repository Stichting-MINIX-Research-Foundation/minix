#	$NetBSD: Makefile,v 1.37 2017/05/21 15:28:36 riastradh Exp $

.include <bsd.own.mk>

PROG=	ed
CPPFLAGS+=-DBACKWARDS
CPPFLAGS+=-DDES

SRCS=	buf.c cbc.c glbl.c io.c main.c re.c sub.c undo.c

LDADD+= -lcrypt
DPADD+= ${LIBCRYPT}

#LINKS=  ${BINDIR}/ed ${BINDIR}/red
#MLINKS= ed.1 red.1

.include <bsd.prog.mk>
