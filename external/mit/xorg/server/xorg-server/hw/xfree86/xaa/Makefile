# $NetBSD: Makefile,v 1.7 2011/08/02 09:11:38 mrg Exp $

XMODULEDIR=	${X11USRLIBDIR}/modules

.include "../../../Makefile.serverlib"
.include "../../../Makefile.servermod"

LIB=	xaa

.PATH:	${X11SRCDIR.xorg-server}/hw/xfree86/${LIB}

SRCS=	xaaInit.c xaaGC.c xaaInitAccel.c xaaFallback.c \
	xaaBitBlt.c xaaCpyArea.c xaaGCmisc.c xaaCpyWin.c \
	xaaCpyPlane.c xaaFillRect.c xaaTEText.c xaaNonTEText.c \
	xaaPCache.c xaaSpans.c xaaROP.c xaaImage.c \
	xaaRect.c xaaLineMisc.c xaaBitOrder.c \
	xaaFillPoly.c xaaWideLine.c xaaTables.c xaaFillArc.c \
	xaaLine.c xaaDashLine.c xaaOverlay.c xaaOffscreen.c \
	xaaOverlayDF.c xaaStateChange.c xaaPict.c

SRCS+=	l-xaaBitmap.c l-xaaStipple.c l-xaaTEGlyph.c \
	l3-xaaBitmap.c l3-xaaStipple.c \
	m-xaaBitmap.c m-xaaStipple.c m-xaaTEGlyph.c \
	m3-xaaBitmap.c m3-xaaStipple.c \
	lf-xaaBitmap.c lf-xaaStipple.c lf-xaaTEGlyph.c \
	lf3-xaaBitmap.c lf3-xaaStipple.c \
	mf-xaaBitmap.c mf-xaaStipple.c mf-xaaTEGlyph.c \
	mf3-xaaBitmap.c mf3-xaaStipple.c \
	s-xaaLine.c s-xaaDashLine.c \

CPPFLAGS+=	-I${DESTDIR}${X11INCDIR}/X11 \
		-I${DESTDIR}${X11INCDIR}/X11/extensions \
		-I${DESTDIR}${X11INCDIR}/xorg \
		-I${DESTDIR}${X11INCDIR}/pixman-1 \
		-I${X11SRCDIR.xorg-server}/Xext \
		-I${X11SRCDIR.xorg-server}/hw/xfree86/${LIB} \
		-I${X11SRCDIR.xorg-server}/hw/xfree86/common \
		-I${X11SRCDIR.xorg-server}/hw/xfree86/os-support \
		-I${X11SRCDIR.xorg-server}/include \
		-I${X11SRCDIR.xorg-server}/include/extensions \
		-I${X11SRCDIR.xorg-server}/mi \
		-I${X11SRCDIR.xorg-server}/miext/cw \
		-I${X11SRCDIR.xorg-server}/render \
		-I${X11SRCDIR.xorg-server}/../include

# XXX to find files lokoing for "../../../../hw/something"
CPPFLAGS+=	-I${X11SRCDIR.xorg-server}/hw/xfree86/dixmods/extmod \

CPPFLAGS+=	${X11FLAGS.DIX}

.if 0
CPPFLAGS.lsb_fixed_xaaBitmap.c=		-DLSBFIRST -DFIXEDBASE
CPPFLAGS.lsb_fixed_xaaBitmap3.c=	-DLSBFIRST -DFIXEDBASE -DTRIPLE_BITS
CPPFLAGS.lsb_fixed_xaaStipple.c=	-DLSBFIRST -DFIXEDBASE
CPPFLAGS.lsb_fixed_xaaStipple3.c=	-DLSBFIRST -DFIXEDBASE -DTRIPLE_BITS
CPPFLAGS.lsb_fixed_xaaTEGlyph.c=	-DLSBFIRST -DFIXEDBASE
CPPFLAGS.lsb_xaaBitmap.c=		-DLSBFIRST
CPPFLAGS.lsb_xaaBitmap3.c=		-DLSBFIRST -DTRIPLE_BITS
CPPFLAGS.lsb_xaaStipple.c=		-DLSBFIRST
CPPFLAGS.lsb_xaaStipple3.c=		-DLSBFIRST -DTRIPLE_BITS
CPPFLAGS.lsb_xaaTEGlyph.c=		-DLSBFIRST
CPPFLAGS.msb_fixed_xaaBitmap.c=		-DMSBFIRST -DFIXEDBASE
CPPFLAGS.msb_fixed_xaaBitmap3.c=	-DMSBFIRST -DFIXEDBASE -DTRIPLE_BITS
CPPFLAGS.msb_fixed_xaaStipple.c=	-DMSBFIRST -DFIXEDBASE
CPPFLAGS.msb_fixed_xaaStipple3.c=	-DMSBFIRST -DFIXEDBASE -DTRIPLE_BITS
CPPFLAGS.msb_fixed_xaaTEGlyph.c=	-DMSBFIRST -DFIXEDBASE
CPPFLAGS.msb_xaaBitmap.c=		-DMSBFIRST
CPPFLAGS.msb_xaaBitmap3.c=		-DMSBFIRST -DTRIPLE_BITS
CPPFLAGS.msb_xaaStipple.c=		-DMSBFIRST
CPPFLAGS.msb_xaaStipple3.c=		-DMSBFIRST -DTRIPLE_BITS
CPPFLAGS.msb_xaaTEGlyph.c=		-DMSBFIRST
CPPFLAGS.xaaSeg.c=			-DPOLYSEGMENT
CPPFLAGS.xaaDashSeg.c=			-DPOLYSEGMENT
.endif

.include <bsd.x11.mk>
LIBDIR=	${XMODULEDIR}
.include <bsd.lib.mk>
