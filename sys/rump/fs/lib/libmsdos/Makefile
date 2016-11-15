#	$NetBSD: Makefile,v 1.2 2010/02/16 20:42:46 pooka Exp $
#

.PATH:  ${.CURDIR}/../../../../fs/msdosfs

LIB=	rumpfs_msdos

SRCS=	msdosfs_conv.c msdosfs_fat.c msdosfs_vfsops.c msdosfs_denode.c	\
	msdosfs_lookup.c msdosfs_vnops.c

.include <bsd.lib.mk>
.include <bsd.klinks.mk>
