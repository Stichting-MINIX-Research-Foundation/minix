#	$NetBSD: Makefile,v 1.6 2013/11/09 21:39:27 christos Exp $
#	@(#)Makefile	8.2 (Berkeley) 3/27/94

CWARNFLAGS+=	-Wno-sign-compare

.include <bsd.own.mk>

PROG=	newfs_ext2fs
SRCS=	newfs_ext2fs.c mke2fs.c ext2fs_bswap.c partutil.c
MAN=	newfs_ext2fs.8

FSCK=${NETBSDSRCDIR}/sbin/fsck
CPPFLAGS+=-I${.CURDIR} -I${FSCK}

DPADD+= ${LIBUTIL}
LDADD+= -lutil

LDADD+=-lprop
DPADD+=${LIBPROP}

.PATH:	${NETBSDSRCDIR}/sys/ufs/ext2fs ${FSCK}

.if ${MACHINE_ARCH} == "vax"
COPTS.mke2fs.c=-O0
.endif

.include <bsd.prog.mk>
