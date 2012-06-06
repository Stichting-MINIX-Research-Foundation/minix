#	$NetBSD: Makefile,v 1.2 2011/06/23 11:47:13 mrg Exp $

.include <bsd.own.mk>

.include "${.CURDIR}/../../arch/${MACHINE_ARCH}/defs.mk"

.cc: # disable .cc->NULL transform

DIST=		${NETBSDSRCDIR}/external/gpl3/gcc/dist

INCS=		${G_debug_headers}
INCSDIR=	/usr/include/g++/debug

.PATH: ${DIST}/libstdc++-v3/include/debug

# Get default targets including <bsd.inc.mk>.
.include <bsd.prog.mk>
