#	$NetBSD: Makefile,v 1.4 2008/11/06 22:28:25 veego Exp $

.include <bsd.own.mk>

LIB=	Xcomposite

XCOMPOSITEDIR=	${X11SRCDIR.${LIB}}
.PATH:		${XCOMPOSITEDIR}/src
.PATH:		${XCOMPOSITEDIR}/man
.PATH:		${XCOMPOSITEDIR}/include/X11/extensions
SRCS=		Xcomposite.c

INCS=	Xcomposite.h
INCSDIR=${X11INCDIR}/X11/extensions

LIBDPLIBS=	X11	${.CURDIR}/../libX11/dynamic

MAN=	Xcomposite.3
MLINKS=	Xcomposite.3 XCompositeCreateRegionFromBorderClip.3 \
	Xcomposite.3 XCompositeGetOverlayWindow.3 \
	Xcomposite.3 XCompositeNameWindowPixmap.3 \
	Xcomposite.3 XCompositeQueryExtension.3 \
	Xcomposite.3 XCompositeQueryVersion.3 \
	Xcomposite.3 XCompositeRedirectSubwindows.3 \
	Xcomposite.3 XCompositeRedirectWindow.3 \
	Xcomposite.3 XCompositeReleaseOverlayWindow.3 \
	Xcomposite.3 XCompositeUnredirectSubwindows.3 \
	Xcomposite.3 XCompositeUnredirectWindow.3 \
	Xcomposite.3 XCompositeVersion.3

PKGDIST=	${LIB}

.include <bsd.x11.mk>
.include <bsd.lib.mk>
