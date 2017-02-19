# $NetBSD: Makefile,v 1.17 2012/08/10 12:10:30 joerg Exp $

WARNS?=	4

.include <bsd.own.mk>

USE_FORT?= yes	# network server

PROG=	rtadvd
SRCS=	rtadvd.c rrenum.c advcap.c if.c config.c timer.c dump.c

CPPFLAGS+=-DINET6
MAN=	rtadvd.8 rtadvd.conf.5
LDADD+=	-lutil
DPADD+=	${LIBUTIL}

.if ${MKSHARE} != "no"
FILESDIR=	/usr/share/examples/rtadvd
FILES=	rtadvd.conf
.endif

.include <bsd.prog.mk>

.if defined(HAVE_GCC) || defined(HAVE_LLVM)
COPTS.dump.c=-fno-strict-aliasing
.endif
