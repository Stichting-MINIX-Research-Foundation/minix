#	$NetBSD: Makefile,v 1.24 2010/12/06 14:50:34 pooka Exp $
#

.include <bsd.own.mk>

USE_FORT?= no	# data-driven bugs?
# LSC Until this is fixed
NOGCCERROR=yes
CPPFLAGS+=-isystem ${DESTDIR}/usr/include

WARNS=		4

LIB=		puffs

SRCS=		puffs.c callcontext.c creds.c \
		null.c pnode.c \
		subr.c \
		inode.c link.c misc.c mount.c open.c path.c path_puffs.c \
		protect.c read.c stadir.c time.c utility.c table.c
MAN=		puffs.3 puffs_cc.3 puffs_cred.3 	\
		puffs_node.3 puffs_ops.3 puffs_path.3
INCS=		puffs.h puffs_msgif.h
INCSDIR=	/usr/include
LINTFLAGS+=-S -w

.include <bsd.lib.mk>
