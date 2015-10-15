#	$NetBSD: Makefile,v 1.10 2015/07/30 02:58:00 macallan Exp $

DRIVER=		xf86-video-r128
DRIVER_NAME=	r128_drv

SRCS=	r128_accel.c r128_cursor.c r128_driver.c r128_exa.c \
	r128_crtc.c r128_output.c \
	r128_video.c r128_misc.c r128_probe.c r128_dri.c

MAN=	r128.4

.if ${MACHINE_ARCH} == "powerpc" || ${MACHINE} == "sparc64"
CPPFLAGS+= -DAVOID_FBDEV
.endif

# Enable VGA support (esp. hw state save/restore) on i386 / amd64 platforms
.if ${MACHINE} == "amd64" || ${MACHINE} == "i386"
CPPFLAGS+=	-DWITH_VGAHW
.endif

CPPFLAGS+=	-I${DESTDIR}${X11INCDIR}/libdrm

CPPFLAGS+=	-DHAVE_XAA_H -DUSE_EXA -DR128DRI

CWARNFLAGS.clang+=	-Wno-format -Wno-pointer-sign

.include "../Makefile.xf86-driver"
