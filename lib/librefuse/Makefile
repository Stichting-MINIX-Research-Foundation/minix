# $NetBSD: Makefile,v 1.8 2007/11/05 13:41:52 pooka Exp $

USE_FORT?=	no	# data driven bugs?

LIB=		refuse
LIBDPLIBS+=	puffs	${.CURDIR}/../libpuffs

.ifdef DEBUG
FUSE_OPT_DEBUG_FLAGS=	-g -DFUSE_OPT_DEBUG
.endif

CFLAGS+=	${FUSE_OPT_DEBUG_FLAGS}
SRCS=		refuse.c refuse_opt.c
MAN=		refuse.3
WARNS=		4
INCS=           fuse.h fuse_opt.h
INCSDIR=        /usr/include

.include <bsd.lib.mk>
