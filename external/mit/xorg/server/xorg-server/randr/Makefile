#	$NetBSD: Makefile,v 1.5 2011/08/02 09:11:39 mrg Exp $

.include "../Makefile.serverlib"
.include "../Makefile.servermod"

LIB=            randr

.PATH:          ${X11SRCDIR.xorg-server}/randr
SRCS=           randr.c randrstr.h rrcrtc.c rrdispatch.c \
		rrinfo.c rrmode.c rroutput.c rrpointer.c rrproperty.c \
		rrscreen.c rrsdispatch.c rrtransform.c
SRCS+=		rrxinerama.c

CPPFLAGS+=      -I${DESTDIR}${X11INCDIR}/pixman-1 \
		-I${DESTDIR}${X11INCDIR}/xorg \
		-I${X11SRCDIR.xorg-server}/Xext \
		-I${X11SRCDIR.xorg-server}/../include
CPPFLAGS+=      ${X11FLAGS.DIX}

COPTS.randr.c+=		-Wno-error
COPTS.rrcrtc.c+=	-Wno-error
COPTS.rrdispatch.c+=	-Wno-error
COPTS.rrmode.c+=	-Wno-error
COPTS.rroutput.c+=	-Wno-error
COPTS.rrproperty.c+=	-Wno-error

.include <bsd.x11.mk>
.include <bsd.lib.mk>
