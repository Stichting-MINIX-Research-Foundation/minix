#	$NetBSD: Makefile,v 1.1 2014/08/10 17:35:07 joerg Exp $

LIB=	lldbPluginDynamicLoaderHexagonDYLD

.include <bsd.init.mk>

.PATH: ${LLDB_SRCDIR}/source/Plugins/DynamicLoader/Hexagon-DYLD

SRCS+=	DynamicLoaderHexagonDYLD.cpp \
	HexagonDYLDRendezvous.cpp

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
