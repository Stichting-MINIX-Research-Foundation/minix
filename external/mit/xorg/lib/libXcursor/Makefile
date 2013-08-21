#	$NetBSD: Makefile,v 1.4 2008/10/14 23:37:19 cube Exp $

.include <bsd.own.mk>

LIB=	Xcursor

.PATH:	${X11SRCDIR.${LIB}}/src
.PATH:	${X11SRCDIR.${LIB}}/include/X11/Xcursor
.PATH:	${X11SRCDIR.${LIB}}/man
SRCS=	cursor.c display.c file.c library.c xlib.c

INCS=	Xcursor.h
INCSDIR=${X11INCDIR}/X11/Xcursor

MAN=	Xcursor.3
X11EXTRAMANDEFS+=	-e 's%__XCURSORPATH__%~/.icons, \
			   /usr/share/icons, /usr/share/pixmaps, \
			   ${X11LIBDIR}/icons%g'

CPPFLAGS+=	${X11FLAGS.THREADLIB}
CPPFLAGS+=	-DICONDIR=\"${X11LIBDIR}/icons\"
CPPFLAGS+=	-I${DESTDIR}${X11INCDIR}/X11/Xcursor
CPPFLAGS+=	-I${X11SRCDIR.${LIB}}/../include

LIBDPLIBS=\
	Xrender	${.CURDIR}/../libXrender \
	Xext	${.CURDIR}/../libXext \
	X11	${.CURDIR}/../libX11/dynamic

PKGDIST=	${LIB}

.include <bsd.x11.mk>
.include <bsd.lib.mk>
