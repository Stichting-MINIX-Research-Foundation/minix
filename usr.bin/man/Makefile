#	$NetBSD: Makefile,v 1.11 2009/04/14 22:15:23 lukem Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/6/93

WARNS?=	2	# XXX -Wcast-qual issues

PROG=	man
SRCS=	man.c manconf.c
MAN=	man.1 man.conf.5

DPADD+=	${LIBUTIL}
LDADD+=	-lutil

.include <bsd.prog.mk>
