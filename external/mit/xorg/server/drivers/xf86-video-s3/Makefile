#	$NetBSD: Makefile,v 1.11 2013/06/03 08:51:18 mrg Exp $

DRIVER=		xf86-video-s3
DRIVER_NAME=	s3_drv
DRIVER_VERSION= 0.6.3

SRCS=		s3_bios.c s3_cursor.c s3_dga.c s3_driver.c s3_IBMRGB.c \
		s3_Ti.c s3_Trio64DAC.c s3_video.c s3_accel.c \
		s3_accel_newmmio.c s3_shadow.c
SRCS+=		s3_GENDAC.c
MAN=		s3.4

# PLEASE NOTE: s3_accel.c is compiled two times with different
# cpp macros for PIO and NEWMMIO support. Check Makefile.am
# before upgrading the driver.
#
BUILDSYMLINKS=	s3_accel.c s3_accel_newmmio.c
CPPFLAGS.s3_accel_newmmio.c+=	-DS3_NEWMMIO=1 \
				-I${X11SRCDIR.xf86-video-s3}/src
CPPFLAGS.s3_video.c+=		-DS3_NEWMMIO=1 \

CPPFLAGS+=	-DPACKAGE_VERSION=\"${PACKAGE_VERSION}\"
CPPFLAGS+=	-DHAVE_XAA_H

CWARNFLAGS.clang+=	-Wno-parentheses

.include "../Makefile.xf86-driver"
