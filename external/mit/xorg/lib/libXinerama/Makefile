#	$NetBSD: Makefile,v 1.6 2015/07/22 02:23:20 mrg Exp $

.include <bsd.own.mk>

LIB=	Xinerama
.PATH:	${X11SRCDIR.${LIB}}/src
.PATH:	${X11SRCDIR.${LIB}}/man
.PATH:	${X11SRCDIR.${LIB}}/include/X11/extensions

SRCS=	Xinerama.c

CPPFLAGS+=	${X11FLAGS.THREADLIB}
CPPFLAGS+=	-DHAVE__XEATDATAWORDS

MAN=	Xinerama.3
MLINKS=	Xinerama.3 XineramaQueryExtension.3 \
	Xinerama.3 XineramaQueryVersion.3 \
	Xinerama.3 XineramaIsActive.3 \
	Xinerama.3 XineramaQueryScreens.3

LIBDPLIBS=	Xext	${.CURDIR}/../libXext \
		X11	${.CURDIR}/../libX11/dynamic

PKGDIST=	${LIB}

INCS=	Xinerama.h panoramiXext.h
INCSDIR=${X11INCDIR}/X11/extensions

NOOBJ=	# defined

.include <bsd.x11.mk>
.include <bsd.lib.mk>
