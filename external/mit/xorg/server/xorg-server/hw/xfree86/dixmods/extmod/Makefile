# $NetBSD: Makefile,v 1.10 2010/11/23 08:22:32 mrg Exp $

XMODULEDIR=     ${X11USRLIBDIR}/modules/extensions

.include "../../../../Makefile.serverlib"
.include "../../../../Makefile.servermod"

LIB=	extmod

.PATH:	${X11SRCDIR.xorg-server}/hw/xfree86/dixmods/extmod
SRCS=	xf86dga2.c \
	xvmod.c \
	xf86vmode.c \
	modinit.c

CPPFLAGS+=	-I${X11SRCDIR.xorg-server}/hw/xfree86/${LIB} \
		-I${X11SRCDIR.xorg-server}/hw/xfree86/afb \
		-I${X11SRCDIR.xorg-server}/hw/xfree86/mfb \
		-I${X11SRCDIR.xorg-server}/hw/xfree86/cfb \
		-I${X11SRCDIR.xorg-server}/hw/xfree86/dbe \
		-I${X11SRCDIR.xorg-server}/hw/xfree86/hw/xfree86/loader \
		-I${X11SRCDIR.xorg-server}/hw/xfree86/miext/shadow \
		-I${X11SRCDIR.xorg-server}/hw/xfree86/common \
		-I${X11SRCDIR.xorg-server}/Xext \
		-I${DESTDIR}${X11INCDIR}/X11 \
		-I${DESTDIR}${X11INCDIR}/pixman-1 \
		-I${DESTDIR}${X11INCDIR}/xorg

CPPFLAGS+=	${X11FLAGS.DIX} ${X11INCS.DIX} \
		-DHAVE_XORG_CONFIG_H

# XXX
CPPFLAGS.xf86RandR12.c=	-Wno-error
CPPFLAGS.xf86dga2.c=	-Wno-error
CPPFLAGS.xf86vmode.c=	-Wno-error	# XXX xf86vmstr.h

XEXTMODULEOBJDIR!=  cd ${NETBSDSRCDIR}/external/mit/xorg/server/xorg-server/Xext/Xextmodule && ${PRINTOBJDIR}
LIBXEXTMODULE=	${XEXTMODULEOBJDIR}/libXextmodule.a

LDADD=  -L${XEXTMODULEOBJDIR} -lXextmodule
DPADD=  ${LIBXEXTMODULE}

.include <bsd.x11.mk>
LIBDIR=	${XMODULEDIR}
.include <bsd.lib.mk>
