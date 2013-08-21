#	$NetBSD: Makefile,v 1.5 2010/05/22 09:22:15 mrg Exp $

.include <bsd.own.mk>

LIB=	Xss
.PATH:	${X11SRCDIR.XScrnSaver}/src
.PATH:	${X11SRCDIR.XScrnSaver}/man
.PATH:	${X11SRCDIR.XScrnSaver}/include/X11/extensions

SRCS=	XScrnSaver.c

CPPFLAGS+=	${X11FLAGS.THREADLIB}
#CPPFLAGS+=	-I${DESTDIR}${X11INCDIR}

LIBDPLIBS=	X11	${.CURDIR}/../libX11/dynamic \
		Xext	${.CURDIR}/../libXext

MAN=	Xss.3
MLINKS=	Xss.3 XScreenSaverAllocInfo.3 \
	Xss.3 XScreenSaverGetRegistered.3 \
	Xss.3 XScreenSaverQueryExtension.3 \
	Xss.3 XScreenSaverQueryInfo.3 \
	Xss.3 XScreenSaverQueryVersion.3 \
	Xss.3 XScreenSaverRegister.3 \
	Xss.3 XScreenSaverSelectInput.3 \
	Xss.3 XScreenSaverSetAttributes.3 \
	Xss.3 XScreenSaverSuspend.3 \
	Xss.3 XScreenSaverUnregister.3 \
	Xss.3 XScreenSaverUnsetAttributes.3

PKGDIST=	XScrnSaver

INCS=	scrnsaver.h
INCSDIR=${X11INCDIR}/X11/extensions

.include <bsd.x11.mk>
.include <bsd.lib.mk>
