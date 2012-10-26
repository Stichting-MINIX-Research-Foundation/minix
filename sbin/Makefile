#	$NetBSD: Makefile,v 1.123 2011/11/24 15:54:54 ahoka Exp $
#	@(#)Makefile	8.5 (Berkeley) 3/31/94

# Not ported: XNSrouted enpload scsiformat startslip
# Missing: icheck ncheck

.include <bsd.own.mk>

SUBDIR= \
	\
	fsck \
	mknod

# support for various file systems
SUBDIR+= newfs_ext2fs fsck_ext2fs
.include <bsd.subdir.mk>
