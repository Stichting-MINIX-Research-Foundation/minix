#	$NetBSD: Makefile,v 1.36 2009/07/26 01:58:20 dholland Exp $

.include <bsd.own.mk>

PROG=	ed
CPPFLAGS+=-DBACKWARDS

.if (${MKCRYPTO} != "no")
CPPFLAGS+=-DDES
.endif

SRCS=	buf.c cbc.c glbl.c io.c main.c re.c sub.c undo.c

LDADD+= -lcrypt
DPADD+= ${LIBCRYPT}

#LINKS=  ${BINDIR}/ed ${BINDIR}/red
#MLINKS= ed.1 red.1

.include <bsd.prog.mk>
