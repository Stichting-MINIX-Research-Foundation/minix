#	$NetBSD: Makefile,v 1.2 2011/05/26 12:56:32 joerg Exp $
#

.PATH:  ${.CURDIR}/../../../../miscfs/umapfs

LIB=	rumpfs_umap

SRCS=	umap_subr.c umap_vfsops.c umap_vnops.c

CWARNFLAGS.clang+=	-Wno-conversion

.include <bsd.lib.mk>
.include <bsd.klinks.mk>
