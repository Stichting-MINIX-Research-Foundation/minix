#	$NetBSD: Makefile,v 1.4 2013/05/31 09:14:18 mrg Exp $

.include <bsd.own.mk>

PROG=	xsetroot

CPPFLAGS+=	-DPACKAGE_STRING="\"xsetroot 1.1.1\""

LDADD+=	-lXmuu -lXt -lSM -lICE -lXext -lX11 -lXcursor
DPADD+=	${LIBXMUU} ${LIBXT} ${LIBSM} ${LIBICE} ${LIBXEXT} ${LIBX11} ${LIBXCURSOR}

.PATH:	${X11SRCDIR.${PROG}}
.PATH:	${X11SRCDIR.${PROG}}/man

.include <bsd.x11.mk>
.include <bsd.prog.mk>
