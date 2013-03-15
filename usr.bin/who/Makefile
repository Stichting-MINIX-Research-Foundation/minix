#	$NetBSD: Makefile,v 1.9 2009/04/14 22:15:29 lukem Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/6/93

PROG=	who
SRCS=	who.c utmpentry.c
CPPFLAGS+=	-DSUPPORT_UTMPX -DSUPPORT_UTMP

.include <bsd.prog.mk>
