#	from: @(#)Makefile	8.1 (Berkeley) 6/6/93
#	$NetBSD: Makefile,v 1.23 2009/10/22 22:50:35 tsarna Exp $

.include <bsd.own.mk>

USE_FORT?= yes	# network server

PROG=	inetd
SRCS=	inetd.c
MAN=	inetd.8
MLINKS=	inetd.8 inetd.conf.5

CPPFLAGS+=-DLIBWRAP
# Use LIBWRAP_INTERNAL for libwrap checking of inetd's `internal' services.
#CPPFLAGS+=-DLIBWRAP_INTERNAL
LDADD+= -lwrap -lutil
DPADD+= ${LIBWRAP} ${LIBUTIL}

.if (${USE_INET6} != "no")
CPPFLAGS+=-DINET6
.endif

.if !defined(__MINIX)
CPPFLAGS+=-DIPSEC
SRCS+=	ipsec.c
LDADD+= -lipsec
DPADD+=	${LIBIPSEC}
.else # defined(__MINIX)
CPPFLAGS+=-DNO_RPC
.endif # defined(__MINIX)

.include <bsd.prog.mk>
