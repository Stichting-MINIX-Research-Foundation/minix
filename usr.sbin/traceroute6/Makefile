# $NetBSD: Makefile,v 1.9 2011/05/10 01:52:49 christos Exp $

USE_FORT?= yes	# network client

TRACEROUTE?= ${.CURDIR}/../traceroute

SRCS=	traceroute6.c as.c
PROG=	traceroute6
MAN=	traceroute6.8

BINMODE=4555
BINOWN=	root

.ifdef __MINIX
CPPFLAGS+=-DINET6
CPPFLAGS+=-DUSE_RFC3542
.else # !__MINIX
CPPFLAGS+=-DINET6 -DIPSEC
CPPFLAGS+=-DUSE_RFC3542
LDADD+=	-lipsec
DPADD+=	${LIBIPSEC}
.endif # !__MINIX

CPPFLAGS+=-DHAVE_POLL
CPPFLAGS+=-I${TRACEROUTE}

.PATH: ${TRACEROUTE}

.include <bsd.prog.mk>
