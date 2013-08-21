#	$NetBSD: Makefile,v 1.8 2010/11/23 08:22:35 mrg Exp $

.include "../Makefile.serverlib"
.include "../Makefile.servermod"

LIB=		xkb

.PATH:		${X11SRCDIR.xorg-server}/${LIB}
.PATH:		${X11SRCDIR.X11}/src
.PATH:		${X11SRCDIR.xkbfile}/src
DDXSRCS=	ddxBeep.c ddxCtrls.c \
		ddxLEDs.c ddxLoad.c \
		ddxList.c
DIXSRCS=	xkb.c xkbUtils.c xkbEvents.c xkbAccessX.c xkbSwap.c \
		xkbLEDs.c xkbInit.c xkbActions.c xkbPrKeyEv.c
XKBFILESRCS=	maprules.c xkmread.c xkbtext.c xkbfmisc.c \
		xkbout.c
X11SRCS=	XKBMisc.c XKBAlloc.c XKBGAlloc.c XKBMAlloc.c

SRCS=		${DDXSRCS} ${DIXSRCS} ${XKBFILESRCS} ${X11SRCS}

CPPFLAGS+=	-I${X11SRCDIR.xorg-server}/include \
		-I${DESTDIR}${X11INCDIR}/X11 \
		-I${DESTDIR}${X11INCDIR}/X11/extensions \
		-I${DESTDIR}${X11INCDIR}/pixman-1 \
		-I${X11SRCDIR.xorg-server}/Xext \
		-I${X11SRCDIR.xorg-server}/Xext/extmod \
		-I${X11SRCDIR.xorg-server}/mi \
		-I${X11SRCDIR.xorg-server}/Xi \
		-I${X11SRCDIR.xorg-server}/${LIB} \
		-I${X11SRCDIR.xorg-server}/hw/xfree86/common \
		-I${X11SRCDIR.xorg-server}/../include

# For ${XKBROOTDIR} and ${XKBCOMPDIR}
.include "../../../bin/xkbcomp/Makefile.common"

# XXX: ddxLoad.c expects XKM_OUTPUT_DIR to have a trailing '/'.

CPPFLAGS+=	-DXKB_IN_SERVER -DHAVE_XKB_CONFIG_H \
		-DXKB_BASE_DIRECTORY=\"${XKBROOTDIR}\" \
		-DXKB_BIN_DIRECTORY=\"${X11BINDIR}\" \
		-DXKM_OUTPUT_DIR=\"${XKBCOMPDIR}/\" \
		-DXKB_DFLT_DISABLED=0

COPTS.XKBAlloc.c=	-Wno-error	# XXX gcc 3.3
COPTS.XKBMAlloc.c=	-Wno-error	# XXX gcc 3.3
COPTS.xkb.c=		-Wno-error	# XXX gcc 3.3
COPTS.xkbUtils.c=	-Wno-error	# XXX gcc 3.3

CLEANFILES+=		${BUILDOBJS}
realall: ${BUILDOBJS}

.include <bsd.x11.mk>
.include <bsd.lib.mk>
