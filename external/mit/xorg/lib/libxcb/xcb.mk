#	$NetBSD: xcb.mk,v 1.4 2015/02/03 21:31:31 jmcneill Exp $

# define XCBEXT to something before including this

LIB=	xcb-${XCBEXT}

SRCS=	${XCBEXT}.c

CPPFLAGS+=	-I${X11SRCDIR.xcb}/src
CPPFLAGS+=	-I${.CURDIR}/../files

LIBDPLIBS=\
	xcb	${.CURDIR}/../libxcb \
	Xau	${.CURDIR}/../../libXau \
	Xdmcp	${.CURDIR}/../../libXdmcp

SHLIB_MAJOR?=	0
SHLIB_MINOR?=	1

PKGCONFIG=	xcb-${XCBEXT}
PKGCONFIG_VERSION.${PKGCONFIG}=     1.10

.include <bsd.x11.mk>
.include <bsd.lib.mk>

.PATH: ${.CURDIR}/../files ${X11SRCDIR.xcb}
