#	$NetBSD: Makefile,v 1.6 2014/03/17 08:01:18 mrg Exp $

# install include/libdrm files

.include <bsd.own.mk>

.PATH:	${X11SRCDIR.drm}/include/drm
.PATH:	${X11SRCDIR.drm}/intel
.PATH:	${X11SRCDIR.drm}/radeon

INCS=	drm.h \
	drm_fourcc.h \
	drm_mode.h \
	drm_sarea.h \
	i915_drm.h \
	intel_aub.h \
	intel_bufmgr.h \
	intel_debug.h \
	mach64_drm.h \
	mga_drm.h \
	nouveau_drm.h \
	r128_drm.h \
	r600_pci_ids.h \
	radeon_bo.h \
	radeon_bo_gem.h \
	radeon_bo_int.h \
	radeon_cs.h \
	radeon_cs_gem.h \
	radeon_cs_int.h \
	radeon_drm.h \
	radeon_surface.h \
	savage_drm.h \
	sis_drm.h \
	via_drm.h \
	qxl_drm.h

INCSDIR=${X11INCDIR}/libdrm

.include <bsd.x11.mk>
.include <bsd.prog.mk>
