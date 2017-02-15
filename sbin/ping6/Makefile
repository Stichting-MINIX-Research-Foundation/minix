# $NetBSD: Makefile,v 1.16 2015/09/09 10:06:05 ozaki-r Exp $

.include <bsd.own.mk>

USE_FORT?=	yes	# setuid
RUMPPRG=	ping6
MAN=		ping6.8

BINOWN=		root
BINMODE=	4555

CPPFLAGS+=	-DINET6
.if !defined(__MINIX)
CPPFLAGS+=	-DIPSEC
.endif # !defined(__MINIX)

.if !defined(__MINIX)
LDADD+=		-lipsec -lm
DPADD+=		${LIBIPSEC} ${LIBM}
.else
LDADD+=		-lm
DPADD+=		${LIBM}
.endif # defined(__MINIX)

.PATH:		${.CURDIR}/../../lib/libc/net
RUMPSRCS=	getaddrinfo.c getifaddrs.c getnameinfo.c
RUMPSRCS+=	if_indextoname.c if_nametoindex.c
.if (${MKRUMP} != "no")
CPPFLAGS+= 	-DRUMP_ACTION
.endif

.include <bsd.prog.mk>
