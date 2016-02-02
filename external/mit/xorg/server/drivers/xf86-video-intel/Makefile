#	$NetBSD: Makefile,v 1.21 2015/05/14 02:43:33 mrg Exp $

.include <bsd.own.mk>

DRIVER=		xf86-video-intel
DRIVER_NAME=	intel_drv

# main module
SRCS+=		intel_device.c
SRCS+=		intel_options.c
SRCS+=		intel_module.c
SRCS+=		backlight.c
SRCS+=		fd.c

# legacy/i810
.PATH:	${X11SRCDIR.${DRIVER}}/src/legacy/i810
SRCS+=		i810_accel.c
SRCS+=		i810_cursor.c
SRCS+=		i810_dga.c
SRCS+=		i810_dri.c
SRCS+=		i810_driver.c
SRCS+=		i810_hwmc.c
SRCS+=		i810_memory.c
SRCS+=		i810_video.c
SRCS+=		i810_wmark.c
SRCS+=		i810_xaa.c

.if !defined(__MINIX)
# sna
.PATH:	${X11SRCDIR.${DRIVER}}/src/sna
SRCS+=		blt.c
SRCS+=		kgem.c
SRCS+=		gen2_render.c
SRCS+=		gen3_render.c
SRCS+=		gen4_common.c
SRCS+=		gen4_render.c
SRCS+=		gen4_source.c
SRCS+=		gen4_vertex.c
SRCS+=		gen5_render.c
SRCS+=		gen6_common.c
SRCS+=		gen6_render.c
SRCS+=		gen7_render.c
SRCS+=		gen8_eu.c
SRCS+=		gen8_render.c
SRCS+=		gen8_vertex.c

SRCS+=		sna_accel.c
SRCS+=		sna_acpi.c
SRCS+=		sna_blt.c
SRCS+=		sna_composite.c
SRCS+=		sna_cpu.c
SRCS+=		sna_damage.c
SRCS+=		sna_display.c
SRCS+=		sna_display_fake.c
SRCS+=		sna_dri2.c
#SRCS+=		sna_dri3.c
SRCS+=		sna_driver.c
SRCS+=		sna_glyphs.c
SRCS+=		sna_gradient.c
SRCS+=		sna_io.c
#SRCS+=		sna_present.c
SRCS+=		sna_render.c
SRCS+=		sna_stream.c
SRCS+=		sna_threads.c
SRCS+=		sna_tiling.c
SRCS+=		sna_transform.c
SRCS+=		sna_trapezoids.c
SRCS+=		sna_trapezoids_boxes.c
SRCS+=		sna_trapezoids_imprecise.c
SRCS+=		sna_trapezoids_mono.c
SRCS+=		sna_trapezoids_precise.c
SRCS+=		sna_vertex.c
SRCS+=		sna_video.c
SRCS+=		sna_video_hwmc.c
SRCS+=		sna_video_overlay.c
SRCS+=		sna_video_sprite.c
SRCS+=		sna_video_textured.c

# sna/brw
.PATH:	${X11SRCDIR.${DRIVER}}/src/sna/brw
SRCS+=		brw_disasm.c
SRCS+=		brw_eu.c
SRCS+=		brw_eu_emit.c
SRCS+=		brw_sf.c
SRCS+=		brw_wm.c

# sna/fb
.PATH:	${X11SRCDIR.${DRIVER}}/src/sna/fb
SRCS+=		fbarc.c
SRCS+=		fbbitmap.c
SRCS+=		fbblt.c
SRCS+=		fbbltone.c
SRCS+=		fbclip.c
SRCS+=		fbcopy.c
SRCS+=		fbfill.c
SRCS+=		fbgc.c
SRCS+=		fbglyph.c
SRCS+=		fbimage.c
SRCS+=		fbline.c
SRCS+=		fbpict.c
SRCS+=		fbpoint.c
SRCS+=		fbpush.c
SRCS+=		fbseg.c
SRCS+=		fbspan.c
SRCS+=		fbstipple.c
SRCS+=		fbtile.c
SRCS+=		fbutil.c
.endif # !defined(__MINIX)

# uxa
.PATH:	${X11SRCDIR.${DRIVER}}/src/uxa
SRCS+=		i830_3d.c
SRCS+=		i830_render.c
SRCS+=		i915_3d.c
SRCS+=		i915_render.c
SRCS+=		i915_video.c
SRCS+=		i965_3d.c
SRCS+=		i965_render.c
SRCS+=		i965_video.c
SRCS+=		intel_batchbuffer.c
SRCS+=		intel_display.c
SRCS+=		intel_dri.c
SRCS+=		intel_driver.c
SRCS+=		intel_hwmc.c
SRCS+=		intel_memory.c
#SRCS+=		intel_present.c
#SRCS+=		intel_sync.c
SRCS+=		intel_uxa.c
SRCS+=		intel_uxa_video.c
SRCS+=		intel_video.c
SRCS+=		intel_video_overlay.c
SRCS+=		uxa-accel.c
SRCS+=		uxa-glyphs.c
SRCS+=		uxa-render.c
SRCS+=		uxa-unaccel.c
SRCS+=		uxa.c

MAN=		intel.4

CPPFLAGS+=      -I${DESTDIR}${X11INCDIR}/X11
CPPFLAGS+=      -I${DESTDIR}${X11INCDIR}/X11/dri
CPPFLAGS+=      -I${DESTDIR}${X11INCDIR}/libdrm
CPPFLAGS+=	-I${X11SRCDIR.${DRIVER}}/src
CPPFLAGS+=	-I${X11SRCDIR.${DRIVER}}/src/render_program
CPPFLAGS+=	-I${X11SRCDIR.${DRIVER}}/xvmc

# Use the Intel-style __sync_* atomic primitives.

CPPFLAGS+=	-DDEFAULT_ACCEL_METHOD=SNA
CPPFLAGS+=	-DENABLE_XVMC=1
CPPFLAGS+=	-DHAS_PIXMAN_GLYPHS=1
CPPFLAGS+=	-DHAS_PIXMAN_TRIANGLES=1
CPPFLAGS+=	-DHAVE_ATOMIC_PRIMITIVES=1
CPPFLAGS+=	-DHAVE_CPUID_H=1
CPPFLAGS+=	-DHAVE_DGAPROC_H=1
CPPFLAGS+=	-DHAVE_DLFCN_H=1
CPPFLAGS+=	-DHAVE_DRI1=1
CPPFLAGS+=	-DHAVE_DRI2=1
CPPFLAGS+=	-DHAVE_DRI2_H=1
CPPFLAGS+=	-DHAVE_DRISTRUCT_H=1
CPPFLAGS+=	-DHAVE_DRI_H=1
CPPFLAGS+=	-DHAVE_GETLINE=1
CPPFLAGS+=	-DHAVE_INTTYPES_H=1
CPPFLAGS+=	-DHAVE_MEMORY_H=1
CPPFLAGS+=	-DHAVE_SAREA_H=1
CPPFLAGS+=	-DHAVE_STDINT_H=1
CPPFLAGS+=	-DHAVE_STDLIB_H=1
CPPFLAGS+=	-DHAVE_STRINGS_H=1
CPPFLAGS+=	-DHAVE_STRING_H=1
CPPFLAGS+=	-DHAVE_STRLCAT=1
CPPFLAGS+=	-DHAVE_STRLCPY=1
CPPFLAGS+=	-DHAVE_STRNDUP=1
CPPFLAGS+=	-DHAVE_SYS_IPC_H=1
CPPFLAGS+=	-DHAVE_SYS_STAT_H=1
CPPFLAGS+=	-DHAVE_SYS_TYPES_H=1
CPPFLAGS+=	-DHAVE_UNISTD_H=1
CPPFLAGS+=	-DHAVE_VGAHW_H=1
CPPFLAGS+=	-DHAVE_XAA_H=1
CPPFLAGS+=	-DKMS=1
CPPFLAGS+=	-DSTDC_HEADERS=1
CPPFLAGS+=	-DUMS=1
CPPFLAGS+=	-DUSE_BACKLIGHT=1
CPPFLAGS+=	-DUSE_SNA=1
CPPFLAGS+=	-DUSE_UXA=1

# XXX This should not be hard-coded here...
DRI_DRIVER_PATH=	/usr/X11R7/lib/modules/dri
CPPFLAGS+=	-DDRI_DRIVER_PATH=\"${DRI_DRIVER_PATH:Q}\"

# XXX Probably better to set this in Makefile.xf86-driver, conditional
# on ELF &c.
CFLAGS+=	-fvisibility=hidden

LIBDPLIBS+=	drm_intel	${.CURDIR}/../../../lib/libdrm_intel
# MINIX: No pthread
#LIBDPLIBS+=	pthread		${NETBSDSRCDIR}/lib/libpthread

.include "../Makefile.xf86-driver"

CWARNFLAGS.clang+=	-Wno-parentheses -Wno-tautological-compare \
			-Wno-empty-body -Wno-error

COPTS.sna_display.c=	-Wno-stack-protector
#LSC: Clang/Minix3
DBG= -O0
NOCLANGERROR?=	yes

.include <bsd.subdir.mk>
