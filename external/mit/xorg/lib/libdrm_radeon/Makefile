#      $NetBSD: Makefile,v 1.4 2010/08/14 10:29:43 mrg Exp $

.include <bsd.own.mk>

LIB=	drm_radeon
.PATH: ${X11SRCDIR.drm}
.PATH: ${X11SRCDIR.drm}/radeon

SRCS=	radeon_bo.c \
	radeon_bo_gem.c \
	radeon_cs.c \
	radeon_cs_gem.c \
	radeon_cs_space.c \
	bof.c

CPPFLAGS+=	-I${DESTDIR}${X11INCDIR}/libdrm
CPPFLAGS+=	-I${DESTDIR}${X11INCDIR}/X11 -I${X11SRCDIR.drm}

COPTS.radeon_cs_gem.c+=	-Wno-error	# XXX

LIBDPLIBS=	drm	${.CURDIR}/../libdrm

PKGCONFIG=	libdrm_radeon
PKGDIST=	drm

.include <bsd.x11.mk>
.include <bsd.lib.mk>
