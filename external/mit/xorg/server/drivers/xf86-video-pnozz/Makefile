#	$NetBSD: Makefile,v 1.1 2009/08/26 22:30:46 macallan Exp $

DRIVER=		xf86-video-pnozz
DRIVER_NAME=	pnozz_drv

SRCS=		pnozz_driver.c pnozz_cursor.c pnozz_accel.c

CPPFLAGS+=	${X11FLAGS.DRI}

MAN=		pnozz.4

.include "../Makefile.xf86-driver"
