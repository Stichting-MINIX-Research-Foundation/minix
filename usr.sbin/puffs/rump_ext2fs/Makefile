#	$NetBSD: Makefile,v 1.6 2010/01/25 15:55:36 pooka Exp $
#

.include <bsd.own.mk>

MOUNTNAME=	ext2fs

LDADD+=		-lrumpfs_ffs
DPADD+=		${LIBRUMPFS_FFS}

ISRUMP=		# yea baby
RUMP_DISKFS=

.include <bsd.prog.mk>
