#	$NetBSD: Makefile,v 1.3 2012/01/07 02:07:59 mrg Exp $

XCBUTIL=	render-util

SRCS=	cache.c glyph.c util.c

XCBUTIL_EXTRA_DPLIBS+=\
	xcb-render	${.CURDIR}/../../libxcb/render

# xxx render-util vs renderutil
PKGCONFIG=	xcb-renderutil
.include "../xcb-util.mk"

# xxx render-util vs renderutil
CPPFLAGS+=	-I${X11SRCDIR.xcb-util}/renderutil
.PATH: ${X11SRCDIR.xcb-util}/renderutil

CWARNFLAGS.clang+=	-Wno-tautological-compare
