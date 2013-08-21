#	$NetBSD: Makefile,v 1.3 2013/05/31 09:14:18 mrg Exp $

.include <bsd.own.mk>

PROG=	xstdcmap

CPPFLAGS+=      -DPACKAGE_STRING="\"xstdcmap 1.0.3\""

LDADD+=	-lXmu -lXt -lSM -lICE -lXext -lX11
DPADD+=	${LIBXMU} ${LIBXT} ${LIBSM} ${LIBICE} ${LIBXEXT} ${LIBX11}

.PATH:	${X11SRCDIR.${PROG}}
.PATH:	${X11SRCDIR.${PROG}}/man

.include <bsd.x11.mk>
.include <bsd.prog.mk>
