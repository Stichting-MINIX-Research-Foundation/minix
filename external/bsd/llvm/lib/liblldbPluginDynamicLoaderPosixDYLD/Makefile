#	$NetBSD: Makefile,v 1.1 2013/09/29 12:12:01 joerg Exp $

LIB=	lldbPluginDynamicLoaderPosixDYLD

.include <bsd.init.mk>

.PATH: ${LLDB_SRCDIR}/source/Plugins/DynamicLoader/POSIX-DYLD

SRCS+=	AuxVector.cpp \
	DYLDRendezvous.cpp \
	DynamicLoaderPOSIXDYLD.cpp

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
