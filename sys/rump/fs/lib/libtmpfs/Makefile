#	$NetBSD: Makefile,v 1.5 2012/05/09 00:16:07 riastradh Exp $
#

.PATH:	${.CURDIR}/../../../../fs/tmpfs

LIB=	rumpfs_tmpfs

SRCS=	tmpfs_fifoops.c tmpfs_specops.c tmpfs_vfsops.c tmpfs_mem.c \
	tmpfs_rename.c tmpfs_subr.c tmpfs_vnops.c

.include <bsd.lib.mk>
.include <bsd.klinks.mk>
