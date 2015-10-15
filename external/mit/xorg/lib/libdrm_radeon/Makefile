#      $NetBSD: Makefile,v 1.7 2014/07/13 16:35:49 joerg Exp $

.include <bsd.own.mk>

LIB=	drm_radeon
.PATH: ${X11SRCDIR.drm}
.PATH: ${X11SRCDIR.drm}/radeon

SRCS=	radeon_bo.c \
	radeon_bo_gem.c \
	radeon_cs.c \
	radeon_cs_gem.c \
	radeon_cs_space.c \
	radeon_surface.c \
	bof.c

CPPFLAGS+=	-I${DESTDIR}${X11INCDIR}/libdrm
CPPFLAGS+=	-I${DESTDIR}${X11INCDIR}/X11 -I${X11SRCDIR.drm}

CWARNFLAGS.clang+=	-Wno-pointer-sign -Wno-parentheses-equality

LIBDPLIBS=	drm	${.CURDIR}/../libdrm

PKGCONFIG=	libdrm_radeon
PKGDIST=	drm

.include "../libdrm/Makefile.defines"

.include <bsd.x11.mk>
.include <bsd.lib.mk>
