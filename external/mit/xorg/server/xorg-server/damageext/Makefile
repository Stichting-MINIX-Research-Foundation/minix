#	$NetBSD: Makefile,v 1.3 2011/01/15 13:54:52 jmcneill Exp $

.include "../Makefile.serverlib"
.include "../Makefile.servermod"

LIB=		damageext
.PATH:		${X11SRCDIR.xorg-server}/damageext

SRCS=		damageext.c

CPPFLAGS+=	-I${X11SRCDIR.xorg-server}/include \
		-I${X11SRCDIR.xorg-server}/Xext \
		-I${X11SRCDIR.xorg-server}/xfixes \
		-I${X11SRCDIR.xorg-server}/damagext \
		-I${DESTDIR}${X11INCDIR}/X11 \
		-I${DESTDIR}${X11INCDIR}/xorg \
		-I${DESTDIR}${X11INCDIR}/pixman-1 \
		-I${X11SRCDIR.xorg-server}/../include \
		${X11FLAGS.DIX} ${X11INCS.DIX}

COPTS.damageext.c+=	-Wno-error

.include <bsd.x11.mk>
.include <bsd.lib.mk>
