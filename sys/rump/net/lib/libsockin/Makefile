#	$NetBSD: Makefile,v 1.8 2014/03/13 02:05:49 pooka Exp $
#

LIB=	rumpnet_sockin

SRCS=	sockin.c
SRCS+=	sockin_component.c

CPPFLAGS+= -I${RUMPTOP}/librump/rumpkern

RUMPCOMP_USER_SRCS=	sockin_user.c

.include <bsd.lib.mk>
.include <bsd.klinks.mk>
