#	$NetBSD: Makefile,v 1.4 2015/07/20 23:50:23 mrg Exp $

.include <bsd.own.mk>

PROG=	xmag
SRCS=	xmag.c CutPaste.c RootWin.c Scale.c

APPDEFS=Xmag

CPPFLAGS+=	-DHAVE_LRINT -DHAVE_NANOSLEEP

LDADD+=	-lXaw -lXmu -lXt -lSM -lICE -lXpm -lXext -lX11 -lm
DPADD+=	${LIBXAW} ${LIBXMU} ${LIBXT} ${LIBSM} ${LIBICE} \
	${LIBXPM} ${LIBXEXT} ${LIBX11} ${LIBM}

.PATH:	${X11SRCDIR.${PROG}} ${X11SRCDIR.${PROG}}/app-defaults
.PATH:	${X11SRCDIR.${PROG}}/man

.include "../../xorg-pkg-ver.mk"

.include <bsd.x11.mk>
.include <bsd.prog.mk>
