#	$NetBSD: Makefile,v 1.8 2002/09/19 11:29:54 lukem Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/6/93

.include <bsd.own.mk>

MDIST=	${NETBSDSRCDIR}/usr.bin/man

PROG=	whatis
SRCS=	whatis.c manconf.c
.PATH:	${MDIST}
CPPFLAGS+=-I${MDIST}

.include <bsd.prog.mk>
