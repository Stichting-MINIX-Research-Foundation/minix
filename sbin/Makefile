#	$NetBSD: Makefile,v 1.125 2013/10/28 19:05:21 mbalmer Exp $
#	@(#)Makefile	8.5 (Berkeley) 3/31/94

# Not ported: XNSrouted enpload scsiformat startslip
# Missing: icheck ncheck

.include <bsd.own.mk>

SUBDIR= \
	chown \
	fsck init \
	mknod nologin \
	ping \
	reboot \
	shutdown \

# support for various file systems
SUBDIR+= newfs_ext2fs fsck_ext2fs
.if !defined(__MINIX)
SUBDIR+= newfs fsck_ffs fsdb dump restore clri tunefs resize_ffs
SUBDIR+= newfs_lfs fsck_lfs dump_lfs resize_lfs
SUBDIR+= newfs_msdos fsck_msdos
SUBDIR+= newfs_sysvbfs
SUBDIR+= newfs_udf
SUBDIR+= newfs_v7fs fsck_v7fs
SUBDIR+= mount_ados
SUBDIR+= mount_cd9660
SUBDIR+= mount_chfs
SUBDIR+= mount_efs
SUBDIR+= mount_ext2fs
SUBDIR+= mount_fdesc
SUBDIR+= mount_filecore
SUBDIR+= mount_ffs
SUBDIR+= mount_hfs
SUBDIR+= mount_kernfs
SUBDIR+= mount_lfs
SUBDIR+= mount_msdos
SUBDIR+= mount_udf
SUBDIR+= mount_nfs
SUBDIR+= mount_nilfs
SUBDIR+= mount_ntfs
SUBDIR+= mount_null
SUBDIR+= mount_overlay
SUBDIR+= mount_portal
SUBDIR+= mount_procfs
SUBDIR+= mount_ptyfs
SUBDIR+= mount_puffs
SUBDIR+= mount_smbfs
SUBDIR+= mount_sysvbfs
SUBDIR+= mount_tmpfs
SUBDIR+= mount_umap
SUBDIR+= mount_union
SUBDIR+= mount_v7fs

.if (${MKCRYPTO} != "no")
SUBDIR+= cgdconfig
.endif

.if (${USE_INET6} != "no")
SUBDIR+= ping6 rtsol
.endif

.if (${MKISCSI} != "no")
SUBDIR+= iscsictl iscsid
.endif

# IPsec
SUBDIR+= setkey

.endif # !defined(__MINIX)
.include <bsd.subdir.mk>
