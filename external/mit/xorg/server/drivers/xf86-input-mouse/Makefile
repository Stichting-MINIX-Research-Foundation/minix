#	$NetBSD: Makefile,v 1.7 2015/07/22 09:49:51 mrg Exp $

WARNS?=		2

DRIVER=		xf86-input-mouse
DRIVER_NAME=	mouse_drv

SRCS=		mouse.c pnp.c bsd_mouse.c
MAN=		mousedrv.4

CPPFLAGS+=	-DHAVE_XORG_CONFIG_H -DHAVE_ASPRINTF
CPPFLAGS+=	-I${X11SRCDIR.${DRIVER}}/include

INCSDIR=	${X11INCDIR}/xorg
INCS=		xf86-mouse-properties.h

PKGCONFIG=	xorg-mouse
PKGDIST.xorg-mouse=	${X11SRCDIR.${DRIVER}}

.include "../Makefile.xf86-driver"

.PATH:  ${X11SRCDIR.${DRIVER}}/include
.PATH:  ${X11SRCDIR.${DRIVER}}

XORG_PKG_VER_PROG=	${DRIVER}
.include "../../../xorg-pkg-ver.mk"
