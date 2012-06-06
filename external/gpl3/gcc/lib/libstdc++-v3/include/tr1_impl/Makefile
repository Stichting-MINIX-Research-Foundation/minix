#	$NetBSD: Makefile,v 1.1 2011/07/01 01:21:39 mrg Exp $

.include <bsd.own.mk>

.include "${.CURDIR}/../../arch/${MACHINE_ARCH}/defs.mk"

.cc: # disable .cc->NULL transform

DIST=		${NETBSDSRCDIR}/external/gpl3/gcc/dist

INCS=		${G_tr1_impl_headers}
INCSDIR=	/usr/include/g++/tr1_impl

.PATH: ${DIST}/libstdc++-v3/include/tr1_impl

# Get default targets including <bsd.inc.mk>.
.include <bsd.prog.mk>
