#	$NetBSD: Makefile,v 1.4 2014/07/12 20:31:04 nakayama Exp $

.include <bsd.own.mk>

PROG=	xset

CPPFLAGS+=${X11FLAGS.EXTENSION} -DXKB
CPPFLAGS+=	-DPACKAGE_STRING="\"xset 1.2.3\""

LDADD+=	-lXmuu -lXt -lSM -lICE -lXxf86misc -lXfontcache -lXext -lX11
DPADD+= ${LIBXMUU} ${LIBXT} ${LIBSM} ${LIBICE} ${LIBXXF86MISC} ${LIBXFONTCACHE}
DPADD+=	${LIBXEXT} ${LIBX11}

.PATH:	${X11SRCDIR.${PROG}}
.PATH:	${X11SRCDIR.${PROG}}/man

.include <bsd.x11.mk>
.include <bsd.prog.mk>
