#	$NetBSD: Makefile,v 1.1 2008/10/12 03:09:10 rtr Exp $

DRIVER=		xf86-video-nvxbox
DRIVER_NAME=	nvxbox_drv

SRCS=		nv_dac.c nv_driver.c nv_setup.c nv_xaa.c \
		nv_cursor.c nv_dga.c nv_shadow.c nv_video.c \
		riva_hw.c

MAN=		# XXX dist is not nvxbox.man??? nvxbox.4

COPTS.nv_video.c=	-Wno-error

.include "../Makefile.xf86-driver"
