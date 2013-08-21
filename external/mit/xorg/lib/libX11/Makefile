#	$NetBSD: Makefile,v 1.11 2011/02/23 07:53:29 mrg Exp $

.include <bsd.own.mk>

SUBDIR=		dynamic .WAIT libX11-xcb static \
		ximcp xlcDef xlcUTF8Load xlibi18n xlocale xomGeneric

INCS=		ImUtil.h XKBlib.h Xcms.h Xlib-xcb.h Xlib.h XlibConf.h \
		Xlibint.h Xlocale.h Xregion.h Xresource.h Xutil.h \
		Xcmsint.h Xlcint.h \
		cursorfont.h
INCSDIR=	${X11INCDIR}/X11

MAN=		AllPlanes.3 BlackPixelOfScreen.3 DisplayOfCCC.3 \
		ImageByteOrder.3 IsCursorKey.3 XAddConnectionWatch.3 \
		XAddHost.3 XAllocClassHint.3 XAllocColor.3 \
		XAllocIconSize.3 XAllocSizeHints.3 \
		XAllocStandardColormap.3 XAllocWMHints.3 \
		XAllowEvents.3 XAnyEvent.3 XButtonEvent.3 \
		XChangeKeyboardControl.3 XChangeKeyboardMapping.3 \
		XChangePointerControl.3 XChangeSaveSet.3 \
		XChangeWindowAttributes.3 XCirculateEvent.3 \
		XCirculateRequestEvent.3 XClearArea.3 \
		XClientMessageEvent.3 XColormapEvent.3 \
		XConfigureEvent.3 XConfigureRequestEvent.3 \
		XConfigureWindow.3 XCopyArea.3 XCreateColormap.3 \
		XCreateFontCursor.3 XCreateFontSet.3 XCreateGC.3 \
		XCreateIC.3 XCreateOC.3 XCreatePixmap.3 \
		XCreateRegion.3 XCreateWindow.3 XCreateWindowEvent.3 \
		XCrossingEvent.3 XDefineCursor.3 XDestroyWindow.3 \
		XDestroyWindowEvent.3 XDrawArc.3 XDrawImageString.3 \
		XDrawLine.3 XDrawPoint.3 XDrawRectangle.3 \
		XDrawString.3 XDrawText.3 XEmptyRegion.3 \
		XErrorEvent.3 XExposeEvent.3 XExtentsOfFontSet.3 \
		XFillRectangle.3 XFilterEvent.3 XFlush.3 \
		XFocusChangeEvent.3 XFontSetExtents.3 \
		XFontsOfFontSet.3 XFree.3 XGetVisualInfo.3 \
		XGetWindowAttributes.3 XGetWindowProperty.3 \
		XGetXCBConnection.3 XGrabButton.3 XGrabKey.3 \
		XGrabKeyboard.3 XGrabPointer.3 XGrabServer.3 \
		XGraphicsExposeEvent.3 XGravityEvent.3 \
		XIconifyWindow.3 XIfEvent.3 XInitImage.3 \
		XInitThreads.3 XInstallColormap.3 XInternAtom.3 \
		XIntersectRegion.3 XKeymapEvent.3 XListFonts.3 \
		XLoadFont.3 XLookupKeysym.3 XMapEvent.3 \
		XMapRequestEvent.3 XMapWindow.3 XNextEvent.3 XNoOp.3 \
		XOpenDisplay.3 XOpenIM.3 XOpenOM.3 XParseGeometry.3 \
		XPolygonRegion.3 XPropertyEvent.3 XPutBackEvent.3 \
		XPutImage.3 XQueryBestSize.3 XQueryColor.3 \
		XQueryExtension.3 XQueryPointer.3 XQueryTree.3 \
		XRaiseWindow.3 XReadBitmapFile.3 XRecolorCursor.3 \
		XReparentEvent.3 XReparentWindow.3 \
		XResizeRequestEvent.3 XResourceManagerString.3 \
		XSaveContext.3 XSelectInput.3 XSelectionClearEvent.3 \
		XSelectionEvent.3 XSelectionRequestEvent.3 \
		XSendEvent.3 XSetArcMode.3 XSetClipOrigin.3 \
		XSetCloseDownMode.3 XSetCommand.3 XSetErrorHandler.3 \
		XSetEventQueueOwner.3 XSetFillStyle.3 XSetFont.3 \
		XSetFontPath.3 XSetICFocus.3 XSetICValues.3 \
		XSetInputFocus.3 XSetLineAttributes.3 \
		XSetPointerMapping.3 XSetScreenSaver.3 \
		XSetSelectionOwner.3 XSetState.3 XSetTextProperty.3 \
		XSetTile.3 XSetTransientForHint.3 \
		XSetWMClientMachine.3 XSetWMColormapWindows.3 \
		XSetWMIconName.3 XSetWMName.3 XSetWMProperties.3 \
		XSetWMProtocols.3 XStoreBytes.3 XStoreColors.3 \
		XStringListToTextProperty.3 XStringToKeysym.3 \
		XSupportsLocale.3 XSynchronize.3 XTextExtents.3 \
		XTextWidth.3 XTranslateCoordinates.3 XUnmapEvent.3 \
		XUnmapWindow.3 XVaCreateNestedList.3 \
		XVisibilityEvent.3 XWarpPointer.3 XcmsAllocColor.3 \
		XcmsCCCOfColormap.3 XcmsCIELabQueryMaxC.3 \
		XcmsCIELuvQueryMaxC.3 XcmsColor.3 XcmsConvertColors.3 \
		XcmsCreateCCC.3 XcmsDefaultCCC.3 XcmsQueryBlack.3 \
		XcmsQueryColor.3 XcmsSetWhitePoint.3 XcmsStoreColor.3 \
		XcmsTekHVCQueryMaxC.3 XmbDrawImageString.3 \
		XmbDrawString.3 XmbDrawText.3 XmbLookupString.3 \
		XmbResetIC.3 XmbTextEscapement.3 XmbTextExtents.3 \
		XmbTextListToTextProperty.3 XmbTextPerCharExtents.3 \
		XrmEnumerateDatabase.3 XrmGetFileDatabase.3 \
		XrmGetResource.3 XrmInitialize.3 XrmMergeDatabases.3 \
		XrmPutResource.3 XrmUniqueQuark.3
# XGetEventData.man

USETBL=		yes

FILES=		XErrorDB XKeysymDB Xcms.txt
FILESDIR=	${X11LIBDIR}

.PATH:		${X11SRCDIR.X11}
.PATH:		${X11SRCDIR.X11}/include/X11
.PATH:		${X11SRCDIR.X11}/man
.PATH:		${X11SRCDIR.X11}/src
.PATH:		${X11SRCDIR.X11}/src/xcms
.PATH:		${X11SRCDIR.X11}/src/xlibi18n

XlibConf.h: XlibConf.h.in
	${_MKTARGET_CREATE}
.if !defined(__MINIX)
	${TOOL_SED} 's/#undef XTHREADS/#define XTHREADS 1/; \
		    s/#undef XUSE_MTSAFE_API/#define XUSE_MTSAFE_API 1/ ' \
		< ${.ALLSRC} > ${.TARGET}
.else
	${TOOL_SED} 's/#undef XTHREADS/#define XTHREADS 1/;' \
		${.ALLSRC} > ${.TARGET}
.endif

CLEANFILES+=	XlibConf.h

PKGDIST=	X11

.include <bsd.x11.mk>
.include <bsd.prog.mk>
.include <bsd.subdir.mk>
