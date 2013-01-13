#	$NetBSD: Makefile,v 1.8 2008/06/25 03:33:39 matt Exp $

SUBDIR=	include .WAIT include/arm26 include/arm32

.ifndef __MINIX
# install footbridge headers.
SUBDIR+= footbridge
.endif

.include <bsd.kinc.mk>
