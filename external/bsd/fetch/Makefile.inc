# $NetBSD: Makefile.inc,v 1.1 2008/09/30 19:09:00 joerg Exp $

.include <bsd.own.mk>

LIBFETCHDIR=	${NETBSDSRCDIR}/external/bsd/fetch/dist/libfetch

USE_FORT?=	yes # complex string handling

WARNS?=	4
