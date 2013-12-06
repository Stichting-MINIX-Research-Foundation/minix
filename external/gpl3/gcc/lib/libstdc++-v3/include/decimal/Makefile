#	$NetBSD: Makefile,v 1.2 2013/08/22 16:03:31 matt Exp $

.include <bsd.own.mk>
.include <bsd.init.mk>

.include "${.CURDIR}/../../arch/${GCC_MACHINE_ARCH}/defs.mk"

.cc: # disable .cc->NULL transform

DIST=		${NETBSDSRCDIR}/external/gpl3/gcc/dist

INCS=		${G_decimal_headers}
INCSDIR=	/usr/include/g++/decimal

.PATH: ${DIST}/libstdc++-v3/include/decimal

# Get default targets including <bsd.inc.mk>.
.include <bsd.prog.mk>
