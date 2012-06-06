#	$NetBSD: Makefile,v 1.2 2011/06/23 11:47:12 mrg Exp $

.include <bsd.own.mk>

.include "${.CURDIR}/../../arch/${MACHINE_ARCH}/defs.mk"

.cc: # disable .cc->NULL transform

DIST=		${NETBSDSRCDIR}/external/gpl3/gcc/dist

INCS=		${G_backward_headers}
INCSDIR=	/usr/include/g++/backward

.PATH: ${DIST}/libstdc++-v3/include/backward

# Get default targets including <bsd.inc.mk>.
.include <bsd.prog.mk>
