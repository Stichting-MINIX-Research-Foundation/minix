# Makefile for libpuffs

LIB=    puffs
CC=	clang
SRCS=	callcontext.c  creds.c  null.c  pnode.c  puffs.c  subr.c\
	table.c  link.c  misc.c  open.c  path.c  path_puffs.c  protect.c\
	read.c  stadir.c  time.c  utility.c  mount.c  device.c  inode.c
INCS=	puffs.h puffs_msgif.h
INCSDIR=       /usr/include
MAN=		puffs.3 puffs_cc.3 puffs_cred.3 puffs_node.3 \
		puffs_ops.3 puffs_path.3

DEFAULT_NR_BUFS= 1024

CPPFLAGS+=	-D_MINIX -D_POSIX_SOURCE -D_POSIX_C_SOURCE -D_NETBSD_SOURCE -DDEFAULT_NR_BUFS=${DEFAULT_NR_BUFS}

.include <bsd.lib.mk>
