# $NetBSD: Makefile,v 1.9 2012/03/21 05:37:43 matt Exp $

USE_FORT?=	yes	# data driven bugs?

LIB=		refuse
.if !defined(__MINIX)
LIBDPLIBS+=	puffs	${.CURDIR}/../libpuffs		\
		pthread	${.CURDIR}/../libpthread
.else
LIBDPLIBS+=	puffs	${.CURDIR}/../libpuffs
.endif # !defined(__MINIX)

.ifdef DEBUG
FUSE_OPT_DEBUG_FLAGS=	-g -DFUSE_OPT_DEBUG
.endif

CFLAGS+=	${FUSE_OPT_DEBUG_FLAGS}
SRCS=		refuse.c refuse_opt.c
MAN=		refuse.3
WARNS?=		5
INCS=           fuse.h fuse_opt.h
INCSDIR=        /usr/include

.include <bsd.lib.mk>
