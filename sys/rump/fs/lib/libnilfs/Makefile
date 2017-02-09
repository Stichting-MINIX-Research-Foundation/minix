#	$NetBSD: Makefile,v 1.3 2011/05/26 12:56:32 joerg Exp $
#

.PATH:  ${.CURDIR}/../../../../fs/nilfs

LIB=	rumpfs_nilfs

SRCS=	nilfs_subr.c nilfs_vfsops.c nilfs_vnops.c

CWARNFLAGS.clang+=	-Wno-self-assign

.include <bsd.lib.mk>
.include <bsd.klinks.mk>
