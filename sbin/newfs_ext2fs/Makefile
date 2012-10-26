#	$NetBSD: Makefile,v 1.4 2012/03/24 08:22:24 joerg Exp $
#	@(#)Makefile	8.2 (Berkeley) 3/27/94

WARNS?=	4

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

SYMLINKS+= $(BINDIR)/$(PROG)  $(BINDIR)/newfs_ext2 \
	$(BINDIR)/$(PROG)  $(BINDIR)/mkfs.ext2

#.PATH:	${NETBSDSRCDIR}/sys/ufs/ext2fs ${FSCK}
.PATH:	${NETBSDSRCDIR}/sbin/fsck_ext2fs ${FSCK}

.include <bsd.prog.mk>
