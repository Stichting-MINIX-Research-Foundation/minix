#	$NetBSD: Makefile,v 1.1 2015/05/21 00:12:10 rjs Exp $

DRIVER=		xf86-video-intel-old
DRIVER_NAME=	ch7017_drv


SRCS=		ch7017_module.c ch7017.c

CPPFLAGS+=      -I${DESTDIR}${X11INCDIR}/X11
CPPFLAGS+=      -I${DESTDIR}${X11INCDIR}/X11/dri

#LSC: Clang/Minix3
NOCLANGERROR?=	yes

.include "../../Makefile.xf86-driver"
.PATH: ${X11SRCDIR.${DRIVER}}/src/ch7017
