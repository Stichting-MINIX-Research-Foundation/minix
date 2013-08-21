#	$NetBSD: Makefile,v 1.7 2011/08/02 09:11:38 mrg Exp $

PROG=	cvt
SRCS=	cvt.c xf86cvt.c xprintf.c
MAN=	cvt.1

.include "../Makefile.utils"

CPPFLAGS+=	${X11FLAGS.DIX} -DHAVE_XORG_CONFIG_H \
		-I${DESTDIR}${X11INCDIR}/xorg \
		-I${DESTDIR}${X11INCDIR}/pixman-1 \
		-I${X11SRCDIR.xorg-server}/Xext \
		-I${X11SRCDIR.xorg-server}/../include \
		-I${X11SRCDIR.xorg-server}/hw/xfree86/ddc \
		-I${X11SRCDIR.xorg-server}/hw/xfree86/parser

LDADD+=	-lm
DPADD+=	${LIBM}

.PATH: ${X11SRCDIR.xorg-server}/hw/xfree86/utils/${PROG}
.PATH: ${X11SRCDIR.xorg-server}/hw/xfree86/utils/man
.PATH: ${X11SRCDIR.xorg-server}/hw/xfree86/modes
.PATH: ${X11SRCDIR.xorg-server}/os

.include <bsd.x11.mk>
.include <bsd.prog.mk>
