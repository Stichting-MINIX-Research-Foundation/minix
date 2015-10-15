#	$NetBSD: Makefile,v 1.3 2014/08/10 17:35:07 joerg Exp $

LIB=	lldPasses

.include <bsd.init.mk>

.PATH: ${LLD_SRCDIR}/lib/Passes

SRCS+=	LayoutPass.cpp \
	RoundTripNativePass.cpp \
	RoundTripYAMLPass.cpp

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
