#	$NetBSD: Makefile,v 1.3 2008/08/03 09:24:14 rtr Exp $

DRIVER=		xf86-video-cirrus
DRIVER_NAME=	cirrus_drv

SRCS=		cir_dga.c cir_driver.c CirrusClk.c cir_shadow.c
MAN=		cirrus.4

CPPFLAGS.cir_driver.c=	-DVERSION=${PACKAGE_MAJOR}

.include "../Makefile.xf86-driver"
