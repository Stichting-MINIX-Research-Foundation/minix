#	$NetBSD: Makefile,v 1.12 2014/03/20 05:32:31 mrg Exp $

.include <bsd.own.mk>

DRIVER=		xf86-video-mga
DRIVER_NAME=	mga_drv

SRCS=		mga_bios.c mga_dac3026.c mga_dacG.c
SRCS+=		mga_dga.c mga_dh.c mga_driver.c mga_g450pll.c
SRCS+=		mga_halmod.c mga_hwcurs.c mga_merge.c mga_shadow.c
SRCS+=		mga_vga.c mga_video.c mga_storm.c
MAN=		mga.4

.if ${X11DRI} != "no"
#SRCS+=		mga_dri.c
.endif

#if USE_EXA
SRCS+=		mga_exa.c
CPPFLAGS+=	-DUSE_EXA
#endif

#if USE_XAA
SRCS+=		mga_arc.c
CPPFLAGS+=	-DUSE_XAA -DHAVE_XAA_H
#endif

CPPFLAGS+=	-I${DESTDIR}${X11INCDIR}/X11
#CPPFLAGS+=	-I${DESTDIR}${X11INCDIR}/X11/dri
CPPFLAGS+=	-I${DESTDIR}${X11INCDIR}/libdrm

CPPFLAGS.mga_driver.c=	-Wno-error	# XXX fix me please
CPPFLAGS.mga_dacG.c=	-Wno-error	# XXX fix me please

CWARNFLAGS.clang+=	-Wno-conversion -Wno-format -Wno-empty-body

.include "../Makefile.xf86-driver"
