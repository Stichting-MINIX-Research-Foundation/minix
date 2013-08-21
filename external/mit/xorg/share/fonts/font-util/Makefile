#	$NetBSD: Makefile,v 1.3 2010/11/21 01:25:33 mrg Exp $

.include <bsd.own.mk>

INCSDIR=${X11FONTDIR}/util
INCS=	map-ISO8859-1 map-ISO8859-2 map-ISO8859-3 map-ISO8859-4 \
	map-ISO8859-5 map-ISO8859-6 map-ISO8859-7 map-ISO8859-8 \
	map-ISO8859-9 map-ISO8859-10 map-ISO8859-11 map-ISO8859-13 \
	map-ISO8859-14 map-ISO8859-15 map-ISO8859-16 map-JISX0201.1976-0 \
	map-KOI8-R

.include "../Makefile.font"

PKGCONFIG=	fontutil
PKGDIST=	font-util

.include <bsd.x11.mk>
.include <bsd.prog.mk>

.PATH:  ${X11SRCDIR.font-util} ${X11SRCDIR.font-util}/man
