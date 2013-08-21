#	$NetBSD: Makefile,v 1.6 2012/03/22 23:46:27 joerg Exp $

DRIVER=		xf86-video-trident
DRIVER_NAME=	trident_drv

SRCS=		blade_accel.c blade_accel_exa.c image_accel.c
SRCS+=		trident_accel.c trident_bank.c trident_dac.c
SRCS+=		trident_dga.c trident_driver.c tridenthelper.c
SRCS+=		trident_i2c.c trident_shadow.c
SRCS+=		trident_tv.c trident_video.c tvga_dac.c xp_accel.c
SRCS+=		xp4_accel.c xp4_accel_exa.c
MAN=		trident.4

COPTS.trident_driver.c=	-Wno-error	# XXX xf86dgastr.h

CWARNFLAGS.clang+=	-Wno-return-type -Wno-empty-body

.include "../Makefile.xf86-driver"
