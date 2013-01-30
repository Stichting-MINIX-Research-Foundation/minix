#	$NetBSD: Makefile,v 1.14 2006/12/14 20:09:36 he Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/2/93

PROG=	ls
SRCS=	cmp.c ls.c main.c print.c util.c

LDADD+=	-lutil
DPADD+=	${LIBUTIL}

.include <bsd.prog.mk>
