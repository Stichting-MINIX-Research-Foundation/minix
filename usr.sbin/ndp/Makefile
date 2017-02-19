# $NetBSD: Makefile,v 1.11 2015/09/01 09:54:34 ozaki-r Exp $

.include <bsd.own.mk>

USE_FORT?=	yes	# network client

TCPDUMP=	${NETBSDSRCDIR}/external/bsd/tcpdump/dist
.PATH:		${TCPDUMP}

RUMPPRG=	ndp
SRCS=		ndp.c gmt2local.c
MAN=		ndp.8

CPPFLAGS+=	-DINET6
CPPFLAGS+=	-I. -I${TCPDUMP}

.PATH:		${.CURDIR}/../../lib/libc/net
RUMPSRCS=	getifaddrs.c getnameinfo.c if_indextoname.c if_nametoindex.c
.if (${MKRUMP} != "no")
CPPFLAGS+= 	-DRUMP_ACTION
.endif

.include <bsd.prog.mk>
