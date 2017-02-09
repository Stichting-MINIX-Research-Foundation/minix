#	$NetBSD: Makefile,v 1.3 2015/08/20 11:51:12 christos Exp $
#

.PATH:	${.CURDIR}/../../../../dev

LIB=	rumpdev_fss
IOCONF=	FSS.ioconf
SRCS=   fss.c

SRCS+=	fss_component.c

CPPFLAGS+=	-I${RUMPTOP}/librump/rumpvfs

.include <bsd.lib.mk>
.include <bsd.klinks.mk>
