#	$NetBSD: Makefile,v 1.1 2014/03/01 19:39:03 tsutsui Exp $

.include <bsd.own.mk>

PROG=		X68k
BINMODE=	4711

SYMLINKS=	${X11BINDIR}/X68k ${X11BINDIR}/X

FILES=		X68kConfig
FILESDIR=	${X11ETCDIR}

.include "../../../Makefile.serverlib"
.include "../../../Makefile.servermod"

WARNS?=	2

.PATH:	${X11SRCDIR.xorg-server}/hw/netbsd/x68k
SRCS=	x68kInit.c x68kConfig.c x68kIo.c x68kMouse.c x68kKbd.c x68kKeyMap.c \
	x68kFb.c x68kGraph.c x68kText.c

.PATH:	${X11SRCDIR.xorg-server}/Xext
SRCS+=	dpmsstubs.c

.PATH:	${X11SRCDIR.xorg-server}/Xi
SRCS+=	stubs.c

.PATH:	${X11SRCDIR.xorg-server}/mi
SRCS+=	miinitext.c

.PATH:	${X11SRCDIR.xorg-server}/fb
SRCS+=	fbcmap_mi.c

CPPFLAGS+=	-I${X11SRCDIR.xorg-server}/include \
		-I${X11SRCDIR.xorg-server}/hw/netbsd/x68k \
		-I${X11SRCDIR.xorg-server}/fb \
		-I${X11SRCDIR.xorg-server}/mi \
		-I${X11SRCDIR.xorg-server}/render \
		-I${DESTDIR}${X11INCDIR}/X11 \

CPPFLAGS+=	-UXFree86LOADER -UXF86VIDMODE -UXFreeXDGA -UXF86MISC \
		-UXF86DRI -UXF86BIGFONT

CPPFLAGS.x68kConfig.c=	 -DX11_LIBDIR=\"${X11ETCDIR}\"

.include "../../../Makefile.Xserver"

LDADD+= ${LDADD.dix} \
	${LDADD.fb} \
	${LDADD.mi} \
	${LDADD.xkb} \
	${LDADD.xkbstubs} \
	${LDADD.randr} \
	${LDADD.Xext} \
	${LDADD.composite} \
	${LDADD.render} \
	${LDADD.os} \
	${LDADD.xfixes} \
	${LDADD.damageext} \
	${LDADD.miext/damage} \
	${LDADD.miext/shadow} \
	${LDADD.miext/sync} \
	${LDADD.Xi} \
	-lXfont \
	-lpixman-1 \
	-lXext \
	-lX11 \
	-lz \
	-lm

DPADD+= ${LDADD.dix} \
	${DPADD.fb} \
	${DPADD.mi} \
	${DPADD.xkb} \
	${DPADD.randr} \
	${DPADD.Xext} \
	${DPADD.composite} \
	${DPADD.render} \
	${DPADD.os} \
	${DPADD.xfixes} \
	${DPADD.damageext} \
	${DPADD.miext/damage} \
	${DPADD.miext/shadow} \
	${DPADD.miext/sync} \
	${DPADD.Xi} \
	${LIBXFONT} \
	${LIBPIXMAN-1} \
	${LIBXEXT} \
	${LIBX11} \
	${LIBZ} \
	${LIBM}

.include <bsd.x11.mk>
.include <bsd.prog.mk>
