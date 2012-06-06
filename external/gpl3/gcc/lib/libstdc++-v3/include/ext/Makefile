#	$NetBSD: Makefile,v 1.3 2011/06/28 04:07:04 mrg Exp $

.include <bsd.own.mk>

.include "${.CURDIR}/../../arch/${MACHINE_ARCH}/defs.mk"

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
