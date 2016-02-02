# $NetBSD: Makefile,v 1.8 2015/07/18 08:56:06 mrg Exp $

XMODULEDIR=	${X11USRLIBDIR}/modules/extensions
XMODULEDIRANDSTATIC=	# defined

.include "../../../Makefile.serverlib"
.include "../../../Makefile.servermod"

LIB=	dri2

.PATH:	${X11SRCDIR.xorg-server}/hw/xfree86/${LIB}
SRCS=	dri2.c dri2ext.c

CPPFLAGS+=	-I${X11SRCDIR.xorg-server}/hw/xfree86/${LIB} \
		-I${X11SRCDIR.xorg-server}/include \
		-I${X11SRCDIR.xorg-server}/hw/xfree86/common \
		-I${X11SRCDIR.xorg-server}/hw/xfree86/os-support \
		-I${X11SRCDIR.xorg-server}/hw/xfree86/os-support/bus \
		-I${X11SRCDIR.xorg-server}/xfixes \
		-I${X11SRCDIR.xorg-server}/glx \
		-I${X11SRCDIR.xorg-server}/GL/include \
		-I${X11SRCDIR.xorg-server}/modes \
		-I${X11SRCDIR.xorg-server}/ddc \
		-I${X11SRCDIR.xorg-server}/i2c \
		-I${X11SRCDIR.xorg-server}/parser \
		-I${X11SRCDIR.xorg-server}/ramdac \
		-I${X11SRCDIR.xorg-server}/Xext \
		-I${X11SRCDIR.MesaLib}/include \
		-I${DESTDIR}${X11INCDIR}/X11 \
		-I${DESTDIR}${X11INCDIR}/X11/dri \
		-I${DESTDIR}${X11INCDIR}/libdrm \
		-I${DESTDIR}${X11INCDIR}/pixman-1 \
		-I${DESTDIR}${X11INCDIR}/xorg \
		-I${X11SRCDIR.xorg-server}/../include

CPPFLAGS+=	-DXF86DRI \
		-DCOMPOSITE \
		${X11FLAGS.DIX}

LIBDPLIBS=	drm ${.CURDIR}/../../../../../lib/libdrm

COPTS.dri2ext.c+=	-Wno-error

.include <bsd.x11.mk>
LIBDIR=	${XMODULEDIR}
.include <bsd.lib.mk>
