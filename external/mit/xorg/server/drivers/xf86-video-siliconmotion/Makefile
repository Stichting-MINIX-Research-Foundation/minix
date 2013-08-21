#	$NetBSD: Makefile,v 1.6 2013/06/03 09:12:28 mrg Exp $

DRIVER=		xf86-video-siliconmotion
DRIVER_NAME=	siliconmotion_drv

SRCS=		smi_501.c smi_accel.c smi_xaa.c smi_exa.c smi_dac.c
SRCS+=		smi_driver.c smi_i2c.c smi_video.c
SRCS+=		smi_crtc.c smilynx_crtc.c smi_output.c 
SRCS+=		smilynx_output.c smilynx_hw.c smilynx.h 
SRCS+=		smi501_crtc.c smi501_output.c
MAN=		siliconmotion.4

CPPFLAGS+=	-I${X11SRCDIR.${DRIVER}}/../include -DHAVE_CONFIG_H

.include "../Makefile.xf86-driver"
