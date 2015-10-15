#	$NetBSD: Makefile,v 1.3 2015/07/23 08:03:25 mrg Exp $

.include <bsd.own.mk>

LIB=	Xrender

XRENDERDIR=	${X11SRCDIR.${LIB}}
.PATH:		${XRENDERDIR}/src
.PATH:		${XRENDERDIR}/include/X11/extensions
SRCS=		Xrender.c AddTrap.c Color.c Composite.c Cursor.c \
		Glyph.c Picture.c FillRect.c FillRects.c Filter.c \
		Poly.c Trap.c Tri.c

INCS=	Xrender.h
INCSDIR=${X11INCDIR}/X11/extensions

CPPFLAGS+=	${X11FLAGS.THREADLIB}
CPPFLAGS+=	-I${DESTDIR}${X11INCDIR}/X11/extensions
CPPFLAGS+=	-I${XRENDERDIR}/../include

LIBDPLIBS=\
	Xext	${.CURDIR}/../libXext \
	X11	${.CURDIR}/../libX11/dynamic

PKGDIST=	${LIB}

.include <bsd.x11.mk>
.include <bsd.lib.mk>
