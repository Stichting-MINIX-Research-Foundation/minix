#	$NetBSD: Makefile,v 1.6 2014/07/12 20:31:04 nakayama Exp $

.include <bsd.own.mk>

PROG=	xrdb

CPPFLAGS+=-DCPP="\"/usr/bin/cpp -traditional\"" -DHAS_MKSTEMP	# XXX
CPPFLAGS+=-DHAVE_ASPRINTF -DPACKAGE_STRING="\"xrdb 1.1.0\""

LDADD+=	-lXmuu -lXt -lSM -lICE -lXext -lX11
DPADD+=	${LIBXMUU} ${LIBXT} ${LIBSM} ${LIBICE} ${LIBXEXT} ${LIBX11}

.PATH:	${X11SRCDIR.${PROG}}
.PATH:	${X11SRCDIR.${PROG}}/man

.include <bsd.x11.mk>
.include <bsd.prog.mk>
