#	$NetBSD: Makefile,v 1.29 2015/09/14 05:12:52 ozaki-r Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/5/93

.include <bsd.own.mk>

RUMPPRG=route
MAN=	route.8
SRCS=	route.c show.c keywords.c rtutil.c

.PATH:		${.CURDIR}/../../lib/libc/net
RUMPSRCS=	getaddrinfo.c getifaddrs.c getnameinfo.c
RUMPSRCS+=	if_indextoname.c if_nametoindex.c
.if (${MKRUMP} != "no")
CPPFLAGS+= -DRUMP_ACTION
.endif

.if (${USE_INET6} != "no")
CPPFLAGS+=-DINET6
.endif

# The Makefile over in ../../distrib/utils/x_route
# would like keywords.[ch] to always exist here, so
# they are now checked in as sources.
#
# CPPFLAGS+=-I.
# CLEANFILES+= keywords.c keywords.h
# keywords.c keywords.h : keywords.sh
# 	${HOST_SH} keywords.sh

.include <bsd.prog.mk>
