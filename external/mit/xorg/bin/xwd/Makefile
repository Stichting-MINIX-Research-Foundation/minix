#	$NetBSD: Makefile,v 1.4 2014/03/17 10:09:18 mrg Exp $

.include <bsd.own.mk>

PROG=	xwd
SRCS=	xwd.c clientwin.c dsimple.c list.c multiVis.c

CPPFLAGS+=-I${X11SRCDIR./xlsfonts} -DXKB
.PATH:	${X11SRCDIR./xlsfonts}

LDADD+=	-lXmuu -lXt -lSM -lICE -lXext -lX11
DPADD+=	${LIBXMUU} ${LIBXT} ${LIBSM} ${LIBICE} ${LIBXEXT} ${LIBX11}

.PATH:	${X11SRCDIR.${PROG}}
.PATH:	${X11SRCDIR.${PROG}}/man

.include <bsd.x11.mk>
.include <bsd.prog.mk>
