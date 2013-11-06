#	$NetBSD: Makefile,v 1.6 2011/08/16 10:37:21 christos Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/6/93

PROG=	jot
DPADD=	${LIBMATH}
LDADD=	-lm

COPTS.jot.c += -Wno-format-nonliteral

.include <bsd.prog.mk>
