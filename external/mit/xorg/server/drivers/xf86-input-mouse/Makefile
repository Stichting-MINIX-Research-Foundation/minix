#	$NetBSD: Makefile,v 1.6 2013/06/05 07:26:36 mrg Exp $

WARNS?=		2

DRIVER=		xf86-input-mouse
DRIVER_NAME=	mouse_drv

SRCS=		mouse.c pnp.c bsd_mouse.c
MAN=		mousedrv.4

CPPFLAGS+=	-DHAVE_CONFIG_H \
		-I${X11SRCDIR.${DRIVER}}/include \
		-I${X11SRCDIR.${DRIVER}}/../include

INCSDIR=	${X11INCDIR}/xorg
INCS=		xf86-mouse-properties.h

PKGCONFIG=	xorg-mouse
PKGDIST.xorg-mouse=	${X11SRCDIR.${DRIVER}}

.include "../Makefile.xf86-driver"

.PATH:  ${X11SRCDIR.${DRIVER}}/include
.PATH:  ${X11SRCDIR.${DRIVER}}
