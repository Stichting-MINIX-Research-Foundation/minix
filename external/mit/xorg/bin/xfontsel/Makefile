#	$NetBSD: Makefile,v 1.3 2013/05/31 07:22:21 mrg Exp $

.include <bsd.own.mk>

PROG=	xfontsel
SRCS=	xfontsel.c ULabel.c

APPDEFS=XFontSel

LDADD+=	-lXaw -lXmu -lXt -lSM -lICE -lXpm -lXext -lX11
DPADD+=	${LIBXAW} ${LIBXMU} ${LIBXT} ${LIBSM} ${LIBICE} \
	${LIBXPM} ${LIBXEXT} ${LIBX11}

.PATH:	${X11SRCDIR.${PROG}} ${X11SRCDIR.${PROG}}/app-defaults
.PATH:	${X11SRCDIR.${PROG}}/man

.include <bsd.x11.mk>
.include <bsd.prog.mk>
