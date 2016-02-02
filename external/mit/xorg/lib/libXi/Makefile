#	$NetBSD: Makefile,v 1.13 2014/06/14 20:49:37 mrg Exp $

.include <bsd.own.mk>

LIB=	Xi
.PATH:	${X11SRCDIR.${LIB}}/src
.PATH:	${X11SRCDIR.${LIB}}/include/X11/extensions

SRCS=	XIAllowEvents.c \
	XIBarrier.c \
	XIGrabDevice.c \
	XIQueryVersion.c \
	XIQueryDevice.c \
	XISetDevFocus.c \
	XIGetDevFocus.c \
	XIPassiveGrab.c \
	XIProperties.c \
	XISelEv.c \
	XISetCPtr.c \
	XIWarpPointer.c \
	XIHierarchy.c \
	XIDefineCursor.c \
	XIQueryPointer.c
SRCS+=	XAllowDv.c \
	XChDProp.c \
	XChgDCtl.c \
	XChgFCtl.c \
	XChgKbd.c \
	XChgKMap.c \
	XChgPnt.c \
	XChgProp.c \
	XCloseDev.c \
	XDelDProp.c \
	XDevBell.c \
	XExtToWire.c \
	XGetBMap.c \
	XGetCPtr.c \
	XGetDCtl.c \
	XGetDProp.c \
	XGetFCtl.c \
	XGetKMap.c \
	XGetMMap.c \
	XGetProp.c \
	XGetVers.c \
	XGMotion.c \
	XGrabDev.c \
	XGrDvBut.c \
	XGrDvKey.c \
	XGtFocus.c \
	XGtSelect.c \
	XListDev.c \
	XListDProp.c \
	XOpenDev.c \
	XQueryDv.c \
	XSelect.c \
	XSetBMap.c \
	XSetDVal.c \
	XSetMMap.c \
	XSetMode.c \
	XSndExEv.c \
	XStFocus.c \
	XUngrDev.c \
	XUngrDvB.c \
	XUngrDvK.c \
	XExtInt.c

CPPFLAGS+=	${X11FLAGS.THREADLIB}
CPPFLAGS+=	-DHAVE__XEATDATAWORDS

LIBDPLIBS=\
	Xext	${.CURDIR}/../libXext \
	X11	${.CURDIR}/../libX11/dynamic

INCS=	XInput.h \
	XInput2.h

INCSDIR=${X11INCDIR}/X11/extensions

PKGDIST=	${LIB}

COPTS.XExtInt.c+=	-Wno-error	# XXX
COPTS.XSndExEv.c+=	-Wno-error	# XXX

CWARNFLAGS.clang+=	-Wno-pointer-sign

# XXX manuals

.include <bsd.x11.mk>
.include <bsd.lib.mk>
