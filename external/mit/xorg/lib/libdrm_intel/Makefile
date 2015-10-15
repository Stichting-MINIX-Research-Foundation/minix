#      $NetBSD: Makefile,v 1.9 2014/06/03 13:51:52 riastradh Exp $

.include <bsd.own.mk>

LIB=	drm_intel
.PATH: ${X11SRCDIR.drm}/intel

SRCS=	intel_bufmgr.c intel_bufmgr_fake.c intel_bufmgr_gem.c \
	intel_decode.c mm.c

CFLAGS+=	-std=c99

CPPFLAGS+=	-I${DESTDIR}${X11INCDIR}/libdrm
CPPFLAGS+=	-I${DESTDIR}${X11INCDIR}/X11 -I${X11SRCDIR.drm}

LIBDPLIBS=	drm	${.CURDIR}/../libdrm
LIBDPLIBS+=	pciaccess	${.CURDIR}/../libpciaccess

PKGCONFIG=	libdrm_intel
PKGDIST=	drm

CWARNFLAGS.clang+=	-Wno-pointer-sign

.include "../libdrm/Makefile.defines"

.include <bsd.x11.mk>
.include <bsd.lib.mk>
