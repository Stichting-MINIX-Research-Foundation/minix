#	$NetBSD: Makefile,v 1.6 2013/05/31 09:33:18 mrg Exp $

.include <bsd.own.mk>

LIB=	Xxf86vm
.PATH:	${X11SRCDIR.${LIB}}/src
.PATH:	${X11SRCDIR.${LIB}}/man
.PATH:	${X11SRCDIR.${LIB}}/include/X11/extensions

SRCS=	XF86VMode.c

CPPFLAGS+=	-DHAVE__XEATDATAWORDS

MAN=	XF86VM.3
MLINKS=	XF86VM.3 XF86VidModeQueryExtension.3 \
	XF86VM.3 XF86VidModeQueryVersion.3 \
	XF86VM.3 XF86VidModeSetClientVersion.3 \
	XF86VM.3 XF86VidModeGetModeLine.3 \
	XF86VM.3 XF86VidModeGetAllModeLines.3 \
	XF86VM.3 XF86VidModeDeleteModeLine.3 \
	XF86VM.3 XF86VidModeModModeLine.3 \
	XF86VM.3 XF86VidModeValidateModeLine.3 \
	XF86VM.3 XF86VidModeSwitchMode.3 \
	XF86VM.3 XF86VidModeSwitchToMode.3 \
	XF86VM.3 XF86VidModeLockModeSwitch.3 \
	XF86VM.3 XF86VidModeGetMonitor.3 \
	XF86VM.3 XF86VidModeGetViewPort.3 \
	XF86VM.3 XF86VidModeSetViewPort.3 \
	XF86VM.3 XF86VidModeGetDotClocks.3 \
	XF86VM.3 XF86VidModeGetGamma.3 \
	XF86VM.3 XF86VidModeSetGamma.3 \
	XF86VM.3 XF86VidModeGetGammaRamp.3 \
	XF86VM.3 XF86VidModeSetGammaRamp.3 \
	XF86VM.3 XF86VidModeGetGammaRampSize.3 \
	XF86VM.3 XF86VidModeGetPermissions.3

LIBDPLIBS=	Xext	${.CURDIR}/../libXext \
		X11	${.CURDIR}/../libX11/dynamic

INCS=	xf86vmode.h

INCSDIR=${X11INCDIR}/X11/extensions

PKGDIST=	${LIB}

.include <bsd.x11.mk>
.include <bsd.lib.mk>
