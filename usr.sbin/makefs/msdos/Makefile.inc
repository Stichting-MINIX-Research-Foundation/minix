#	$NetBSD: Makefile.inc,v 1.7 2016/02/06 10:40:58 mlelstv Exp $
#

MSDOS=	${NETBSDSRCDIR}/sys/fs/msdosfs
MSDOS_NEWFS=	${NETBSDSRCDIR}/sbin/newfs_msdos

.PATH:	${.CURDIR}/msdos ${MSDOS} ${MSDOS_NEWFS} ${NETBSDSRCDIR}/sys/dev

CPPFLAGS+= -DMSDOS_EI -I${MSDOS} -I${MSDOS_NEWFS}
.if !defined(HOSTPROGNAME)
CPPFLAGS+= -I${NETBSDSRCDIR}/sys
.endif

SRCS+= mkfs_msdos.c msdosfs_fat.c msdosfs_conv.c msdosfs_vfsops.c
SRCS+= msdosfs_lookup.c msdosfs_denode.c msdosfs_vnops.c msdosfs_unicode.c
SRCS+= clock_subr.c
