#	$NetBSD: Makefile,v 1.3 2009/06/05 21:52:31 haad Exp $
#	@(#)Makefile	8.2 (Berkeley) 3/27/94

WARNS?=	3	# XXX: sign-compare issues

.include <bsd.own.mk>

PROG=	newfs_ext2fs
SRCS=	newfs_ext2fs.c mke2fs.c ext2fs_bswap.c partutil.c
MAN=	newfs_ext2fs.8

FSCK=${NETBSDSRCDIR}/sbin/fsck
MYFSCK=${NETBSDSRCDIR}/sbin/fsck_ext2fs
CPPFLAGS+=-I${.CURDIR} -I${FSCK}

DPADD+= ${LIBUTIL}
LDADD+= -lutil

LDADD+=-lprop
DPADD+=${LIBPROP}

SYMLINKS+= $(BINDIR)/$(PROG)  $(BINDIR)/newfs_ext2 \
	$(BINDIR)/$(PROG)  $(BINDIR)/mkfs.ext2

.PATH:	${FSCK} ${MYFSCK} #${NETBSDSRCDIR}/sys/ufs/ext2fs 

.include <bsd.prog.mk>
