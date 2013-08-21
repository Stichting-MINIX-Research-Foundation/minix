#	$NetBSD: Makefile,v 1.2 2012/01/07 02:07:58 mrg Exp $

XCBUTIL=	property

SRCS=	property.c

SHLIB_MAJOR=	1

XCBUTIL_EXTRA_DPLIBS+=\
	xcb-event	${.CURDIR}/../event

.include "../xcb-util.mk"

CPPFLAGS+=	-I${X11SRCDIR.xcb-util}/event
