#	$NetBSD: Makefile,v 1.3 2008/11/06 22:28:26 veego Exp $

.include <bsd.own.mk>

LIB=	xkbui
.PATH:	${X11SRCDIR.${LIB}}/src
.PATH:	${X11SRCDIR.${LIB}}/include/X11/extensions

SRCS=	XKBui.c

INCS=	XKBui.h
INCSDIR=${X11INCDIR}/X11/extensions

CPPFLAGS+=-I${DESTDIR}${X11INCDIR}/X11/extensions

LIBDPLIBS=	X11	${.CURDIR}/../libX11/dynamic \
		xkbfile	${.CURDIR}/../libxkbfile \
		m	${.CURDIR}/../../../../../lib/libm

PKGCONFIG=	xkbui

.include <bsd.x11.mk>
.include <bsd.lib.mk>
