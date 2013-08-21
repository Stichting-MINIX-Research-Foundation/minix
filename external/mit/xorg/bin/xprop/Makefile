#	$NetBSD: Makefile,v 1.5 2013/05/30 23:42:19 mrg Exp $

.include <bsd.own.mk>

PROG=	xprop
SRCS=	xprop.c dsimple.c clientwin.c

CPPFLAGS+=-DHAVE_WCHAR_H -DHAVE_WCTYPE_H -DNO_WIDEC_H
CPPFLAGS+=-I${X11SRCDIR.${PROG}}/../include

LDADD+=	-lXmuu -lXt -lSM -lICE -lXext -lX11
DPADD+=	${LIBXMUU} ${LIBXT} ${LIBSM} ${LIBICE} ${LIBXEXT} ${LIBX11}

.PATH:	${X11SRCDIR.${PROG}} ${X11SRCDIR.${PROG}}/man

COPTS.xprop.c+=	-Wno-error	# uses XKeycodeToKeysym

.include <bsd.x11.mk>
.include <bsd.prog.mk>
