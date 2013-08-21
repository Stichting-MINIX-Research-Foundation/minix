# $NetBSD: Makefile,v 1.3 2008/08/24 13:47:05 rtr Exp $

XMODULEDIR=	${X11USRLIBDIR}/modules

.include "../../../Makefile.serverlib"
.include "../../../Makefile.servermod"

LIB=	xf8_32bpp

.PATH:	${X11SRCDIR.xorg-server}/hw/xfree86/${LIB}
SRCS=	cfbbstore.c \
	cfbcpyarea.c \
	cfbcpyplane.c \
	cfbgcmisc.c \
	cfbimage.c \
	cfbpntwin.c \
	cfbscrinit.c \
	cfbwindow.c \
	xf86overlay.c \
	cfb8_32module.c \
	cfbgc8.c \
	cfbgc32.c \
	cfbgcunder.c


CPPFLAGS+=	-I${X11SRCDIR.xorg-server}/hw/xfree86/${LIB} \
		-I${X11SRCDIR.xorg-server}/hw/xfree86/xf1bpp \
		-I${X11SRCDIR.xorg-server}/mfb \
		-I${X11SRCDIR.xorg-server}/cfb \
		-I${X11SRCDIR.xorg-server}/mi \
		-I${X11SRCDIR.xorg-server}/include \
		-I${X11SRCDIR.xorg-server}/hw/xfree86/os-support \
		-I${X11SRCDIR.xorg-server}/hw/xfree86/common \
		-I${DESTDIR}${X11INCDIR}/X11/extensions \
		-I${DESTDIR}${X11INCDIR}/X11 \
		-I${DESTDIR}${X11INCDIR}/pixman-1 \
		-I${DESTDIR}${X11INCDIR}/xorg \
		-I${X11SRCDIR.xorg-server}/Xext

# XXX for ../../../../ files
CPPFLAGS+=	-I${X11SRCDIR.xorg-server}/hw/xfree86/dixmods/extmod

.include <bsd.x11.mk>
LIBDIR=	${XMODULEDIR}
.include <bsd.lib.mk>
