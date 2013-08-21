#	$NetBSD: Makefile,v 1.2 2011/01/19 01:52:44 jakllsch Exp $

.include <bsd.own.mk>

LIB=	I810XvMC
.PATH:	${X11SRCDIR.xf86-video-intel}/src/xvmc

SRCS=	I810XvMC.c

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

.include <bsd.x11.mk>
.include <bsd.lib.mk>
