#	$NetBSD: Makefile,v 1.4 2009/08/22 02:40:26 mrg Exp $

.include "../Makefile.serverlib"
.include "../Makefile.servermod"

LIB=		composite
.PATH:		${X11SRCDIR.xorg-server}/composite

SRCS=		compalloc.c compext.c compint.h compinit.c compwindow.c \
		compoverlay.c

CPPFLAGS+=	${X11FLAGS.PERVASIVE_EXTENSION} ${X11FLAGS.DIX} ${X11INCS.DIX}

CPPFLAGS+=	-I${X11SRCDIR.xorg-server}/include \
		-I${X11SRCDIR.xorg-server}/mi \
		-I${X11SRCDIR.xorg-server}/Xext \
		-I${X11SRCDIR.xorg-server}/render \
		-I${X11SRCDIR.xorg-server}/xfixes \
		-I${X11SRCDIR.xorg-server}/damageext \
		-I${X11SRCDIR.xorg-server}/miext/damage \
		-I${DESTDIR}${X11INCDIR}/X11 \
		-I${DESTDIR}${X11INCDIR}/pixman-1 \
		-I${X11SRCDIR.xorg-server}/../include \

# XXX
COPTS.compwindow.c=	-Wno-error
COPTS.compext.c=	-Wno-error
COPTS.compinit.c=	-Wno-error

.include <bsd.x11.mk>
.include <bsd.lib.mk>
