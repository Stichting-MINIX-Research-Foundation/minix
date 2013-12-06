#	$NetBSD: Makefile,v 1.8 2011/08/17 13:22:33 christos Exp $

PROG=	stat

.if !defined(HOSTPROG)
LINKS=	${BINDIR}/stat ${BINDIR}/readlink
MLINKS=	stat.1 readlink.1
.endif

COPTS.stat.c += -Wno-format-nonliteral

.include <bsd.prog.mk>
