#	$NetBSD: Makefile,v 1.10 2007/10/05 07:38:52 lukem Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/6/93

PROG=	man
SRCS=	man.c manconf.c
MAN=	man.1 man.conf.5

DPADD+=	${LIBUTIL}
LDADD+=	-lutil

FILES=man.conf
FILESDIR=/etc

.include <bsd.prog.mk>
