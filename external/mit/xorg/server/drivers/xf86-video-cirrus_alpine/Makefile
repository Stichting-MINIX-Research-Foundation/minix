#	$NetBSD: Makefile,v 1.3 2008/10/27 20:29:10 veego Exp $

# XXX move this under ../xf86-video-cirrus

DRIVER=		xf86-video-cirrus
DRIVER_NAME=	cirrus_alpine

SRCS=		alp_driver.c alp.h alp_hwcurs.c alp_i2c.c alp_xaa.c
SRCS+=		alp_xaam.c

#CPPFLAGS.cir_driver.c=	-DVERSION=${PACKAGE_MAJOR}

.include "../Makefile.xf86-driver"
