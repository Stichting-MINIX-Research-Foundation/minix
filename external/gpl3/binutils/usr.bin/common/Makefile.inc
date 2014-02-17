#	$NetBSD: Makefile.inc,v 1.3 2013/08/06 05:35:57 matt Exp $

.if !defined(__COMMON_MAKEFILE_INC_INCLUDED__)
__COMMON_MAKEFILE_INC_INCLUDED__=1

BINUTILS_MACHINE_ARCH=${MACHINE_ARCH:C/armv[4-7]/arm/}
DIST=		${NETBSDSRCDIR}/external/gpl3/binutils/dist
BFDSUBDIR=	lib

.endif
