#	$NetBSD: Makefile,v 1.16 2011/07/24 08:55:30 uch Exp $

.include <bsd.own.mk>

.if !defined(__MINIX)
SUBDIR=	mount_9p mount_psshfs mount_sysctlfs

SUBDIR+=rump_cd9660 rump_efs rump_ext2fs rump_ffs rump_hfs rump_lfs
SUBDIR+=rump_msdos rump_nfs rump_ntfs rump_syspuffs rump_sysvbfs
SUBDIR+=rump_tmpfs rump_udf rump_v7fs

.if ${MKCRYPTO} != "no"
SUBDIR+=rump_smbfs
.endif
.else
SUBDIR=	rump_sysvbfs
.endif

#
# The following are not built by default
#

# userspace fdesc server cannot grope the fd info from the kernel,
# and is therefore not useful for the average user
#SUBDIR+=rump_fdesc

# ditto for kernfs
#SUBDIR+=rump_kernfs

# rump_mqmfs is just another ffs, useful mainly for debugging
#SUBDIR+=rump_nqmfs

# rump_au-naturel presents lets you access '/' for a rump kernel.
# mostly for debugging.
#SUBDIR+=rump_au-naturel

.include <bsd.subdir.mk>
