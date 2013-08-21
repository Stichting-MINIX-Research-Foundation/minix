#	$NetBSD: Makefile,v 1.6 2011/05/15 22:55:01 christos Exp $

DRIVER=		xf86-video-chips
DRIVER_NAME=	chips_drv

SRCS=		ct_accel.c ct_bank.c ct_cursor.c ct_ddc.c ct_dga.c
SRCS+=		ct_driver.c ct_regs.c ct_shadow.c ct_video.c
SRCS+=		ct_accelmm.c ct_accelhi.c
MAN=		chips.4

.if ${MACHINE} == "shark"
CPPFLAGS+=		-DHAVE_ISA
.endif

CPPFLAGS.ct_driver.c=	-DVERSION=${PACKAGE_MAJOR} -I${X11SRCDIR.xorg-server}
CPPFLAGS.ct_accelmm.c=	-DCHIPS_MMIO -I${X11SRCDIR.${DRIVER}}/src
CPPFLAGS.ct_accelhi.c=	-DCHIPS_MMIO -DCHIPS_HIQV -I${X11SRCDIR.${DRIVER}}/src

BUILDSYMLINKS=	ct_accel.c ct_accelmm.c \
		ct_accel.c ct_accelhi.c

.include "../Makefile.xf86-driver"
