#	$NetBSD: Makefile,v 1.7 2014/06/01 19:51:02 mrg Exp $

.include <bsd.own.mk>
.include <bsd.init.mk>

.include "${.CURDIR}/../../arch/${GCC_MACHINE_ARCH}/defs.mk"

.cc: # disable .cc->NULL transform

INCS=		${G_ext_headers}

INCSDIR=	/usr/include/g++/ext

.include "${.CURDIR}/../Makefile.includes"

.include <bsd.prog.mk>
# Get default targets including <bsd.inc.mk>.

.PATH: ${DIST}/libstdc++-v3/include/ext
.PATH: ${DIST}/libstdc++-v3/include
.PATH: ${DIST}/libstdc++-v3/config
.PATH: ${DIST}/libstdc++-v3/libsupc++
# for ext/hash_set
#.PATH: ${DIST}/libstdc++-v3/include/backward
