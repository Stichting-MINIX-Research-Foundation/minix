#	$NetBSD: Makefile,v 1.5 2011/07/23 06:06:40 mrg Exp $

.include <bsd.own.mk>

PROG=	xman
SRCS=	ScrollByL.c handler.c man.c buttons.c help.c \
	search.c globals.c main.c misc.c tkfuncs.c vendor.c

CPPFLAGS+=${X11FLAGS.VERSION}
CPPFLAGS+=-DHELPFILE=\"${X11LIBDIR}/xman.help\"
CPPFLAGS+=-DMANCONFIGSTYLE_OpenBSD -DMANCONF=\"/etc/man.conf\"
CPPFLAGS+=-DHAVE_CONFIG_H -I${X11SRCDIR.${PROG}}/../include

APPDEFS=Xman

FILES=	xman.help
FILESDIR=${X11LIBDIR}

LDADD+=	-lXaw -lXmu -lXt -lSM -lICE -lXpm -lXext -lX11
DPADD+=	${LIBXAW} ${LIBXMU} ${LIBXT} ${LIBSM} ${LIBICE} \
	${LIBXPM} ${LIBXEXT} ${LIBX11}

.PATH:	${X11SRCDIR.${PROG}} ${X11SRCDIR.${PROG}}/app-defaults ${X11SRCDIR.${PROG}}/man

.include <bsd.x11.mk>
.include <bsd.prog.mk>
