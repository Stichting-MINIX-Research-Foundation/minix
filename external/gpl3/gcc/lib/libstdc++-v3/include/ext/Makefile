#	$NetBSD: Makefile,v 1.4 2013/08/22 16:03:31 matt Exp $

.include <bsd.own.mk>
.include <bsd.init.mk>

.include "${.CURDIR}/../../arch/${GCC_MACHINE_ARCH}/defs.mk"

.cc: # disable .cc->NULL transform

DIST=		${NETBSDSRCDIR}/external/gpl3/gcc/dist

INCS=		${G_ext_headers}
INCSDIR=	/usr/include/g++/ext

.PATH: ${DIST}/libstdc++-v3/include/ext
# for ext/hash_set
.PATH: ${DIST}/libstdc++-v3/include/backward

# Get default targets including <bsd.inc.mk>.
.include <bsd.prog.mk>

# XXX Not yet
#SUBDIR=	pb_assoc

#.include <bsd.subdir.mk>
