#	$NetBSD: Makefile,v 1.25 2012/03/21 05:37:43 matt Exp $
#

.include <bsd.own.mk>

USE_FORT?=	yes	# data-driven bugs?

WARNS?=		5

LIB=		puffs

SRCS=		puffs.c callcontext.c creds.c \
		null.c pnode.c \
		subr.c
MAN=		puffs.3 puffs_cc.3 puffs_cred.3 puffs_flush.3		\
		puffs_framebuf.3 puffs_node.3 puffs_ops.3 puffs_path.3
INCS=		puffs.h
INCSDIR=	/usr/include
LINTFLAGS+=-S -w

.if defined(__MINIX)
SRCS+=		inode.c link.c misc.c mount.c open.c path.c path_puffs.c \
		protect.c read.c stadir.c time.c utility.c table.c

CPPFLAGS+= -D_MINIX_SYSTEM

NOGCCERROR=yes
NOCLANGERROR=yes
.endif # defined(__MINIX)

.include <bsd.lib.mk>
