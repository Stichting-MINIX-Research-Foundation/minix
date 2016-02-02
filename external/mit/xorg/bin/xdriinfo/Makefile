#	$NetBSD: Makefile,v 1.2 2015/07/19 19:49:18 mrg Exp $

.include <bsd.own.mk>

PROG=	xdriinfo

SRCS=	xdriinfo.c

#LSC: No pthreads on MINIX!
LDADD+=	-lGL -lXext -lX11
DPADD+=	${LIBGL} ${LIBXEXT} ${LIBX11}

.PATH:	${X11SRCDIR.${PROG}} ${X11SRCDIR.${PROG}}/man

.include "../../xorg-pkg-ver.mk"

.include <bsd.x11.mk>
.include <bsd.prog.mk>
