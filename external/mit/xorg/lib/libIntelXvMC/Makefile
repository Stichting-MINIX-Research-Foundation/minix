#	$NetBSD: Makefile,v 1.2 2011/08/11 23:15:38 joerg Exp $

.include <bsd.own.mk>

LIB=	IntelXvMC
#MKLINT=	no
.PATH:	${X11SRCDIR.xf86-video-intel}/src/xvmc

SRCS=	intel_xvmc.c \
	intel_xvmc_dump.c \
	i915_xvmc.c \
	i965_xvmc.c \
	intel_batchbuffer.c \
	xf86dri.c

CPPFLAGS+=	${X11FLAGS.THREADLIB} \
		-DHAVE_XEXTPROTO_71 \
		-DXSERVER_LIBPCIACCESS \
		-DTRUE=1 -DFALSE=0
CPPFLAGS+=	-I${DESTDIR}${X11INCDIR}/libdrm \
		-I${DESTDIR}${X11INCDIR}/xorg \
		-I${X11SRCDIR.xf86-video-intel}/src

LIBDPLIBS=	Xext	${.CURDIR}/../libXext \
		X11	${.CURDIR}/../libX11/dynamic \
		Xv	${.CURDIR}/../libXv \
		XvMC	${.CURDIR}/../libXvMC \
		drm	${.CURDIR}/../libdrm

CWARNFLAGS.clang+=	-Wno-pointer-sign -Wno-format -Wno-parentheses

.include <bsd.x11.mk>
.include <bsd.lib.mk>
