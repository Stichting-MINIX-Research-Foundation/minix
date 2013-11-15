#	$NetBSD: Makefile,v 1.1 2013/09/29 12:12:00 joerg Exp $

LIB=	lldbHostNetBSD

.include <bsd.init.mk>

.PATH: ${LLDB_SRCDIR}/source/Host/freebsd

SRCS+=	Host.cpp

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
