#	$NetBSD: Makefile,v 1.2 2012/01/07 02:07:58 mrg Exp $

XCBUTIL=	icccm

SRCS=	icccm.c

SHLIB_MAJOR=	1

XCBUTIL_EXTRA_DPLIBS+=\
	xcb-atom	${.CURDIR}/../atom \
	xcb-property	${.CURDIR}/../property \
	xcb-event	${.CURDIR}/../event

.include "../xcb-util.mk"

CPPFLAGS+=	-I${X11SRCDIR.xcb-util}/atom
CPPFLAGS+=	-I${X11SRCDIR.xcb-util}/property
CPPFLAGS+=	-I${X11SRCDIR.xcb-util}/event
