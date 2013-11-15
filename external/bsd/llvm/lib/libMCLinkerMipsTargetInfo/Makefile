#	$NetBSD: Makefile,v 1.1 2013/02/27 21:25:10 joerg Exp $

LIB=	MCLinkerMipsTargetInfo

.include <bsd.init.mk>

.PATH: ${MCLINKER_SRCDIR}/lib/Target/Mips/TargetInfo

SRCS=	MipsTargetInfo.cpp

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
