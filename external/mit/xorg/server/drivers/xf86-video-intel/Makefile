#	$NetBSD: Makefile,v 1.11 2013/10/16 15:28:27 martin Exp $

DRIVER=		xf86-video-intel
DRIVER_NAME=	intel_drv

SRCS=		drmmode_display.c i810_accel.c i810_cursor.c i810_dga.c
SRCS+=		i810_dri.c i810_driver.c i810_io.c i810_memory.c i810_video.c
SRCS+=		i810_wmark.c i830_3d.c i830_accel.c i830_bios.c
SRCS+=		i830_batchbuffer.c i830_crt.c i830_cursor.c i830_debug.c
SRCS+=		i830_display.c i830_quirks.c i830_driver.c i830_dvo.c
SRCS+=		i830_hdmi.c i830_i2c.c i830_io.c i830_lvds.c i830_memory.c
SRCS+=		i830_modes.c i830_video.c i830_sdvo.c i830_tv.c
SRCS+=		i915_3d.c i915_video.c i965_video.c 
SRCS+=		i830_xaa.c i830_render.c i915_render.c i965_render.c
SRCS+=		i830_dri.c i830_exa.c
SRCS+=		i830_hwmc.c i915_hwmc.c i965_hwmc.c
MAN=		intel.4

CPPFLAGS+=      -I${DESTDIR}${X11INCDIR}/X11
CPPFLAGS+=      -I${DESTDIR}${X11INCDIR}/X11/dri
CPPFLAGS+=      -I${DESTDIR}${X11INCDIR}/libdrm
CPPFLAGS+=	-DI830_XV -DI830_USE_XAA -DI830_USE_EXA -DINTEL_XVMC

LDADD+=		-ldrm_intel

.include "../Makefile.xf86-driver"

SUBDIR= ch7017 ch7xxx ivch sil164 tfp410

CWARNFLAGS.clang+=	-Wno-parentheses -Wno-tautological-compare \
			-Wno-empty-body -Wno-error

#LSC: Clang/Minix3
DBG= -O0
NOCLANGERROR?=	yes

.include <bsd.subdir.mk>
