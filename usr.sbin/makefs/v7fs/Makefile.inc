#	$NetBSD: Makefile.inc,v 1.5 2013/01/23 21:03:15 christos Exp $
#

V7FS=	${NETBSDSRCDIR}/sys/fs/v7fs
V7FS_NEWFS=	${NETBSDSRCDIR}/sbin/newfs_v7fs
FSCK=	${NETBSDSRCDIR}/sbin/fsck	# use progress meter.

.PATH:	${.CURDIR}/v7fs ${V7FS} ${V7FS_NEWFS} ${FSCK}

CPPFLAGS+= -DV7FS_EI -I${V7FS} -I${V7FS_NEWFS} -I${FSCK}

SRCS += v7fs_endian.c v7fs_superblock.c v7fs_superblock_util.c v7fs_inode.c \
v7fs_inode_util.c v7fs_datablock.c v7fs_dirent.c v7fs_io.c v7fs_file.c \
v7fs_file_util.c v7fs_io_user.c
SRCS += main.c		# newfs
.if !defined(HOSTPROG)
SRCS += progress.c	# progress bar (fsck)
.endif

SRCS += v7fs_estimate.c v7fs_populate.c
