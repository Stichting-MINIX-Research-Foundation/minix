/*	$NetBSD: cgsixvar.h,v 1.13 2012/07/12 01:20:22 macallan Exp $ */

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Paul Kranenburg.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "wsdisplay.h"
#include <dev/wscons/wsdisplay_vconsvar.h>
#include <dev/wscons/wsdisplay_glyphcachevar.h>

/*
 * color display (cgsix) driver; common definitions.
 */

union cursor_cmap {		/* colormap, like bt_cmap, but tiny */
	u_char	cm_map[2][3];	/* 2 R/G/B entries */
	u_int	cm_chip[2];	/* 2 chip equivalents */
};

struct cg6_cursor {		/* cg6 hardware cursor status */
	short	cc_enable;		/* cursor is enabled */
	struct	fbcurpos cc_pos;	/* position */
	struct	fbcurpos cc_hot;	/* hot-spot */
	struct	fbcurpos cc_size;	/* size of mask & image fields */
	u_int	cc_bits[2][32];		/* space for mask & image bits */
	union	cursor_cmap cc_color;	/* cursor colormap */
};

/* per-display variables */
struct cgsix_softc {
	device_t	sc_dev;		/* base device */
	struct fbdevice	sc_fb;		/* frame buffer device */
	bus_space_tag_t	sc_bustag;
	bus_addr_t	sc_paddr;	/* phys address for device mmap() */

	volatile struct bt_regs *sc_bt;		/* Brooktree registers */
	volatile int *sc_fhc;			/* FHC register */
	volatile struct cg6_thc *sc_thc;	/* THC registers */
	volatile struct cg6_tec_xxx *sc_tec;	/* TEC registers */
	volatile struct cg6_fbc *sc_fbc;	/* FBC registers */
	short	sc_fhcrev;		/* hardware rev */
	short	sc_blanked;		/* true if blanked */
	struct	cg6_cursor sc_cursor;	/* software cursor info */
	
	uint32_t sc_width;
	uint32_t sc_height;	/* display width / height */
	uint32_t sc_stride;
	uint32_t sc_mono_width;	/* how many monochrome pixels to write */
	uint32_t sc_ramsize;		/* VRAM size in bytes */
	int sc_fb_is_open;
#if NWSDISPLAY > 0	
	int sc_mode;
	uint32_t sc_bg;
	struct vcons_data vd;
	uint8_t sc_default_cmap[768];
	glyphcache sc_gc;	
#endif
	union	bt_cmap sc_cmap;	/* Brooktree color map */
};

#define IS_IN_EMUL_MODE(sc) \
	((sc->sc_fb_is_open == 0) && \
	 (sc->sc_mode == WSDISPLAYIO_MODE_EMUL))

#ifdef RASTERCONSOLE
extern int cgsix_use_rasterconsole;
#else
#define cgsix_use_rasterconsole 0
#endif

void	cg6attach(struct cgsix_softc *, const char *, int);
