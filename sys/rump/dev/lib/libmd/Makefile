#	$NetBSD: Makefile,v 1.2 2014/03/17 11:30:40 pooka Exp $
#

.PATH:	${.CURDIR}/../../../../dev

LIB=	rumpdev_md
IOCONF=	MD.ioconf

SRCS=	md.c
SRCS+=	md_component.c

CPPFLAGS+=	-I${.CURDIR}/../../../librump/rumpvfs

.include <bsd.lib.mk>
.include <bsd.klinks.mk>
