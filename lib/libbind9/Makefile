#	$NetBSD: Makefile,v 1.1 2007/11/04 23:28:17 tls Exp $

LIB=bind9

.include <bsd.own.mk>

.include "${NETBSDSRCDIR}/usr.sbin/bind/Makefile.inc"

DIST=	${IDIST}/lib/bind9
.include "${DIST}/api"

.PATH.c:	${DIST}

SRCS=	check.c getaddresses.c version.c

.include <bsd.lib.mk>
