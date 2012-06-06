#	$NetBSD: Makefile,v 1.1 2011/07/01 01:21:38 mrg Exp $

.include <bsd.own.mk>

.include "${.CURDIR}/../../arch/${MACHINE_ARCH}/defs.mk"

.cc: # disable .cc->NULL transform

DIST=		${NETBSDSRCDIR}/external/gpl3/gcc/dist

INCS=		${G_decimal_headers}
INCSDIR=	/usr/include/g++/decimal

.PATH: ${DIST}/libstdc++-v3/include/decimal

# Get default targets including <bsd.inc.mk>.
.include <bsd.prog.mk>
