#	$NetBSD: Makefile,v 1.4 2011/08/16 12:05:52 christos Exp $
#	from: @(#)Makefile	8.1 (Berkeley) 6/6/93

PROG=	pr
SRCS=	pr.c egetopt.c

DPADD+=	${LIBUTIL}
LDADD+=	-lutil

COPTS.pr.c += -Wno-format-nonliteral

.include <bsd.prog.mk>
