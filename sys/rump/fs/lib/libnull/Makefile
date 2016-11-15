#	$NetBSD: Makefile,v 1.2 2011/05/26 12:56:32 joerg Exp $
#

.PATH:  ${.CURDIR}/../../../../miscfs/nullfs

LIB=	rumpfs_null

SRCS=	null_vfsops.c null_vnops.c

CWARNFLAGS.clang+=	-Wno-conversion

.include <bsd.lib.mk>
.include <bsd.klinks.mk>
