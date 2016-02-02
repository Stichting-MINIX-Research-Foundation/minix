#	$NetBSD: Makefile,v 1.5 2015/07/23 02:04:09 mrg Exp $

DRIVER=		xf86-video-ark
DRIVER_NAME=	ark_drv

SRCS=		ark_accel.c ark_driver.c

CPPFLAGS+=	-DPACKAGE_VERSION=\"${PACKAGE_VERSION}\"
CPPFLAGS+=	-DHAVE_XAA_H

#LSC: Clang/Minix3
NOCLANGERROR?=	yes

.include "../Makefile.xf86-driver"
