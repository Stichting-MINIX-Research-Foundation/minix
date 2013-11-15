#	$NetBSD: Makefile,v 1.1 2013/09/29 12:12:00 joerg Exp $

LIB=	lldbLog

.include <bsd.init.mk>

.PATH: ${LLDB_SRCDIR}/source

SRCS+=	lldb.cpp \
	lldb-log.cpp

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
