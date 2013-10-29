#	$NetBSD: Makefile,v 1.14 2011/08/14 13:45:34 christos Exp $
#	from: @(#)Makefile	8.1 (Berkeley) 6/6/93

PROG=	hexdump
SRCS=	conv.c display.c hexdump.c hexsyntax.c odsyntax.c parse.c
MAN=	hexdump.1 od.1

.ifndef HOSTPROG
LDADD+=-lutil
DPADD+=${LIBUTIL}

LINKS=	${BINDIR}/hexdump ${BINDIR}/od
.endif

COPTS.conv.c += -Wno-format-nonliteral
COPTS.display.c += -Wno-format-nonliteral

.include <bsd.prog.mk>
