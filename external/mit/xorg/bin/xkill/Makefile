#	$NetBSD: Makefile,v 1.3 2014/07/12 20:31:04 nakayama Exp $

.include <bsd.own.mk>

PROG=	xkill

LDADD+=	-lXmuu -lXt -lSM -lICE -lXext -lX11
DPADD+=	${LIBXMUU} ${LIBXT} ${LIBSM} ${LIBICE} ${LIBXEXT} ${LIBX11}

CPPFLAGS+=	-DPACKAGE_STRING="\"xkill 1.0.4\""

.PATH:	${X11SRCDIR.${PROG}} ${X11SRCDIR.${PROG}}/man

.include <bsd.x11.mk>
.include <bsd.prog.mk>
