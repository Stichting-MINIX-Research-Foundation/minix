#	$NetBSD: Makefile,v 1.3 2013/06/02 03:15:49 christos Exp $

.include <bsd.own.mk>

LIB=	chromeXvMC
.PATH:	${X11SRCDIR.xf86-video-openchrome}/libxvmc
.PATH:	${X11SRCDIR.xf86-video-openchrome}/src
.PATH:	${X11SRCDIR.xf86-video-openchrome}/src/xvmc

SRCS=	viaLowLevel.c \
	driDrawable.c \
	viaXvMC.c \
	xf86dri.c

CPPFLAGS+=	${X11FLAGS.THREADLIB} \
		-DHAVE_XEXTPROTO_71 \
		-DXSERVER_LIBPCIACCESS \
		-DTRUE=1 -DFALSE=0
CPPFLAGS+=	-I${DESTDIR}${X11INCDIR}/libdrm \
		-I${DESTDIR}${X11INCDIR}/xorg \
		-I${X11SRCDIR.xf86-video-openchrome}/src

LIBDPLIBS=	Xext	${.CURDIR}/../libXext \
		X11	${.CURDIR}/../libX11/dynamic \
		Xv	${.CURDIR}/../libXv \
		XvMC	${.CURDIR}/../libXvMC \
		drm	${.CURDIR}/../libdrm

.include <bsd.x11.mk>
.include <bsd.lib.mk>
