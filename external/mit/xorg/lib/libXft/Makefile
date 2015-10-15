#	$NetBSD: Makefile,v 1.7 2014/11/21 01:11:50 joerg Exp $

.include <bsd.own.mk>

LIB=	Xft

.PATH:	${X11SRCDIR.${LIB}}/src
.PATH:	${X11SRCDIR.${LIB}}/include/X11/Xft
.PATH:	${X11SRCDIR.${LIB}}/man
SRCS=	xftcolor.c xftcore.c xftdbg.c xftdpy.c xftdraw.c xftextent.c \
	xftfont.c xftfreetype.c xftglyphs.c xftinit.c xftlist.c \
	xftname.c xftrender.c xftstr.c xftswap.c xftxlfd.c

INCS=	Xft.h XftCompat.h
INCSDIR=${X11INCDIR}/X11/Xft

MAN=	Xft.3

CPPFLAGS+=	${X11FLAGS.THREADLIB}
CPPFLAGS+=	-DFREETYPE2 -I${DESTDIR}${X11INCDIR}/freetype2 -DXFREE86_FT2
CPPFLAGS+=	-I${DESTDIR}${X11INCDIR}/X11/Xft

.if !empty(MACHINE_ARCH:Mearm*) && ${HAVE_GCC:U} == 48
# Workaround a crash in XftGlyphSpecRender with GCC 4.8.4
COPTS.xftrender.c+= ${${ACTIVE_CC} == "gcc" :? -O0 :}
.endif

LIBDPLIBS=	X11		${.CURDIR}/../libX11/dynamic \
		fontconfig	${.CURDIR}/../fontconfig/src \
		freetype	${.CURDIR}/../freetype \
		Xrandr		${.CURDIR}/../libXrandr \
		Xrender		${.CURDIR}/../libXrender

PKGDIST=	${LIB}

.include <bsd.x11.mk>
.include <bsd.lib.mk>
