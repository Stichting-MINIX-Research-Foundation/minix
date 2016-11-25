#	$NetBSD: Makefile,v 1.9 2015/09/14 15:09:35 pooka Exp $
#

LIB=	rumpdev_ugenhc

.if ${MACHINE} == "i386" || ${MACHINE} == "amd64"
IOCONF= UGENHC.ioconf

SRCS=	ugenhc.c ugenhc_at_mainbus.c ugenhc_dma.c

CPPFLAGS+=	-I${RUMPTOP}/librump/rumpkern

RUMPCOMP_USER_SRCS=	ugenhc_user.c
.else
.PATH:	${.CURDIR}/../libusb
SRCS=	dummy.c
.endif

.include <bsd.lib.mk>
.include <bsd.klinks.mk>
