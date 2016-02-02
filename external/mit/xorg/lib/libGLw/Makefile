#	$NetBSD: Makefile,v 1.4 2014/12/18 06:24:30 mrg Exp $

.include <bsd.own.mk>

LIB=	GLw
.PATH:	${X11SRCDIR.glw}

SRCS=	GLwDrawA.c

CPPFLAGS+=	${X11FLAGS.THREADLIB}

# Motif stubs for glwMDrawingAreaClass are enabled by setting MKX11MOTIF
# in /etc/mk.conf.  If Motif is not installed in the default location
# (/usr/pkg), set X11MOTIFPATH as well.

.if "${MKX11MOTIF}" == "yes"
X11MOTIFPATH?=	/usr/pkg
SRCS+=		GLwMDrawA.c
CPPFLAGS+=	-I${X11MOTIFPATH}/include
.endif

.PATH:		${X11SRCDIR.glw}
INCS=	GLwDrawA.h GLwDrawAP.h GLwMDrawA.h GLwMDrawAP.h
INCSDIR=${X11INCDIR}/GL

LIBDPLIBS=	Xext		${.CURDIR}/../libXext \
		X11		${.CURDIR}/../libX11/dynamic \
		Xxf86vm		${.CURDIR}/../libXxf86vm \
		Xfixes		${.CURDIR}/../libXfixes \
		Xdamage		${.CURDIR}/../libXdamage \
		m		${.CURDIR}/../../../../../lib/libm \
		drm		${.CURDIR}/../libdrm \
		GL		${.CURDIR}/../libGL \
		ICE		${.CURDIR}/../libICE \
		SM		${.CURDIR}/../libSM \
		Xt		${.CURDIR}/../libXt \
		xcb-dri2	${.CURDIR}/../libxcb/dri2

PKGCONFIG=	glw
PKGDIST.glw=	${X11SRCDIR.MesaLib}/src/glw
PKGCONFIG_VERSION.glw=	1.0.0

.include <bsd.x11.mk>
.include <bsd.lib.mk>
