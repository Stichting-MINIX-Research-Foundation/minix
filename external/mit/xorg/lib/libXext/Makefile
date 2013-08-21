#	$NetBSD: Makefile,v 1.8 2013/06/01 07:53:11 mrg Exp $

.include <bsd.own.mk>

LIB=	Xext
.PATH:	${X11SRCDIR.${LIB}}/src
.PATH:	${X11SRCDIR.${LIB}}/man
.PATH:	${X11SRCDIR.${LIB}}/include/X11/extensions/

SRCS=	DPMS.c \
	MITMisc.c \
	XAppgroup.c \
	XEVI.c \
	XLbx.c \
	XMultibuf.c \
	XSecurity.c \
	XShape.c \
	XShm.c \
	XSync.c \
	XTestExt1.c \
	Xcup.c \
	Xdbe.c \
	Xge.c \
	extutil.c \
	globals.c

CPPFLAGS+=	${X11FLAGS.THREADLIB}
CPPFLAGS+=	-DHAVE__XEATDATAWORDS

LIBDPLIBS=	X11	${.CURDIR}/../libX11/dynamic

# XXX
COPTS.extutil.c+=	-Wno-error

MAN=	DBE.3 DPMSCapable.3 DPMSDisable.3 DPMSEnable.3 DPMSForceLevel.3 \
	DPMSGetTimeouts.3 DPMSGetVersion.3 DPMSInfo.3 DPMSQueryExtension.3 \
	DPMSSetTimeouts.3 XShape.3 XShm.3 XcupGetReservedColormapEntries.3 \
	XcupQueryVersion.3 XcupStoreColors.3 XdbeAllocateBackBufferName.3 \
	XdbeBeginIdiom.3 XdbeDeallocateBackBufferName.3 XdbeEndIdiom.3 \
	XdbeFreeVisualInfo.3 XdbeGetBackBufferAttributes.3 \
	XdbeGetVisualInfo.3 XdbeQueryExtension.3 XdbeSwapBuffers.3 Xevi.3 \
	Xmbuf.3

MLINKS=		XShape.3 XShapeQueryExtensionXShapeQueryVersion.3 \
		XShape.3 XShapeCombineRegion.3 \
		XShape.3 XShapeCombineRectangles.3 \
		XShape.3 XShapeCombineMask.3 \
		XShape.3 XShapeCombineShape.3 \
		XShape.3 XShapeOffsetShape.3 \
		XShape.3 XShapeQueryExtents.3 \
		XShape.3 XShapeSelectInput.3 \
		XShape.3 XShapeInputSelected.3 \
		XShape.3 XShapeGetRectangles.3

MLINKS+=	XShm.3 XShmQueryExtension.3 \
		XShm.3 XShmQueryVersion.3 \
		XShm.3 XShmPixmapFormat.3 \
		XShm.3 XShmAttach.3 \
		XShm.3 XShmDetach.3 \
		XShm.3 XShmCreateImage.3 \
		XShm.3 XShmPutImage.3 \
		XShm.3 XShmGetImage.3 \
		XShm.3 XShmCreatePixmap.3 \
		XShm.3 XShmGetEventBase.3

MLINKS+=	Xmbuf.3 XmbufQueryExtension.3 \
		Xmbuf.3 XmbufGetVersion.3 \
		Xmbuf.3 XmbufCreateBuffers.3 \
		Xmbuf.3 XmbufDestroyBuffers.3 \
		Xmbuf.3 XmbufDisplayBuffers.3 \
		Xmbuf.3 XmbufGetWindowAttributes.3 \
		Xmbuf.3 XmbufChangeWindowAttributes.3 \
		Xmbuf.3 XmbufGetBufferAttributes.3 \
		Xmbuf.3 XmbufChangeBufferAttributes.3 \
		Xmbuf.3 XmbufGetScreenInfo.3 \
		Xmbuf.3 XmbufCreateStereoWindow.3

MLINKS+=	Xevi.3 XeviQueryVersion.3 \
		Xevi.3 XeviGetVisualInfo.3 \
		Xevi.3 XeviQueryExtension.3

INCS=	MITMisc.h \
	XEVI.h \
	XLbx.h \
	XShm.h \
	Xag.h \
	Xcup.h \
	Xdbe.h \
	Xext.h \
	Xge.h \
	dpms.h \
	extutil.h \
	multibuf.h \
	security.h \
	shape.h \
	sync.h \
	xtestext1.h

INCSDIR=${X11INCDIR}/X11/extensions

PKGDIST=	${LIB}

.include <bsd.x11.mk>
.include <bsd.lib.mk>
