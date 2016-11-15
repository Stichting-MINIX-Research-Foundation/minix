#	$NetBSD: Makefile,v 1.2 2011/05/26 12:56:32 joerg Exp $
#

.PATH:  ${.CURDIR}/../../../../miscfs/genfs

LIB=	rumpvfs_layerfs

SRCS=	layer_subr.c layer_vfsops.c layer_vnops.c

CWARNFLAGS.clang+=	-Wno-conversion

.include <bsd.lib.mk>
.include <bsd.klinks.mk>
