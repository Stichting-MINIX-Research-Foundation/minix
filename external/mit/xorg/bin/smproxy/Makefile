#	$NetBSD: Makefile,v 1.4 2015/07/19 19:49:17 mrg Exp $

.include <bsd.own.mk>

PROG=	smproxy
SRCS=	smproxy.c save.c

CPPFLAGS+=-DHAVE_MKSTEMP -DHAVE_ASPRINTF

STRING!=	${PRINT_PACKAGE_STRING}
CPPFLAGS+=	-DPACKAGE_STRING=\"${STRING}\"

LDADD+=	-lXmu -lXt -lSM -lICE -lXext -lX11
DPADD+=	${LIBXMU} ${LIBXT} ${LIBSM} ${LIBICE} ${LIBXEXT} ${LIBX11}

.PATH:	${X11SRCDIR.${PROG}} ${X11SRCDIR.${PROG}}/man

.include <bsd.x11.mk>
.include <bsd.prog.mk>
