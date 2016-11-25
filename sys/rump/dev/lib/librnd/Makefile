#	$NetBSD: Makefile,v 1.9 2015/08/20 11:51:12 christos Exp $
#

.PATH:	${.CURDIR}/../../../../dev

LIB=	rumpdev_rnd
IOCONF=	RND.ioconf
SRCS=	rndpseudo.c

SRCS+=	rnd_component.c

CPPFLAGS+=	-I${RUMPTOP}/librump/rumpvfs

.include <bsd.lib.mk>
.include <bsd.klinks.mk>
