# $NetBSD: Makefile,v 1.3 2008/08/24 13:47:05 rtr Exp $

XMODULEDIR=	${X11USRLIBDIR}/modules

.include "../../../Makefile.serverlib"
.include "../../../Makefile.servermod"

LIB=	xf4bpp

.PATH:	${X11SRCDIR.xorg-server}/hw/xfree86/${LIB}
SRCS=	ppcArea.c \
	ppcBStore.c \
	ppcClip.c \
	ppcCpArea.c \
	ppcCReduce.c \
	ppcDepth.c \
	ppcFillRct.c \
	ppcWinFS.c \
	ppcPixFS.c \
	ppcGC.c \
	ppcGetSp.c \
	ppcImg.c \
	ppcPixmap.c \
	ppcPntWin.c \
	ppcPolyPnt.c \
	ppcQuery.c \
	ppcRslvC.c \
	ppcSetSp.c \
	ppcWindow.c \
	ppcIO.c \
	emulOpStip.c \
	emulRepAre.c \
	emulTile.c \
	vgaGC.c \
	vgaBitBlt.c \
	vgaImages.c \
	vgaStipple.c \
	vgaSolid.c \
	offscreen.c \
	wm3.c \
	mfbimggblt.c \
	mfbline.c \
	mfbseg.c \
	mfbhrzvert.c \
	mfbbres.c \
	mfbbresd.c \
	mfbfillarc.c \
	mfbzerarc.c \
	vgamodule.c


CPPFLAGS+=	-I${X11SRCDIR.xorg-server}/hw/xfree86/${LIB} \
		-I${X11SRCDIR.xorg-server}/hw/xfree86/xf1bpp \
		-I${X11SRCDIR.xorg-server}/mfb \
		-I${X11SRCDIR.xorg-server}/cfb \
		-I${X11SRCDIR.xorg-server}/mi \
		-I${X11SRCDIR.xorg-server}/include \
		-I${X11SRCDIR.xorg-server}/hw/xfree86/os-support \
		-I${X11SRCDIR.xorg-server}/hw/xfree86/common \
		-I${DESTDIR}${X11INCDIR}/X11 \
		-I${DESTDIR}${X11INCDIR}/pixman-1 \
		-I${DESTDIR}${X11INCDIR}/xorg

CPPFLAGS+=	-DXF86VGA16

# XXX for ../../../../ files
CPPFLAGS+=	-I${X11SRCDIR.xorg-server}/hw/xfree86/dixmods/extmod

.include <bsd.x11.mk>
LIBDIR=	${XMODULEDIR}
.include <bsd.lib.mk>
