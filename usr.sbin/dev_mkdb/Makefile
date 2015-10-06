#	from: @(#)Makefile	8.1 (Berkeley) 6/6/93
#	$NetBSD: Makefile,v 1.9 2012/08/10 12:10:28 joerg Exp $

PROG=	dev_mkdb
MAN=	dev_mkdb.8

LDADD+=	-lutil
DPADD+=	${LIBUTIL}

.include <bsd.prog.mk>

LDFLAGS+=	-Wl,--no-fatal-warnings
