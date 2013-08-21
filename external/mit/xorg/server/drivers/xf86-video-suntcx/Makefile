#	$NetBSD: Makefile,v 1.2 2013/06/04 22:45:08 mrg Exp $

DRIVER=		xf86-video-suntcx
DRIVER_NAME=	suntcx_drv

SRCS=		tcx_driver.c tcx_cursor.c tcx_accel.c

CPPFLAGS+=	${X11FLAGS.DRI}
CPPFLAGS+=	-DHAVE_XAA_H

MAN=		suntcx.4

.include "../Makefile.xf86-driver"
