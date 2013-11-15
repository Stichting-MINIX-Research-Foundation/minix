#	$NetBSD: Makefile,v 1.2 2013/10/14 01:37:40 joerg Exp $

LIB=	lldReaderWriter

.include <bsd.init.mk>

.PATH: ${LLD_SRCDIR}/lib/ReaderWriter

SRCS+=	CoreLinkingContext.cpp \
	LinkerScript.cpp \
	Reader.cpp \
	ReaderLinkerScript.cpp \
	Writer.cpp

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
