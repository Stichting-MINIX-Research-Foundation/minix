#	$NetBSD: Makefile,v 1.2 2015/07/19 22:43:26 mrg Exp $

.include <bsd.own.mk>

PROG=	xfindproxy

LDADD+=	-lXt -lSM -lICE -lXext -lX11
DPADD+=	${LIBXT} ${LIBSM} ${LIBICE} ${LIBXEXT} ${LIBX11}

.PATH:	${X11SRCDIR.${PROG}}

.include "../../xorg-pkg-ver.mk"

.include <bsd.x11.mk>
.include <bsd.prog.mk>
