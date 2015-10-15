#	$NetBSD: Makefile,v 1.5 2014/06/01 19:51:02 mrg Exp $

.include <bsd.own.mk>
.include <bsd.init.mk>

.include "${.CURDIR}/../../arch/${GCC_MACHINE_ARCH}/defs.mk"

.cc: # disable .cc->NULL transform

INCS=		${G_tr1_impl_headers}
INCSDIR=	/usr/include/g++/tr1_impl

# Get default targets including <bsd.inc.mk>.
.include <bsd.prog.mk>

.PATH: ${DIST}/libstdc++-v3/include/tr1_impl
