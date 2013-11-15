#	$NetBSD: Makefile,v 1.1 2013/02/27 21:25:10 joerg Exp $

LIB=	MCLinkerObject

.include <bsd.init.mk>

.PATH: ${MCLINKER_SRCDIR}/lib/Object

SRCS=	ObjectBuilder.cpp \
	ObjectLinker.cpp \
	SectionMap.cpp

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
