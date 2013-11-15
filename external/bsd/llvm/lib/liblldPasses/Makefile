#	$NetBSD: Makefile,v 1.2 2013/10/30 20:27:39 joerg Exp $

LIB=	lldPasses

.include <bsd.init.mk>

.PATH: ${LLD_SRCDIR}/lib/Passes

SRCS+=	GOTPass.cpp \
	LayoutPass.cpp \
	RoundTripNativePass.cpp \
	RoundTripYAMLPass.cpp \
	StubsPass.cpp

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
