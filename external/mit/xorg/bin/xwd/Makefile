#	$NetBSD: Makefile,v 1.3 2013/05/31 08:02:00 mrg Exp $

.include <bsd.own.mk>

PROG=	xwd
SRCS=	xwd.c clientwin.c dsimple.c list.c multiVis.c

CPPFLAGS+=-I${X11SRCDIR./xlsfonts}
.PATH:	${X11SRCDIR./xlsfonts}

LDADD+=	-lXmuu -lXt -lSM -lICE -lXext -lX11
DPADD+=	${LIBXMUU} ${LIBXT} ${LIBSM} ${LIBICE} ${LIBXEXT} ${LIBX11}

.PATH:	${X11SRCDIR.${PROG}}
.PATH:	${X11SRCDIR.${PROG}}/man

.include <bsd.x11.mk>
.include <bsd.prog.mk>
