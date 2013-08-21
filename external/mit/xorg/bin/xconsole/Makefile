#	$NetBSD: Makefile,v 1.3 2011/05/15 21:25:33 christos Exp $

.include <bsd.own.mk>

PROG=	xconsole

APPDEFS=XConsole

LDADD+=	-lutil -lXaw -lXmu -lXt -lSM -lICE -lXpm -lXext -lX11
DPADD+=	${LIBUTIL} ${LIBXAW} ${LIBXMU} ${LIBXT} ${LIBSM} ${LIBICE} \
	${LIBXPM} ${LIBXEXT} ${LIBX11}

.PATH:	${X11SRCDIR.${PROG}} ${X11SRCDIR.${PROG}}/app-defaults

CPPFLAGS+=-DHAS_OPENPTY -DHAVE_UTIL_H

.include <bsd.x11.mk>
.include <bsd.prog.mk>
