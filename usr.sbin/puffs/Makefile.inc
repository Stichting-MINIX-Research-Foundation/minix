#	$NetBSD: Makefile.inc,v 1.14 2014/01/16 01:15:34 christos Exp $

WARNS?=	4

.include <bsd.own.mk>

USE_FORT?= yes	# network client

.if exists(${.CURDIR}/../../Makefile.inc)
.include "${.CURDIR}/../../Makefile.inc"
.endif

.ifdef ISRUMP
.ifdef MOUNTNAME
PROG=		rump_${MOUNTNAME}
MAN=		rump_${MOUNTNAME}.8

MOUNTDIR=	${.CURDIR}/../../../sbin
MOUNT=		${MOUNTDIR}/mount

CPPFLAGS+=	-I${MOUNTDIR}/mount_${MOUNTNAME} -I${MOUNT} -DMOUNT_NOMAIN
SRCS+=		mount_${MOUNTNAME}.c rump_${MOUNTNAME}.c pathadj.c fattr.c

# on select archs use the kernel module directly, otherwise fallback
# to the rump library
.ifndef DONOTLINKLIBS
.undef RUMPKMOD
.ifdef RUMPKMOD
OSRELEASE!=	${HOST_SH} ${NETBSDSRCDIR}/sys/conf/osrelease.sh
KMODULEDIR=	${DESTDIR}/stand/${MACHINE}/${OSRELEASE}/modules/${MOUNTNAME}
LDADD+=		${KMODULEDIR}/${MOUNTNAME}.kmod
.else
LDADD+=		-lrumpfs_${MOUNTNAME}
.endif
.endif

.PATH:		${MOUNT} ${MOUNTDIR}/mount_${MOUNTNAME}
.endif # MOUNTNAME

# disk components for disk-based file systems
.if RUMP_DISKFS
LDADD+=		-lrumpdev_disk -lrumpdev
.endif

LDADD+=		-lp2k -lukfs -lrumpvfs -lrump -lrumpuser -lpuffs -lutil
.if defined(__MINIX)
LDADD+=		-lmthread -lfsdriver -lsys
.else
LDADD+=		-lpthread
.endif # defined(__MINIX)

DPADD+=		${LIBP2K} ${LIBUKFS} ${LIBRUMPVFS} ${LIBRUMP} ${LIBRUMPUSER}
.if defined(__MINIX)
DPADD+=		${LIBPUFFS} ${LIBUTIL} ${LIBMTHREAD}
.else
DPADD+=		${LIBPUFFS} ${LIBUTIL} ${LIBPTHREAD}
.endif # defined(__MINIX)
.endif # ISRUMP
