#	$NetBSD: Makefile,v 1.6 2014/03/13 01:53:48 pooka Exp $
#

.PATH:	${.CURDIR}/../../../../dev/putter

LIB=	rumpdev_putter

SRCS=	putter.c
SRCS+=	putter_component.c

CPPFLAGS+=	-I${RUMPTOP}/librump/rumpvfs

.include <bsd.lib.mk>
.include <bsd.klinks.mk>
