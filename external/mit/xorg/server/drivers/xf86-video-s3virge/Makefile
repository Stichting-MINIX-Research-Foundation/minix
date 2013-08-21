#	$NetBSD: Makefile,v 1.4 2011/08/11 23:15:40 joerg Exp $

DRIVER=		xf86-video-s3virge
DRIVER_NAME=	s3virge_drv

SRCS=		s3v_accel.c s3v_dac.c s3v_dga.c s3v_driver.c s3v_hwcurs.c
SRCS+=		s3v_i2c.c s3v_shadow.c s3v_xv.c
MAN=		s3virge.4

CPPFLAGS+=	-DPACKAGE_VERSION=\"${PACKAGE_VERSION}\"

CWARNFLAGS.clang+=	-Wno-parentheses

.include "../Makefile.xf86-driver"
