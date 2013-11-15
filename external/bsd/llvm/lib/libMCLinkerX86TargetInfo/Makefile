#	$NetBSD: Makefile,v 1.1 2013/02/27 21:25:11 joerg Exp $

LIB=	MCLinkerX86TargetInfo

.include <bsd.init.mk>

.PATH: ${MCLINKER_SRCDIR}/lib/Target/X86/TargetInfo

SRCS=	X86TargetInfo.cpp

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
