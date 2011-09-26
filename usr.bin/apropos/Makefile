#	$NetBSD: Makefile,v 1.13 2009/04/14 22:15:17 lukem Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/6/93

.include <bsd.own.mk>

MDIST=	${NETBSDSRCDIR}/usr.bin/man

PROG=	apropos
SRCS=	apropos.c manconf.c

.PATH:	${MDIST}
CPPFLAGS+=-I${MDIST}

.include <bsd.prog.mk>
