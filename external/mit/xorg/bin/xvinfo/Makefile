#	$NetBSD: Makefile,v 1.5 2015/07/19 23:15:06 mrg Exp $

.include <bsd.own.mk>

PROG=	xvinfo

LDADD+=	-lXv -lXext -lX11
DPADD+=	${LIBXV} ${LIBXEXT} ${LIBX11}

.PATH:	${X11SRCDIR.${PROG}} ${X11SRCDIR.${PROG}}/man

CWARNFLAGS.clang+=	-Wno-dangling-else

.include "../../xorg-pkg-ver.mk"

.include <bsd.x11.mk>
.include <bsd.prog.mk>
