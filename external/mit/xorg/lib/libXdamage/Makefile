#	$NetBSD: Makefile,v 1.3 2008/11/06 22:28:25 veego Exp $

.include <bsd.own.mk>

LIB=	Xdamage

XDAMAGEDIR=	${X11SRCDIR.${LIB}}
.PATH:		${XDAMAGEDIR}/src
.PATH:		${XDAMAGEDIR}/include/X11/extensions
SRCS=		Xdamage.c

CPPFLAGS+=	-I${DESTDIR}${X11INCDIR}/X11/extensions

INCS=	Xdamage.h
INCSDIR=${X11INCDIR}/X11/extensions

LIBDPLIBS=	X11	${.CURDIR}/../libX11/dynamic

PKGDIST=	${LIB}

.include <bsd.x11.mk>
.include <bsd.lib.mk>
