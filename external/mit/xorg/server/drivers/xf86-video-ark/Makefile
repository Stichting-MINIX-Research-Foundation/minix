#	$NetBSD: Makefile,v 1.4 2009/06/10 01:59:09 mrg Exp $

DRIVER=		xf86-video-ark
DRIVER_NAME=	ark_drv

SRCS=		ark_accel.c ark_driver.c

CPPFLAGS+=	-DPACKAGE_VERSION=\"${PACKAGE_VERSION}\"

#LSC: Clang/Minix3
NOCLANGERROR?=	yes

.include "../Makefile.xf86-driver"
