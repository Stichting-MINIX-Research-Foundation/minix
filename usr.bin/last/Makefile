#	$NetBSD: Makefile,v 1.7 2004/11/19 21:41:25 christos Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/6/93

PROG=	last
CPPFLAGS+=-DSUPPORT_UTMPX -DSUPPORT_UTMP

LDADD=-lutil
DPADD=${LIBUTIL}

.include <bsd.prog.mk>
