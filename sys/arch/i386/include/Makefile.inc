# $NetBSD: Makefile.inc,v 1.3 2014/10/19 15:41:05 joerg Exp $

CFLAGS+=	-msoft-float
CFLAGS+=	-mno-mmx -mno-sse -mno-avx

# this should really be !(RUMPKERNEL && PIC)
.if !defined(RUMPKERNEL)
CFLAGS+=	-fno-omit-frame-pointer
.endif

USE_SSP?=	yes
