#	$NetBSD: Makefile,v 1.1 2015/01/29 20:41:37 joerg Exp $

LIB=	lldConfig

.include <bsd.init.mk>

.PATH: ${LLD_SRCDIR}/lib/Config

SRCS+=	Version.cpp

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
