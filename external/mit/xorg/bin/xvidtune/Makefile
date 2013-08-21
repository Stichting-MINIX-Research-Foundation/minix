#	$NetBSD: Makefile,v 1.4 2013/05/31 08:02:00 mrg Exp $

.include <bsd.own.mk>

PROG=		xvidtune
APPDEFS=	Xvidtune

SRCS=	xvidtune.c

LDADD+=	-lXxf86vm -lXaw -lXmu -lXt -lSM -lICE -lXpm -lXext -lX11
DPADD+=	${LIBXXF86VM} ${LIBXAW} ${LIBXMU} ${LIBXT} ${LIBSM} ${LIBICE} \
	${LIBXPM} ${LIBXEXT} ${LIBX11}

.PATH:	${X11SRCDIR.${PROG}} ${X11SRCDIR.${PROG}}/app-defaults
.PATH:	${X11SRCDIR.${PROG}}/man

VENDOR_NAME=The Xorg Foundation

.include <bsd.x11.mk>
.include <bsd.prog.mk>
