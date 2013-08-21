# $NetBSD: Makefile,v 1.3 2008/08/24 13:47:05 rtr Exp $

XMODULEDIR=	${X11USRLIBDIR}/modules

.include "../../../Makefile.serverlib"
.include "../../../Makefile.servermod"

LIB=	xf1bpp

.PATH:	${X11SRCDIR.xorg-server}/hw/xfree86/${LIB}
SRCS=	mfbseg.c \
	mfbpgbwht.c \
	mfbpgbblak.c \
	mfbpgbinv.c \
	mfbigbwht.c \
	mfbigbblak.c \
	mfbpawhite.c \
	mfbpablack.c \
	mfbpainv.c \
	mfbtewhite.c \
	mfbteblack.c \
	mfbplywhite.c \
	mfbplyblack.c \
	mfbplyinv.c \
	mfbbltC.c \
	mfbbltX.c \
	mfbbltCI.c \
	mfbbltO.c \
	mfbbltG.c \
	mfbtileC.c \
	mfbtileG.c

SRCS+=	maskbits.c \
	mfbbitblt.c \
	mfbbres.c \
	mfbbresd.c \
	mfbbstore.c \
	mfbclip.c \
	mfbcmap.c \
	mfbfillarc.c \
	mfbfillrct.c \
	mfbfillsp.c \
	mfbfont.c \
	mfbgc.c \
	mfbgetsp.c \
	mfbhrzvert.c \
	mfbimage.c \
	mfbline.c \
	mfbmisc.c \
	mfbpixmap.c \
	mfbpntwin.c \
	mfbpolypnt.c \
	mfbpushpxl.c \
	mfbscrclse.c \
	mfbscrinit.c \
	mfbsetsp.c \
	mfbwindow.c \
	mfbzerarc.c

CPPFLAGS+=	-I${X11SRCDIR.xorg-server}/mfb \
		-I${X11SRCDIR.xorg-server}/mi \
                -I${X11SRCDIR.xorg-server}/include \
                -I${X11SRCDIR.xorg-server}/hw/xfree86/${LIB} \
                -I${X11SRCDIR.xorg-server}/hw/xfree86/common \
                -I${X11SRCDIR.xorg-server}/hw/xfree86 \
                -I${DESTDIR}${X11INCDIR}/X11 \
                -I${DESTDIR}${X11INCDIR}/pixman-1 \
                -I${DESTDIR}${X11INCDIR}/xorg

# XXX for ../../../../ files
CPPFLAGS+=	-I${X11SRCDIR.xorg-server}/hw/xfree86/dixmods/extmod

COPTS.mfbclip.c=	-Wno-error
COPTS.mfbfont.c=	-Wno-error
COPTS.mfbmisc.c=	-Wno-error
COPTS.mfbpushpxl.c=	-Wno-error

.include <bsd.x11.mk>
LIBDIR=	${XMODULEDIR}
.include <bsd.lib.mk>
