#	$NetBSD: Makefile,v 1.9 2011/08/28 08:24:42 christos Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/6/93

.include <bsd.own.mk>

CPPFLAGS+=	-DNOT_CSH
CPPFLAGS+=	-I. -I${NETBSDSRCDIR}/bin
SRCS=	time.c xtime.c
PROG=	time

.PATH: ${NETBSDSRCDIR}/bin/csh

LDADD+=-lutil
DPADD+=${LIBUTIL}

.include <bsd.prog.mk>
