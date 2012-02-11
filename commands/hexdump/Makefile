#	$NetBSD: Makefile,v 1.13 2009/04/14 22:15:21 lukem Exp $
#	from: @(#)Makefile	8.1 (Berkeley) 6/6/93

PROG=	hexdump
SRCS=	conv.c display.c hexdump.c hexsyntax.c odsyntax.c parse.c
MAN=	hexdump.1 #od.1

.ifndef HOSTPROG

LDADD+=        -lutil
DPADD+=        ${LIBUTIL}

#LINKS=	${BINDIR}/hexdump ${BINDIR}/od
.endif

.include <bsd.prog.mk>
