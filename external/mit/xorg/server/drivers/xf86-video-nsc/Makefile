#	$NetBSD: Makefile,v 1.4 2009/06/12 04:40:22 mrg Exp $

DRIVER=		xf86-video-nsc
DRIVER_NAME=	nsc_drv

SRCS=		durango.c nsc_driver.c nsc_galstub.c
SRCS+=		nsc_gx1_accel.c nsc_gx1_cursor.c nsc_gx1_dga.c
SRCS+=		nsc_gx1_driver.c nsc_gx1_shadow.c nsc_gx1_video.c
SRCS+=		nsc_gx2_accel.c nsc_gx2_cursor.c nsc_gx2_dga.c
SRCS+=		nsc_gx2_driver.c nsc_gx2_shadow.c nsc_gx2_video.c
SRCS+=		panel.c
MAN=		nsc.4

CPPFLAGS+=	-I${X11SRCDIR.${DRIVER}}/src/gfx
CPPFLAGS+=	-I${X11SRCDIR.${DRIVER}}/src/panel

.include "../Makefile.xf86-driver"
