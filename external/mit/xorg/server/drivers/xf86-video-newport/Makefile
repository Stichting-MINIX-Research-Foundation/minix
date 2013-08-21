#	$NetBSD: Makefile,v 1.1 2009/02/14 03:15:15 macallan Exp $

DRIVER=		xf86-video-newport
DRIVER_NAME=	newport_drv

SRCS=		newport_accel.c newport_cmap.c newport_cursor.c \
		newport_driver.c newport_regs.c newport_shadow.c

CPPFLAGS+=	${X11FLAGS.DRI}

MAN=		newport.4

.include "../Makefile.xf86-driver"
