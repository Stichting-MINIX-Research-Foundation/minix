#	$NetBSD: Makefile,v 1.3 2010/05/23 01:59:13 mrg Exp $

DRIVER=		xf86-video-intel
DRIVER_NAME=	ch7017_drv


SRCS=		ch7017_module.c ch7017.c

CPPFLAGS+=      -I${DESTDIR}${X11INCDIR}/X11
CPPFLAGS+=      -I${DESTDIR}${X11INCDIR}/X11/dri

.include "../../Makefile.xf86-driver"

#LSC: Clang/Minix3
NOCLANGERROR?=	yes

.PATH: ${X11SRCDIR.${DRIVER}}/src/ch7017
