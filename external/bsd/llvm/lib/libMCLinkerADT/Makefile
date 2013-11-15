#	$NetBSD: Makefile,v 1.1 2013/02/27 21:25:09 joerg Exp $

LIB=	MCLinkerADT

.include <bsd.init.mk>

.PATH: ${MCLINKER_SRCDIR}/lib/ADT

SRCS=	StringEntry.cpp

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
