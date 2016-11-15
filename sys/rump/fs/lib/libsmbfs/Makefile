#	$NetBSD: Makefile,v 1.2 2010/02/16 20:42:46 pooka Exp $
#

.PATH:  ${.CURDIR}/../../../../fs/smbfs

LIB=	rumpfs_smbfs

SRCS=	smbfs_io.c smbfs_kq.c smbfs_node.c smbfs_smb.c smbfs_subr.c	\
	smbfs_vfsops.c smbfs_vnops.c

.include <bsd.lib.mk>
.include <bsd.klinks.mk>
