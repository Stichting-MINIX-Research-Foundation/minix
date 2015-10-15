#	$NetBSD: Makefile,v 1.2 2015/09/19 23:00:44 wiz Exp $

.include <bsd.own.mk>

DRIVER=		xf86-video-ati-kms
DRIVER_NAME=	radeon_drv

.PATH:  ${X11SRCDIR.${DRIVER}}/src/AtomBios
SRCS.ATOMBIOS= \
        CD_Operations.c \
        Decoder.c \
        hwserv_drv.c

.if ${X11DRI} != "no"
SRCS.DRI=	radeon_dri.c
CPPFLAGS+=	${X11FLAGS.DRI}
.endif

.if ${X11DRI} != "no"
# XXX
. if ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "x86_64"
SRCS.KMS=	radeon_dri2.c radeon_kms.c drmmode_display.c radeon_vbo.c
CPPFLAGS+=	-DXF86DRM_MODE
LIBDPLIBS+=	drm_radeon ${.CURDIR}/../../../lib/libdrm_radeon
. endif
.endif

SRCS.EXA=	radeon_exa.c r600_exa.c r6xx_accel.c \
		r600_textured_videofuncs.c r600_shader.c radeon_exa_shared.c \
		evergreen_exa.c evergreen_accel.c evergreen_shader.c \
		evergreen_textured_videofuncs.c cayman_accel.c cayman_shader.c
SRCS.KMS=	radeon_dri2.c radeon_kms.c drmmode_display.c radeon_vbo.c \
		radeon_bo_helper.c
# XXX not yet
SRCS.GLAMOR=	radeon_glamor.c

SRCS=		radeon_accel.c radeon_video.c \
		radeon_misc.c radeon_probe.c \
		radeon_textured_video.c radeon_xvmc.c \
		$(SRCS.EXA) \
		$(SRCS.KMS)

MAN=		radeon.4
MANSUBDIR=	/x86

CPPFLAGS+=	-DHAVE_CONFIG_H
CPPFLAGS+=	-I${DESTDIR}${X11INCDIR}/X11 \
		-I${DESTDIR}${X11INCDIR}/X11/dri \
		-I${DESTDIR}${X11INCDIR}/libdrm \
		-I${DESTDIR}${X11INCDIR}/pixman-1 \
		-I${X11SRCDIR.${DRIVER}}/../include

# XXX
COPTS+=	-Wno-error

.include "../Makefile.xf86-driver"
