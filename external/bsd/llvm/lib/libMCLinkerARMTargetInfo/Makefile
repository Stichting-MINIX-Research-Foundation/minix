#	$NetBSD: Makefile,v 1.1 2013/02/27 21:25:09 joerg Exp $

LIB=	MCLinkerARMTargetInfo

.include <bsd.init.mk>

.PATH: ${MCLINKER_SRCDIR}/lib/Target/ARM/TargetInfo

SRCS=	ARMTargetInfo.cpp

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
