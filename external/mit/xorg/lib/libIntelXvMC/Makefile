#	$NetBSD: Makefile,v 1.4 2014/04/21 21:57:03 christos Exp $

.include <bsd.own.mk>

LIB=	IntelXvMC
MKLINT=	no
.PATH:	${X11SRCDIR.xf86-video-intel}/xvmc

SRCS+=		intel_xvmc.c
SRCS+=		intel_xvmc_dump.c
SRCS+=		i915_xvmc.c
SRCS+=		i965_xvmc.c
SRCS+=		xvmc_vld.c
SRCS+=		intel_batchbuffer.c

CPPFLAGS+=	${X11FLAGS.THREADLIB} \
		-DTRUE=1 -DFALSE=0
CPPFLAGS+=	-I${DESTDIR}${X11INCDIR}/libdrm \
		-I${DESTDIR}${X11INCDIR}/xorg \
		-I${X11SRCDIR.xf86-video-intel}/src

LIBDPLIBS=	Xext		${.CURDIR}/../libXext \
		X11		${.CURDIR}/../libX11/dynamic \
		Xv		${.CURDIR}/../libXv \
		XvMC		${.CURDIR}/../libXvMC \
		drm		${.CURDIR}/../libdrm \
		drm_intel	${.CURDIR}/../libdrm_intel \
		xcb		${.CURDIR}/../libxcb/libxcb

CWARNFLAGS.clang+=	-Wno-pointer-sign -Wno-format -Wno-parentheses

.include <bsd.x11.mk>
.include <bsd.lib.mk>
