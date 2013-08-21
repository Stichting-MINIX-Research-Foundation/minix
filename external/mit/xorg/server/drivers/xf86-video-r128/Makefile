#	$NetBSD: Makefile,v 1.7 2013/06/03 08:51:18 mrg Exp $

DRIVER=		xf86-video-r128
DRIVER_NAME=	r128_drv

SRCS=	r128_accel.c r128_cursor.c r128_dga.c r128_driver.c \
	r128_video.c r128_misc.c r128_probe.c r128_dri.c

#SRCS+=	r128_exa.c r128_exa_render.c
#CPPFLAGS+=	-DUSE_EXA

MAN=	r128.4

.if ${MACHINE_ARCH} == "powerpc" || ${MACHINE} == "sparc64"
CPPFLAGS+= -DAVOID_FBDEV
.endif

# Enable VGA support (esp. hw state save/restore) on i386 / amd64 platforms
.if ${MACHINE} == "amd64" || ${MACHINE} == "i386"
CPPFLAGS+=	-DWITH_VGAHW
.endif

CPPFLAGS+=	-I${DESTDIR}${X11INCDIR}/libdrm

CPPFLAGS+=	-DHAVE_XEXTPROTO_71 \
		-DHAVE_XAA_H -DR128DRI

CWARNFLAGS.clang+=	-Wno-format -Wno-pointer-sign

.include "../Makefile.xf86-driver"
