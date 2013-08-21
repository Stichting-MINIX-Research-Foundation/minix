#	$NetBSD: Makefile,v 1.5 2009/11/09 03:27:05 mrg Exp $

.include <bsd.own.mk>

LIB=	Xinerama
.PATH:	${X11SRCDIR.${LIB}}/src
.PATH:	${X11SRCDIR.${LIB}}/man
.PATH:	${X11SRCDIR.${LIB}}/include/X11/extensions

SRCS=	Xinerama.c

CPPFLAGS+=	${X11FLAGS.THREADLIB}

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
