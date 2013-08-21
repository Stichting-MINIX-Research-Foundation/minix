#	$NetBSD: Makefile,v 1.5 2013/05/31 09:33:17 mrg Exp $

.include <bsd.own.mk>

LIB=	Xtst
.PATH:	${X11SRCDIR.${LIB}}/src
.PATH:	${X11SRCDIR.${LIB}}/man
.PATH:	${X11SRCDIR.${LIB}}/include/X11/extensions

SRCS=	XTest.c XRecord.c

CPPFLAGS+=	-DHAVE__XEATDATAWORDS

CPPFLAGS+=	${X11FLAGS.THREADLIB}

LIBDPLIBS=\
	Xext	${.CURDIR}/../libXext \
	X11	${.CURDIR}/../libX11/dynamic

INCS=	XTest.h \
	record.h

INCSDIR=${X11INCDIR}/X11/extensions

PKGDIST=	${LIB}

.include <bsd.x11.mk>
.include <bsd.lib.mk>
