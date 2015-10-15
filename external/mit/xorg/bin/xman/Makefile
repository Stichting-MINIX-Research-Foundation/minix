#	$NetBSD: Makefile,v 1.7 2015/07/20 23:50:23 mrg Exp $

.include <bsd.own.mk>

PROG=	xman
SRCS=	ScrollByL.c handler.c man.c buttons.c help.c \
	search.c globals.c main.c misc.c tkfuncs.c vendor.c

CPPFLAGS+=${X11FLAGS.VERSION}
CPPFLAGS+=-DHELPFILE=\"${X11LIBDIR}/xman.help\"
CPPFLAGS+=-DMANCONFIGSTYLE_OpenBSD -DMANCONF=\"/etc/man.conf\"

APPDEFS=Xman

FILES=	xman.help
FILESDIR=${X11LIBDIR}

LDADD+=	-lXaw -lXmu -lXt -lSM -lICE -lXpm -lXext -lX11
DPADD+=	${LIBXAW} ${LIBXMU} ${LIBXT} ${LIBSM} ${LIBICE} \
	${LIBXPM} ${LIBXEXT} ${LIBX11}

.PATH:	${X11SRCDIR.${PROG}} ${X11SRCDIR.${PROG}}/app-defaults ${X11SRCDIR.${PROG}}/man

.include "../../xorg-pkg-ver.mk"

.include <bsd.x11.mk>
.include <bsd.prog.mk>
