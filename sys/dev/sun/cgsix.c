/*	$NetBSD: cgsix.c,v 1.65 2014/07/25 08:10:39 dholland Exp $ */

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

/*
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)cgsix.c	8.4 (Berkeley) 1/21/94
 */

/*
 * color display (cgsix) driver.
 *
 * Does not handle interrupts, even though they can occur.
 *
 * XXX should defer colormap updates to vertical retrace interrupts
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: cgsix.c,v 1.65 2014/07/25 08:10:39 dholland Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>
#include <sys/mman.h>
#include <sys/tty.h>
#include <sys/conf.h>

#ifdef DEBUG
#include <sys/proc.h>
#include <sys/syslog.h>
#endif

#include <sys/bus.h>

#include <dev/sun/fbio.h>
#include <dev/sun/fbvar.h>

#include <dev/sun/btreg.h>
#include <dev/sun/btvar.h>
#include <dev/sun/pfourreg.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wsfont/wsfont.h>
#include <dev/rasops/rasops.h>

#include "opt_wsemul.h"
#include "rasops_glue.h"

#include <dev/sun/cgsixreg.h>
#include <dev/sun/cgsixvar.h>

#include "ioconf.h"

static void	cg6_unblank(device_t);
static void	cg6_blank(struct cgsix_softc *, int);

dev_type_open(cgsixopen);
dev_type_close(cgsixclose);
dev_type_ioctl(cgsixioctl);
dev_type_mmap(cgsixmmap);

const struct cdevsw cgsix_cdevsw = {
	.d_open = cgsixopen,
	.d_close = cgsixclose,
	.d_read = noread,
	.d_write = nowrite,
	.d_ioctl = cgsixioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = nopoll,
	.d_mmap = cgsixmmap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = D_OTHER
};

/* frame buffer generic driver */
static struct fbdriver cg6_fbdriver = {
	cg6_unblank, cgsixopen, cgsixclose, cgsixioctl, nopoll, cgsixmmap,
	nokqfilter
};

static void cg6_reset (struct cgsix_softc *);
static void cg6_loadcmap (struct cgsix_softc *, int, int);
static void cg6_loadomap (struct cgsix_softc *);
static void cg6_setcursor (struct cgsix_softc *);/* set position */
static void cg6_loadcursor (struct cgsix_softc *);/* set shape */

#if NWSDISPLAY > 0
#ifdef RASTERCONSOLE
#error RASTERCONSOLE and wsdisplay are mutually exclusive
#endif

static void cg6_setup_palette(struct cgsix_softc *);

struct wsscreen_descr cgsix_defaultscreen = {
	"std",
	0, 0,	/* will be filled in -- XXX shouldn't, it's global */
	NULL,		/* textops */
	8, 16,	/* font width/height */
	WSSCREEN_WSCOLORS,	/* capabilities */
	NULL	/* modecookie */
};

static int 	cgsix_ioctl(void *, void *, u_long, void *, int, struct lwp *);
static paddr_t	cgsix_mmap(void *, void *, off_t, int);
static void	cgsix_init_screen(void *, struct vcons_screen *, int, long *);

static void	cgsix_clearscreen(struct cgsix_softc *);

void 	cgsix_setup_mono(struct cgsix_softc *, int, int, int, int, uint32_t, 
		uint32_t);
void 	cgsix_feed_line(struct cgsix_softc *, int, uint8_t *);
void 	cgsix_rectfill(struct cgsix_softc *, int, int, int, int, uint32_t);
void	cgsix_bitblt(void *, int, int, int, int, int, int, int);

int	cgsix_putcmap(struct cgsix_softc *, struct wsdisplay_cmap *);
int	cgsix_getcmap(struct cgsix_softc *, struct wsdisplay_cmap *);
void	cgsix_putchar(void *, int, int, u_int, long);
void	cgsix_putchar_aa(void *, int, int, u_int, long);
void	cgsix_cursor(void *, int, int, int);

struct wsdisplay_accessops cgsix_accessops = {
	cgsix_ioctl,
	cgsix_mmap,
	NULL,	/* alloc_screen */
	NULL,	/* free_screen */
	NULL,	/* show_screen */
	NULL, 	/* load_font */
	NULL,	/* pollc */
	NULL	/* scroll */
};

const struct wsscreen_descr *_cgsix_scrlist[] = {
	&cgsix_defaultscreen
};

struct wsscreen_list cgsix_screenlist = {
	sizeof(_cgsix_scrlist) / sizeof(struct wsscreen_descr *),
	_cgsix_scrlist
};


extern const u_char rasops_cmap[768];

#endif /* NWSDISPLAY > 0 */

#if (NWSDISPLAY > 0) || defined(RASTERCONSOLE)
void	cg6_invert(struct cgsix_softc *, int, int, int, int);

/* need this for both cases because ri_hw points to it */
static struct vcons_screen cg6_console_screen;
#endif

#ifdef RASTERCONSOLE
int cgsix_use_rasterconsole = 1;
#endif

/*
 * cg6 accelerated console routines.
 *
 * Note that buried in this code in several places is the assumption
 * that pixels are exactly one byte wide.  Since this is cg6-specific
 * code, this seems safe.  This assumption resides in things like the
 * use of ri_emuwidth without messing around with ri_pelbytes, or the
 * assumption that ri_font->fontwidth is the right thing to multiply
 * character-cell counts by to get byte counts.
 */

/*
 * Magic values for blitter
 */

/* Values for the mode register */
#define CG6_MODE	(						\
	  0x00200000 /* GX_BLIT_SRC */					\
	| 0x00020000 /* GX_MODE_COLOR8 */				\
	| 0x00008000 /* GX_DRAW_RENDER */				\
	| 0x00002000 /* GX_BWRITE0_ENABLE */				\
	| 0x00001000 /* GX_BWRITE1_DISABLE */				\
	| 0x00000200 /* GX_BREAD_0 */					\
	| 0x00000080 /* GX_BDISP_0 */					\
)
#define CG6_MODE_MASK	(						\
	  0x00300000 /* GX_BLIT_ALL */					\
	| 0x00060000 /* GX_MODE_ALL */					\
	| 0x00018000 /* GX_DRAW_ALL */					\
	| 0x00006000 /* GX_BWRITE0_ALL */				\
	| 0x00001800 /* GX_BWRITE1_ALL */				\
	| 0x00000600 /* GX_BREAD_ALL */					\
	| 0x00000180 /* GX_BDISP_ALL */					\
)

/* Value for the alu register for screen-to-screen copies */
#define CG6_ALU_COPY	(						\
	  0x80000000 /* GX_PLANE_ONES (ignore planemask register) */	\
	| 0x20000000 /* GX_PIXEL_ONES (ignore pixelmask register) */	\
	| 0x00800000 /* GX_ATTR_SUPP (function unknown) */		\
	| 0x00000000 /* GX_RAST_BOOL (function unknown) */		\
	| 0x00000000 /* GX_PLOT_PLOT (function unknown) */		\
	| 0x08000000 /* GX_PATTERN_ONES (ignore pattern) */		\
	| 0x01000000 /* GX_POLYG_OVERLAP (unsure - handle overlap?) */	\
	| 0x0000cccc /* ALU = src */					\
)

/* Value for the alu register for region fills */
#define CG6_ALU_FILL	(						\
	  0x80000000 /* GX_PLANE_ONES (ignore planemask register) */	\
	| 0x20000000 /* GX_PIXEL_ONES (ignore pixelmask register) */	\
	| 0x00800000 /* GX_ATTR_SUPP (function unknown) */		\
	| 0x00000000 /* GX_RAST_BOOL (function unknown) */		\
	| 0x00000000 /* GX_PLOT_PLOT (function unknown) */		\
	| 0x08000000 /* GX_PATTERN_ONES (ignore pattern) */		\
	| 0x01000000 /* GX_POLYG_OVERLAP (unsure - handle overlap?) */	\
	| 0x0000ff00 /* ALU = fg color */				\
)

/* Value for the alu register for toggling an area */
#define CG6_ALU_FLIP	(						\
	  0x80000000 /* GX_PLANE_ONES (ignore planemask register) */	\
	| 0x20000000 /* GX_PIXEL_ONES (ignore pixelmask register) */	\
	| 0x00800000 /* GX_ATTR_SUPP (function unknown) */		\
	| 0x00000000 /* GX_RAST_BOOL (function unknown) */		\
	| 0x00000000 /* GX_PLOT_PLOT (function unknown) */		\
	| 0x08000000 /* GX_PATTERN_ONES (ignore pattern) */		\
	| 0x01000000 /* GX_POLYG_OVERLAP (unsure - handle overlap?) */	\
	| 0x00005555 /* ALU = ~dst */					\
)

/*
 * Run a blitter command
 */
#define CG6_BLIT(f) { (void)f->fbc_blit; }

/*
 * Run a drawing command
 */
#define CG6_DRAW(f) { (void)f->fbc_draw; }

/*
 * Wait for the whole engine to go idle.  This may not matter in our case;
 * I'm not sure whether blits are actually queued or not.  It more likely
 * is intended for lines and such that do get queued.
 * 0x10000000 bit: GX_INPROGRESS
 */
#define CG6_DRAIN(fbc) do {						\
	while ((fbc)->fbc_s & GX_INPROGRESS)				\
		/*EMPTY*/;						\
} while (0)

/*
 * something is missing here
 * Waiting for GX_FULL to clear should be enough to send another command
 * but some CG6 ( LX onboard for example ) lock up if we do that while
 * it works fine on others ( a 4MB TGX+ I've got here )
 * So, until I figure out what's going on we wait for the blitter to go
 * fully idle.
 */
#define CG6_WAIT_READY(fbc) do {					\
       	while (((fbc)->fbc_s & GX_INPROGRESS/*GX_FULL*/) != 0)		\
		/*EMPTY*/;						\
} while (0)

#if (NWSDISPLAY > 0) || defined(RASTERCONSOLE)
static void cg6_ras_init(struct cgsix_softc *);
static void cg6_ras_copyrows(void *, int, int, int);
static void cg6_ras_copycols(void *, int, int, int, int);
static void cg6_ras_erasecols(void *, int, int, int, long int);
static void cg6_ras_eraserows(void *, int, int, long int);
#if defined(RASTERCONSOLE) && defined(CG6_BLIT_CURSOR)
static void cg6_ras_do_cursor(struct rasops_info *);
#endif

static void
cg6_ras_init(struct cgsix_softc *sc)
{
	volatile struct cg6_fbc *fbc = sc->sc_fbc;

	CG6_DRAIN(fbc);
	fbc->fbc_mode &= ~CG6_MODE_MASK;
	fbc->fbc_mode |= CG6_MODE;

	/* set some common drawing engine parameters */
	fbc->fbc_clip = 0;
	fbc->fbc_s = 0;
	fbc->fbc_offx = 0;
	fbc->fbc_offy = 0;
	fbc->fbc_clipminx = 0;
	fbc->fbc_clipminy = 0;
	fbc->fbc_clipmaxx = 0x3fff;
	fbc->fbc_clipmaxy = 0x3fff;
}

static void
cg6_ras_copyrows(void *cookie, int src, int dst, int n)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;
	struct cgsix_softc *sc = scr->scr_cookie;
	volatile struct cg6_fbc *fbc = sc->sc_fbc;

	if (dst == src)
		return;
	if (src < 0) {
		n += src;
		src = 0;
	}
	if (src+n > ri->ri_rows)
		n = ri->ri_rows - src;
	if (dst < 0) {
		n += dst;
		dst = 0;
	}
	if (dst+n > ri->ri_rows)
		n = ri->ri_rows - dst;
	if (n <= 0)
		return;
	n *= ri->ri_font->fontheight;
	src *= ri->ri_font->fontheight;
	dst *= ri->ri_font->fontheight;

	CG6_WAIT_READY(fbc);

	fbc->fbc_alu = CG6_ALU_COPY;
	fbc->fbc_mode = GX_BLIT_SRC | GX_MODE_COLOR8;

	fbc->fbc_x0 = ri->ri_xorigin;
	fbc->fbc_y0 = ri->ri_yorigin + src;
	fbc->fbc_x1 = ri->ri_xorigin + ri->ri_emuwidth - 1;
	fbc->fbc_y1 = ri->ri_yorigin + src + n - 1;
	fbc->fbc_x2 = ri->ri_xorigin;
	fbc->fbc_y2 = ri->ri_yorigin + dst;
	fbc->fbc_x3 = ri->ri_xorigin + ri->ri_emuwidth - 1;
	fbc->fbc_y3 = ri->ri_yorigin + dst + n - 1;
	CG6_BLIT(fbc);
}

static void
cg6_ras_copycols(void *cookie, int row, int src, int dst, int n)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;
	struct cgsix_softc *sc = scr->scr_cookie;
	volatile struct cg6_fbc *fbc = sc->sc_fbc;

	if (dst == src)
		return;
	if ((row < 0) || (row >= ri->ri_rows))
		return;
	if (src < 0) {
		n += src;
		src = 0;
	}
	if (src+n > ri->ri_cols)
		n = ri->ri_cols - src;
	if (dst < 0) {
		n += dst;
		dst = 0;
	}
	if (dst+n > ri->ri_cols)
		n = ri->ri_cols - dst;
	if (n <= 0)
		return;
	n *= ri->ri_font->fontwidth;
	src *= ri->ri_font->fontwidth;
	dst *= ri->ri_font->fontwidth;
	row *= ri->ri_font->fontheight;

	CG6_WAIT_READY(fbc);

	fbc->fbc_alu = CG6_ALU_COPY;
	fbc->fbc_mode = GX_BLIT_SRC | GX_MODE_COLOR8;

	fbc->fbc_x0 = ri->ri_xorigin + src;
	fbc->fbc_y0 = ri->ri_yorigin + row;
	fbc->fbc_x1 = ri->ri_xorigin + src + n - 1;
	fbc->fbc_y1 = ri->ri_yorigin + row + 
	    ri->ri_font->fontheight - 1;
	fbc->fbc_x2 = ri->ri_xorigin + dst;
	fbc->fbc_y2 = ri->ri_yorigin + row;
	fbc->fbc_x3 = ri->ri_xorigin + dst + n - 1;
	fbc->fbc_y3 = ri->ri_yorigin + row + 
	    ri->ri_font->fontheight - 1;
	CG6_BLIT(fbc);
}

static void
cg6_ras_erasecols(void *cookie, int row, int col, int n, long int attr)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;
	struct cgsix_softc *sc = scr->scr_cookie;
	volatile struct cg6_fbc *fbc = sc->sc_fbc;

	if ((row < 0) || (row >= ri->ri_rows))
		return;
	if (col < 0) {
		n += col;
		col = 0;
	}
	if (col+n > ri->ri_cols)
		n = ri->ri_cols - col;
	if (n <= 0)
		return;
	n *= ri->ri_font->fontwidth;
	col *= ri->ri_font->fontwidth;
	row *= ri->ri_font->fontheight;

	CG6_WAIT_READY(fbc);
	fbc->fbc_alu = CG6_ALU_FILL;
	fbc->fbc_mode = GX_BLIT_SRC | GX_MODE_COLOR8;

	fbc->fbc_fg = ri->ri_devcmap[(attr >> 16) & 0xff];
	fbc->fbc_arecty = ri->ri_yorigin + row;
	fbc->fbc_arectx = ri->ri_xorigin + col;
	fbc->fbc_arecty = ri->ri_yorigin + row + 
	    ri->ri_font->fontheight - 1;
	fbc->fbc_arectx = ri->ri_xorigin + col + n - 1;
	CG6_DRAW(fbc);
}

static void
cg6_ras_eraserows(void *cookie, int row, int n, long int attr)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;
	struct cgsix_softc *sc = scr->scr_cookie;
	volatile struct cg6_fbc *fbc = sc->sc_fbc;

	if (row < 0) {
		n += row;
		row = 0;
	}
	if (row+n > ri->ri_rows)
		n = ri->ri_rows - row;
	if (n <= 0)
		return;

	CG6_WAIT_READY(fbc);
	fbc->fbc_alu = CG6_ALU_FILL;
	fbc->fbc_mode = GX_BLIT_SRC | GX_MODE_COLOR8;

	fbc->fbc_fg = ri->ri_devcmap[(attr >> 16) & 0xff];
	if ((n == ri->ri_rows) && (ri->ri_flg & RI_FULLCLEAR)) {
		fbc->fbc_arecty = 0;
		fbc->fbc_arectx = 0;
		fbc->fbc_arecty = ri->ri_height - 1;
		fbc->fbc_arectx = ri->ri_width - 1;
	} else {
		row *= ri->ri_font->fontheight;
		fbc->fbc_arecty = ri->ri_yorigin + row;
		fbc->fbc_arectx = ri->ri_xorigin;
		fbc->fbc_arecty = ri->ri_yorigin + row + 
		    (n * ri->ri_font->fontheight) - 1;
		fbc->fbc_arectx = ri->ri_xorigin + ri->ri_emuwidth - 1;
	}
	CG6_DRAW(fbc);
}

#if defined(RASTERCONSOLE) && defined(CG6_BLIT_CURSOR)
/*
 * Really want something more like fg^bg here, but that would be more
 * or less impossible to migrate to colors.  So we hope there's
 * something not too inappropriate in the colormap...besides, it's what
 * the non-accelerated code did. :-)
 */
static void
cg6_ras_do_cursor(struct rasops_info *ri)
{
	struct vcons_screen *scr = ri->ri_hw;
	struct cgsix_softc *sc = scr->cookie;
	int row, col;
	
	row = ri->ri_crow * ri->ri_font->fontheight;
	col = ri->ri_ccol * ri->ri_font->fontwidth;
	cg6_invert(sc, ri->ri_xorigin + col,ri->ri_yorigin + 
	    row, ri->ri_font->fontwidth, ri->ri_font->fontheight);
}
#endif	/* RASTERCONSOLE */

#endif	/* (NWSDISPLAY > 0) || defined(RASTERCONSOLE) */

void
cg6attach(struct cgsix_softc *sc, const char *name, int isconsole)
{
	struct fbdevice *fb = &sc->sc_fb;
#if NWSDISPLAY > 0
	struct wsemuldisplaydev_attach_args aa;
	struct rasops_info *ri = &cg6_console_screen.scr_ri;
	unsigned long defattr;
#endif
	
	fb->fb_driver = &cg6_fbdriver;

	/* Don't have to map the pfour register on the cgsix. */
	fb->fb_pfour = NULL;

	fb->fb_type.fb_cmsize = 256;
	fb->fb_type.fb_size = sc->sc_ramsize;

	printf(": %s, %d x %d", name,
	       fb->fb_type.fb_width, fb->fb_type.fb_height);
	if(sc->sc_fhc) {
		sc->sc_fhcrev = (*sc->sc_fhc >> FHC_REV_SHIFT) &
			(FHC_REV_MASK >> FHC_REV_SHIFT);
	} else
		sc->sc_fhcrev=-1;
	printf(", rev %d", sc->sc_fhcrev);

	/* reset cursor & frame buffer controls */
	cg6_reset(sc);

	/* enable video */
	sc->sc_thc->thc_misc |= THC_MISC_VIDEN;

	if (isconsole) {
		printf(" (console)");

/* this is the old console attachment stuff - sparc still needs it */
#ifdef RASTERCONSOLE
		if (cgsix_use_rasterconsole) {
			fbrcons_init(&sc->sc_fb);
			/* 
			 * we don't use the screen struct but keep it here to 
			 * avoid ugliness in the cg6_ras_* functions
			 */
			cg6_console_screen.scr_cookie = sc;
			sc->sc_fb.fb_rinfo.ri_hw = &cg6_console_screen;
			sc->sc_fb.fb_rinfo.ri_ops.copyrows = cg6_ras_copyrows;
			sc->sc_fb.fb_rinfo.ri_ops.copycols = cg6_ras_copycols;
			sc->sc_fb.fb_rinfo.ri_ops.erasecols = cg6_ras_erasecols;
			sc->sc_fb.fb_rinfo.ri_ops.eraserows = cg6_ras_eraserows;
#ifdef CG6_BLIT_CURSOR
			sc->sc_fb.fb_rinfo.ri_do_cursor = cg6_ras_do_cursor;
#endif
			cg6_ras_init(sc);
		}
#endif
	}
	printf("\n");

	fb_attach(&sc->sc_fb, isconsole);
	sc->sc_width = fb->fb_type.fb_width;
	sc->sc_stride = fb->fb_type.fb_width;
	sc->sc_height = fb->fb_type.fb_height;
	
	printf("%s: framebuffer size: %d MB\n", device_xname(sc->sc_dev), 
	    sc->sc_ramsize >> 20);

#if NWSDISPLAY
	/* setup rasops and so on for wsdisplay */
	memcpy(sc->sc_default_cmap, rasops_cmap, 768);
	wsfont_init();
	cg6_ras_init(sc);
	sc->sc_mode = WSDISPLAYIO_MODE_EMUL;
	sc->sc_bg = WS_DEFAULT_BG;
	sc->sc_fb_is_open = FALSE;
	
	vcons_init(&sc->vd, sc, &cgsix_defaultscreen, &cgsix_accessops);
	sc->vd.init_screen = cgsix_init_screen;

	sc->sc_gc.gc_bitblt = cgsix_bitblt;
	sc->sc_gc.gc_blitcookie = sc;
	sc->sc_gc.gc_rop = CG6_ALU_COPY;

	if(isconsole) {
		/* we mess with cg6_console_screen only once */
		vcons_init_screen(&sc->vd, &cg6_console_screen, 1,
		    &defattr);
		sc->sc_bg = (defattr >> 16) & 0xf; /* yes, this is an index into devcmap */
		
		/*
		 * XXX
		 * Is this actually necessary? We're going to use the blitter later on anyway.
		 */ 
		/* We need unaccelerated initial screen clear on old revisions */
		if (sc->sc_fhcrev < 2) {
			memset(sc->sc_fb.fb_pixels, ri->ri_devcmap[sc->sc_bg],
			    sc->sc_stride * sc->sc_height);
		} else
			cgsix_clearscreen(sc);

		cg6_console_screen.scr_flags |= VCONS_SCREEN_IS_STATIC;
		
		cgsix_defaultscreen.textops = &ri->ri_ops;
		cgsix_defaultscreen.capabilities = ri->ri_caps;
		cgsix_defaultscreen.nrows = ri->ri_rows;
		cgsix_defaultscreen.ncols = ri->ri_cols;
		SCREEN_VISIBLE(&cg6_console_screen);
		sc->vd.active = &cg6_console_screen;
		wsdisplay_cnattach(&cgsix_defaultscreen, ri, 0, 0, defattr);
		if (ri->ri_flg & RI_ENABLE_ALPHA) {
			glyphcache_init(&sc->sc_gc, sc->sc_height + 5,
				(sc->sc_ramsize / sc->sc_stride) - 
				  sc->sc_height - 5,
				sc->sc_width,
				ri->ri_font->fontwidth,
				ri->ri_font->fontheight,
				defattr);
		}	
		vcons_replay_msgbuf(&cg6_console_screen);
	} else {
		/*
		 * since we're not the console we can postpone the rest
		 * until someone actually allocates a screen for us
		 */
		if (cg6_console_screen.scr_ri.ri_rows == 0) {
			/* do some minimal setup to avoid weirdnesses later */
			vcons_init_screen(&sc->vd, &cg6_console_screen, 1,
			    &defattr);
		} else
			(*ri->ri_ops.allocattr)(ri, 0, 0, 0, &defattr);
		sc->sc_bg = (defattr >> 16) & 0xf;
		if (ri->ri_flg & RI_ENABLE_ALPHA) {
			glyphcache_init(&sc->sc_gc, sc->sc_height + 5,
				(sc->sc_ramsize / sc->sc_stride) - 
				  sc->sc_height - 5,
				sc->sc_width,
				ri->ri_font->fontwidth,
				ri->ri_font->fontheight,
				defattr);
		}
	}
	cg6_setup_palette(sc);
	
	aa.scrdata = &cgsix_screenlist;
	aa.console = isconsole;
	aa.accessops = &cgsix_accessops;
	aa.accesscookie = &sc->vd;
	config_found(sc->sc_dev, &aa, wsemuldisplaydevprint);
#else
	bt_initcmap(&sc->sc_cmap, 256);	
	cg6_loadcmap(sc, 0, 256);
	
#endif
}


int
cgsixopen(dev_t dev, int flags, int mode, struct lwp *l)
{
	device_t dv = device_lookup(&cgsix_cd, minor(dev));
	struct cgsix_softc *sc = device_private(dv);

	if (dv == NULL)
		return ENXIO;
	sc->sc_fb_is_open = TRUE;

	return 0;
}

int
cgsixclose(dev_t dev, int flags, int mode, struct lwp *l)
{
	device_t dv = device_lookup(&cgsix_cd, minor(dev));
	struct cgsix_softc *sc = device_private(dv);

	cg6_reset(sc);
	sc->sc_fb_is_open = FALSE;

#if NWSDISPLAY > 0
	if (IS_IN_EMUL_MODE(sc)) {
		struct vcons_screen *ms = sc->vd.active;

		cg6_ras_init(sc);
		cg6_setup_palette(sc);
		glyphcache_wipe(&sc->sc_gc);

		/* we don't know if the screen exists */
		if (ms != NULL)
			vcons_redraw_screen(ms);
	}
#else
	/* (re-)initialize the default color map */
	bt_initcmap(&sc->sc_cmap, 256);
	
	cg6_loadcmap(sc, 0, 256);
#endif
	return 0;
}

int
cgsixioctl(dev_t dev, u_long cmd, void *data, int flags, struct lwp *l)
{
	struct cgsix_softc *sc = device_lookup_private(&cgsix_cd, minor(dev));
	union cursor_cmap tcm;
	uint32_t image[32], mask[32];
	u_int count;
	int v, error;

#ifdef CGSIX_DEBUG
	printf("cgsixioctl(%lx)\n",cmd);
#endif

	switch (cmd) {

	case FBIOGTYPE:
		*(struct fbtype *)data = sc->sc_fb.fb_type;
		break;

	case FBIOGATTR:
#define fba ((struct fbgattr *)data)
		fba->real_type = sc->sc_fb.fb_type.fb_type;
		fba->owner = 0;		/* XXX ??? */
		fba->fbtype = sc->sc_fb.fb_type;
		fba->sattr.flags = 0;
		fba->sattr.emu_type = sc->sc_fb.fb_type.fb_type;
		fba->sattr.dev_specific[0] = -1;
		fba->emu_types[0] = sc->sc_fb.fb_type.fb_type;
		fba->emu_types[1] = -1;
#undef fba
		break;

	case FBIOGETCMAP:
#define	p ((struct fbcmap *)data)
		return (bt_getcmap(p, &sc->sc_cmap, 256, 1));

	case FBIOPUTCMAP:
		/* copy to software map */
		error = bt_putcmap(p, &sc->sc_cmap, 256, 1);
		if (error)
			return error;
		/* now blast them into the chip */
		/* XXX should use retrace interrupt */
		cg6_loadcmap(sc, p->index, p->count);
#undef p
		break;

	case FBIOGVIDEO:
		*(int *)data = sc->sc_blanked;
		break;

	case FBIOSVIDEO:
		cg6_blank(sc, !(*(int *)data));
		break;

/* these are for both FBIOSCURSOR and FBIOGCURSOR */
#define p ((struct fbcursor *)data)
#define cc (&sc->sc_cursor)

	case FBIOGCURSOR:
		/* do not quite want everything here... */
		p->set = FB_CUR_SETALL;	/* close enough, anyway */
		p->enable = cc->cc_enable;
		p->pos = cc->cc_pos;
		p->hot = cc->cc_hot;
		p->size = cc->cc_size;

		/* begin ugh ... can we lose some of this crap?? */
		if (p->image != NULL) {
			count = cc->cc_size.y * 32 / NBBY;
			error = copyout(cc->cc_bits[1], p->image, count);
			if (error)
				return error;
			error = copyout(cc->cc_bits[0], p->mask, count);
			if (error)
				return error;
		}
		if (p->cmap.red != NULL) {
			error = bt_getcmap(&p->cmap,
			    (union bt_cmap *)&cc->cc_color, 2, 1);
			if (error)
				return error;
		} else {
			p->cmap.index = 0;
			p->cmap.count = 2;
		}
		/* end ugh */
		break;

	case FBIOSCURSOR:
		/*
		 * For setcmap and setshape, verify parameters, so that
		 * we do not get halfway through an update and then crap
		 * out with the software state screwed up.
		 */
		v = p->set;
		if (v & FB_CUR_SETCMAP) {
			/*
			 * This use of a temporary copy of the cursor
			 * colormap is not terribly efficient, but these
			 * copies are small (8 bytes)...
			 */
			tcm = cc->cc_color;
			error = bt_putcmap(&p->cmap, (union bt_cmap *)&tcm, 2, 
			    1);
			if (error)
				return error;
		}
		if (v & FB_CUR_SETSHAPE) {
			if ((u_int)p->size.x > 32 || (u_int)p->size.y > 32)
				return EINVAL;
			count = p->size.y * 32 / NBBY;
			error = copyin(p->image, image, count);
			if (error)
				return error;
			error = copyin(p->mask, mask, count);
			if (error)
				return error;
		}

		/* parameters are OK; do it */
		if (v & (FB_CUR_SETCUR | FB_CUR_SETPOS | FB_CUR_SETHOT)) {
			if (v & FB_CUR_SETCUR)
				cc->cc_enable = p->enable;
			if (v & FB_CUR_SETPOS)
				cc->cc_pos = p->pos;
			if (v & FB_CUR_SETHOT)
				cc->cc_hot = p->hot;
			cg6_setcursor(sc);
		}
		if (v & FB_CUR_SETCMAP) {
			cc->cc_color = tcm;
			cg6_loadomap(sc); /* XXX defer to vertical retrace */
		}
		if (v & FB_CUR_SETSHAPE) {
			cc->cc_size = p->size;
			count = p->size.y * 32 / NBBY;
			memset(cc->cc_bits, 0, sizeof cc->cc_bits);
			memcpy(cc->cc_bits[1], image, count);
			memcpy(cc->cc_bits[0], mask, count);
			cg6_loadcursor(sc);
		}
		break;

#undef p
#undef cc

	case FBIOGCURPOS:
		*(struct fbcurpos *)data = sc->sc_cursor.cc_pos;
		break;

	case FBIOSCURPOS:
		sc->sc_cursor.cc_pos = *(struct fbcurpos *)data;
		cg6_setcursor(sc);
		break;

	case FBIOGCURMAX:
		/* max cursor size is 32x32 */
		((struct fbcurpos *)data)->x = 32;
		((struct fbcurpos *)data)->y = 32;
		break;

	default:
#ifdef DEBUG
		log(LOG_NOTICE, "cgsixioctl(0x%lx) (%s[%d])\n", cmd,
		    l->l_proc->p_comm, l->l_proc->p_pid);
#endif
		return ENOTTY;
	}
	return 0;
}

/*
 * Clean up hardware state (e.g., after bootup or after X crashes).
 */
static void
cg6_reset(struct cgsix_softc *sc)
{
	volatile struct cg6_tec_xxx *tec;
	int fhc;
	volatile struct bt_regs *bt;

	/* hide the cursor, just in case */
	sc->sc_thc->thc_cursxy = (THC_CURSOFF << 16) | THC_CURSOFF;

	/* turn off frobs in transform engine (makes X11 work) */
	tec = sc->sc_tec;
	tec->tec_mv = 0;
	tec->tec_clip = 0;
	tec->tec_vdc = 0;

	/* take care of hardware bugs in old revisions */
	if (sc->sc_fhcrev < 5) {
		/*
		 * Keep current resolution; set CPU to 68020, set test
		 * window (size 1Kx1K), and for rev 1, disable dest cache.
		 */
		fhc = (*sc->sc_fhc & FHC_RES_MASK) | FHC_CPU_68020 |
		    FHC_TEST |
		    (11 << FHC_TESTX_SHIFT) | (11 << FHC_TESTY_SHIFT);
		if (sc->sc_fhcrev < 2)
			fhc |= FHC_DST_DISABLE;
		*sc->sc_fhc = fhc;
	}

	/* Enable cursor in Brooktree DAC. */
	bt = sc->sc_bt;
	bt->bt_addr = 0x06 << 24;
	bt->bt_ctrl |= 0x03 << 24;
}

static void
cg6_setcursor(struct cgsix_softc *sc)
{

	/* we need to subtract the hot-spot value here */
#define COORD(f) (sc->sc_cursor.cc_pos.f - sc->sc_cursor.cc_hot.f)
	sc->sc_thc->thc_cursxy = sc->sc_cursor.cc_enable ?
	    ((COORD(x) << 16) | (COORD(y) & 0xffff)) :
	    (THC_CURSOFF << 16) | THC_CURSOFF;
#undef COORD
}

static void
cg6_loadcursor(struct cgsix_softc *sc)
{
	volatile struct cg6_thc *thc;
	u_int edgemask, m;
	int i;

	/*
	 * Keep the top size.x bits.  Here we *throw out* the top
	 * size.x bits from an all-one-bits word, introducing zeros in
	 * the top size.x bits, then invert all the bits to get what
	 * we really wanted as our mask.  But this fails if size.x is
	 * 32---a sparc uses only the low 5 bits of the shift count---
	 * so we have to special case that.
	 */
	edgemask = ~0;
	if (sc->sc_cursor.cc_size.x < 32)
		edgemask = ~(edgemask >> sc->sc_cursor.cc_size.x);
	thc = sc->sc_thc;
	for (i = 0; i < 32; i++) {
		m = sc->sc_cursor.cc_bits[0][i] & edgemask;
		thc->thc_cursmask[i] = m;
		thc->thc_cursbits[i] = m & sc->sc_cursor.cc_bits[1][i];
	}
}

/*
 * Load a subset of the current (new) colormap into the color DAC.
 */
static void
cg6_loadcmap(struct cgsix_softc *sc, int start, int ncolors)
{
	volatile struct bt_regs *bt;
	u_int *ip, i;
	int count;

	ip = &sc->sc_cmap.cm_chip[BT_D4M3(start)];	/* start/4 * 3 */
	count = BT_D4M3(start + ncolors - 1) - BT_D4M3(start) + 3;
	bt = sc->sc_bt;
	bt->bt_addr = BT_D4M4(start) << 24;
	while (--count >= 0) {
		i = *ip++;
		/* hardware that makes one want to pound boards with hammers */
		bt->bt_cmap = i;
		bt->bt_cmap = i << 8;
		bt->bt_cmap = i << 16;
		bt->bt_cmap = i << 24;
	}
}

/*
 * Load the cursor (overlay `foreground' and `background') colors.
 */
static void
cg6_loadomap(struct cgsix_softc *sc)
{
	volatile struct bt_regs *bt;
	u_int i;

	bt = sc->sc_bt;
	bt->bt_addr = 0x01 << 24;	/* set background color */
	i = sc->sc_cursor.cc_color.cm_chip[0];
	bt->bt_omap = i;		/* R */
	bt->bt_omap = i << 8;		/* G */
	bt->bt_omap = i << 16;		/* B */

	bt->bt_addr = 0x03 << 24;	/* set foreground color */
	bt->bt_omap = i << 24;		/* R */
	i = sc->sc_cursor.cc_color.cm_chip[1];
	bt->bt_omap = i;		/* G */
	bt->bt_omap = i << 8;		/* B */
}

/* blank or unblank the screen */
static void
cg6_blank(struct cgsix_softc *sc, int flag)
{

	if (sc->sc_blanked != flag) {
		sc->sc_blanked = flag;
		if (flag) {
			sc->sc_thc->thc_misc &= ~THC_MISC_VIDEN;
		} else {
			sc->sc_thc->thc_misc |= THC_MISC_VIDEN;
		}
	}
}

/* 
 * this is called on panic or ddb entry - force the console to the front, reset 
 * the colour map and enable drawing so we actually see the message even when X
 * is running
 */
static void
cg6_unblank(device_t dev)
{
	struct cgsix_softc *sc = device_private(dev);

	cg6_blank(sc, 0);
}

/* XXX the following should be moved to a "user interface" header */
/*
 * Base addresses at which users can mmap() the various pieces of a cg6.
 * Note that although the Brooktree color registers do not occupy 8K,
 * the X server dies if we do not allow it to map 8K there (it just maps
 * from 0x70000000 forwards, as a contiguous chunk).
 */
#define	CG6_USER_FBC	0x70000000
#define	CG6_USER_TEC	0x70001000
#define	CG6_USER_BTREGS	0x70002000
#define	CG6_USER_FHC	0x70004000
#define	CG6_USER_THC	0x70005000
#define	CG6_USER_ROM	0x70006000
#define	CG6_USER_RAM	0x70016000
#define	CG6_USER_DHC	0x80000000

struct mmo {
	u_long	mo_uaddr;	/* user (virtual) address */
	u_long	mo_size;	/* size, or 0 for video ram size */
	u_long	mo_physoff;	/* offset from sc_physadr */
};

/*
 * Return the address that would map the given device at the given
 * offset, allowing for the given protection, or return -1 for error.
 *
 * XXX	needs testing against `demanding' applications (e.g., aviator)
 */
paddr_t
cgsixmmap(dev_t dev, off_t off, int prot)
{
	struct cgsix_softc *sc = device_lookup_private(&cgsix_cd, minor(dev));
	struct mmo *mo;
	u_int u, sz;
	static struct mmo mmo[] = {
		{ CG6_USER_RAM, 0, CGSIX_RAM_OFFSET },

		/* do not actually know how big most of these are! */
		{ CG6_USER_FBC, 1, CGSIX_FBC_OFFSET },
		{ CG6_USER_TEC, 1, CGSIX_TEC_OFFSET },
		{ CG6_USER_BTREGS, 8192 /* XXX */, CGSIX_BT_OFFSET },
		{ CG6_USER_FHC, 1, CGSIX_FHC_OFFSET },
		{ CG6_USER_THC, sizeof(struct cg6_thc), CGSIX_THC_OFFSET },
		{ CG6_USER_ROM, 65536, CGSIX_ROM_OFFSET },
		{ CG6_USER_DHC, 1, CGSIX_DHC_OFFSET },
	};
#define NMMO (sizeof mmo / sizeof *mmo)

	if (off & PGOFSET)
		panic("cgsixmmap");

	/*
	 * Entries with size 0 map video RAM (i.e., the size in fb data).
	 *
	 * Since we work in pages, the fact that the map offset table's
	 * sizes are sometimes bizarre (e.g., 1) is effectively ignored:
	 * one byte is as good as one page.
	 */
	for (mo = mmo; mo < &mmo[NMMO]; mo++) {
		if ((u_long)off < mo->mo_uaddr)
			continue;
		u = off - mo->mo_uaddr;
		sz = mo->mo_size ? mo->mo_size : 
		    sc->sc_ramsize;
		if (u < sz) {
			return (bus_space_mmap(sc->sc_bustag,
				sc->sc_paddr, u+mo->mo_physoff,
				prot, BUS_SPACE_MAP_LINEAR));
		}
	}

#ifdef DEBUG
	{
	  struct proc *p = curlwp->l_proc;	/* XXX */
	  log(LOG_NOTICE, "cgsixmmap(0x%llx) (%s[%d])\n",
		(long long)off, p->p_comm, p->p_pid);
	}
#endif
	return -1;	/* not a user-map offset */
}

#if NWSDISPLAY > 0

static void
cg6_setup_palette(struct cgsix_softc *sc)
{
	int i, j;

	rasops_get_cmap(&cg6_console_screen.scr_ri, sc->sc_default_cmap,
	    sizeof(sc->sc_default_cmap));
	j = 0;
	for (i = 0; i < 256; i++) {
		sc->sc_cmap.cm_map[i][0] = sc->sc_default_cmap[j];
		j++;
		sc->sc_cmap.cm_map[i][1] = sc->sc_default_cmap[j];
		j++;
		sc->sc_cmap.cm_map[i][2] = sc->sc_default_cmap[j];
		j++;
	}
	cg6_loadcmap(sc, 0, 256);
}

int
cgsix_ioctl(void *v, void *vs, u_long cmd, void *data, int flag,
	struct lwp *l)
{
	/* we'll probably need to add more stuff here */
	struct vcons_data *vd = v;
	struct cgsix_softc *sc = vd->cookie;
	struct wsdisplay_fbinfo *wdf;
	struct rasops_info *ri = &sc->sc_fb.fb_rinfo;
	struct vcons_screen *ms = sc->vd.active;

#ifdef CGSIX_DEBUG
	printf("cgsix_ioctl(%lx)\n",cmd);
#endif
	switch (cmd) {
		case WSDISPLAYIO_GTYPE:
			*(u_int *)data = WSDISPLAY_TYPE_SUNTCX;
			return 0;
		case WSDISPLAYIO_GINFO:
			wdf = (void *)data;
			wdf->height = ri->ri_height;
			wdf->width = ri->ri_width;
			wdf->depth = ri->ri_depth;
			wdf->cmsize = 256;
			return 0;

		case WSDISPLAYIO_GETCMAP:
			return cgsix_getcmap(sc, 
			    (struct wsdisplay_cmap *)data);
		case WSDISPLAYIO_PUTCMAP:
			return cgsix_putcmap(sc, 
			    (struct wsdisplay_cmap *)data);

		case WSDISPLAYIO_LINEBYTES:
			*(u_int *)data = sc->sc_stride;
			return 0;

		case WSDISPLAYIO_SMODE:
			{
				int new_mode = *(int*)data;

				if (new_mode != sc->sc_mode) {
					sc->sc_mode = new_mode;
					if (IS_IN_EMUL_MODE(sc)) {
						cg6_reset(sc);
						cg6_ras_init(sc);
						cg6_setup_palette(sc);
						glyphcache_wipe(&sc->sc_gc);
						vcons_redraw_screen(ms);
					}
				}
			}
	}
	return EPASSTHROUGH;
}

paddr_t
cgsix_mmap(void *v, void *vs, off_t offset, int prot)
{
	struct vcons_data *vd = v;
	struct cgsix_softc *sc = vd->cookie;

	if (offset < sc->sc_ramsize) {
		return bus_space_mmap(sc->sc_bustag, sc->sc_paddr,
		    CGSIX_RAM_OFFSET + offset, prot, BUS_SPACE_MAP_LINEAR);
	}
	return -1;
}

int
cgsix_putcmap(struct cgsix_softc *sc, struct wsdisplay_cmap *cm)
{
	u_int index = cm->index;
	u_int count = cm->count;
	int error, i;

	if (index >= 256 || count > 256 || index + count > 256)
		return EINVAL;

	for (i = 0; i < count; i++)
	{
		error = copyin(&cm->red[i],
		    &sc->sc_cmap.cm_map[index + i][0], 1);
		if (error)
			return error;
		error = copyin(&cm->green[i],
		    &sc->sc_cmap.cm_map[index + i][1],
		    1);
		if (error)
			return error;
		error = copyin(&cm->blue[i],
		    &sc->sc_cmap.cm_map[index + i][2], 1);
		if (error)
			return error;
	}
	cg6_loadcmap(sc, index, count);

	return 0;
}

int
cgsix_getcmap(struct cgsix_softc *sc, struct wsdisplay_cmap *cm)
{
	u_int index = cm->index;
	u_int count = cm->count;
	int error,i;

	if (index >= 256 || count > 256 || index + count > 256)
		return EINVAL;

	for (i = 0; i < count; i++)
	{
		error = copyout(&sc->sc_cmap.cm_map[index + i][0],
		    &cm->red[i], 1);
		if (error)
			return error;
		error = copyout(&sc->sc_cmap.cm_map[index + i][1],
		    &cm->green[i], 1);
		if (error)
			return error;
		error = copyout(&sc->sc_cmap.cm_map[index + i][2],
		    &cm->blue[i], 1);
		if (error)
			return error;
	}

	return 0;
}

void
cgsix_init_screen(void *cookie, struct vcons_screen *scr,
    int existing, long *defattr)
{
	struct cgsix_softc *sc = cookie;
	struct rasops_info *ri = &scr->scr_ri;
	int av;

	ri->ri_depth = 8;
	ri->ri_width = sc->sc_width;
	ri->ri_height = sc->sc_height;
	ri->ri_stride = sc->sc_stride;
	av = sc->sc_ramsize - (sc->sc_height * sc->sc_stride);
	ri->ri_flg = RI_CENTER  | RI_8BIT_IS_RGB;
	if (av > (128 * 1024)) {
		ri->ri_flg |= RI_ENABLE_ALPHA;
	}
	ri->ri_bits = sc->sc_fb.fb_pixels;
	
	rasops_init(ri, 0, 0);
	ri->ri_caps = WSSCREEN_WSCOLORS | WSSCREEN_REVERSE;
	rasops_reconfig(ri, sc->sc_height / ri->ri_font->fontheight,
		    sc->sc_width / ri->ri_font->fontwidth);

	/* enable acceleration */
	ri->ri_hw = scr;
	ri->ri_ops.copyrows = cg6_ras_copyrows;
	ri->ri_ops.copycols = cg6_ras_copycols;
	ri->ri_ops.eraserows = cg6_ras_eraserows;
	ri->ri_ops.erasecols = cg6_ras_erasecols;
	ri->ri_ops.cursor = cgsix_cursor;
	if (FONT_IS_ALPHA(ri->ri_font)) {
		ri->ri_ops.putchar = cgsix_putchar_aa;
	} else
		ri->ri_ops.putchar = cgsix_putchar;
}

void 
cgsix_rectfill(struct cgsix_softc *sc, int xs, int ys, int wi, int he, 
    uint32_t col)
{
	volatile struct cg6_fbc *fbc = sc->sc_fbc;

	CG6_WAIT_READY(fbc);

	fbc->fbc_alu = CG6_ALU_FILL;
	fbc->fbc_mode = GX_BLIT_SRC | GX_MODE_COLOR8;

	fbc->fbc_fg = col;
	fbc->fbc_arecty = ys;
	fbc->fbc_arectx = xs;
	fbc->fbc_arecty = ys + he - 1;
	fbc->fbc_arectx = xs + wi - 1;
	CG6_DRAW(fbc);
}

void
cgsix_bitblt(void *cookie, int xs, int ys, int xd, int yd,
    int wi, int he, int rop)
{
	struct cgsix_softc *sc = cookie;
	volatile struct cg6_fbc *fbc = sc->sc_fbc;
	CG6_WAIT_READY(fbc);

	fbc->fbc_alu = rop;
	fbc->fbc_mode = GX_BLIT_SRC | GX_MODE_COLOR8;

	fbc->fbc_x0 = xs;
	fbc->fbc_y0 = ys;
	fbc->fbc_x1 = xs + wi - 1;
	fbc->fbc_y1 = ys + he - 1;
	fbc->fbc_x2 = xd;
	fbc->fbc_y2 = yd;
	fbc->fbc_x3 = xd + wi - 1;
	fbc->fbc_y3 = yd + he - 1;
	CG6_BLIT(fbc);
}

void 
cgsix_setup_mono(struct cgsix_softc *sc, int x, int y, int wi, int he, 
    uint32_t fg, uint32_t bg) 
{
	volatile struct cg6_fbc *fbc=sc->sc_fbc;

	CG6_WAIT_READY(fbc);

	fbc->fbc_x0 = x;
	fbc->fbc_x1 = x + wi - 1;
	fbc->fbc_y0 = y;
	fbc->fbc_incx = 0;
	fbc->fbc_incy = 1;
	fbc->fbc_fg = fg;
	fbc->fbc_bg = bg;
	fbc->fbc_mode = GX_BLIT_NOSRC | GX_MODE_COLOR1;
	fbc->fbc_alu = GX_PATTERN_ONES | ROP_OSTP(GX_ROP_CLEAR, GX_ROP_SET);
	sc->sc_mono_width = wi;
	/* now feed the data into the chip */
}

void 
cgsix_feed_line(struct cgsix_softc *sc, int count, uint8_t *data)
{
	int i;
	uint32_t latch, res = 0, shift;
	volatile struct cg6_fbc *fbc = sc->sc_fbc;
	
	if (sc->sc_mono_width > 32) {
		/* ARGH! */
	} else
	{
		shift = 24;
		for (i = 0; i < count; i++) {
			latch = data[i];
			res |= latch << shift;
			shift -= 8;
		}
		fbc->fbc_font = res;
	}
}	

void
cgsix_putchar(void *cookie, int row, int col, u_int c, long attr)
{
	struct rasops_info *ri = cookie;
	struct wsdisplay_font *font = PICK_FONT(ri, c);
	struct vcons_screen *scr = ri->ri_hw;
	struct cgsix_softc *sc = scr->scr_cookie;
	int inv;
	
	if ((row >= 0) && (row < ri->ri_rows) && (col >= 0) && 
	    (col < ri->ri_cols)) {

		if (sc->sc_mode == WSDISPLAYIO_MODE_EMUL) {

			int fg, bg, uc, i;
			uint8_t *data;
			int x, y, wi, he;
			volatile struct cg6_fbc *fbc = sc->sc_fbc;

			wi = font->fontwidth;
			he = font->fontheight;
			
			if (!CHAR_IN_FONT(c, font))
				return;
			inv = ((attr >> 8) & WSATTR_REVERSE);
			if (inv) {
				fg = (u_char)ri->ri_devcmap[(attr >> 16) & 
				    0xff];
				bg = (u_char)ri->ri_devcmap[(attr >> 24) &
				    0xff];
			} else {
				bg = (u_char)ri->ri_devcmap[(attr >> 16) &
				    0xff];
				fg = (u_char)ri->ri_devcmap[(attr >> 24) &
				    0xff];
			}

			x = ri->ri_xorigin + col * wi;
			y = ri->ri_yorigin + row * he;

			if (c == 0x20) {
				cgsix_rectfill(sc, x, y, wi, he, bg);
			} else {
				uc = c - font->firstchar;
				data = (uint8_t *)font->data + uc * 
				    ri->ri_fontscale;

				cgsix_setup_mono(sc, x, y, wi, 1, fg, bg);		
				for (i = 0; i < he; i++) {
					cgsix_feed_line(sc, font->stride,
					    data);
					data += font->stride;
				}
				/* put the chip back to normal */
				fbc->fbc_incy = 0;
			}
		}
	}
}

void
cgsix_putchar_aa(void *cookie, int row, int col, u_int c, long attr)
{
	struct rasops_info *ri = cookie;
	struct wsdisplay_font *font = PICK_FONT(ri, c);
	struct vcons_screen *scr = ri->ri_hw;
	struct cgsix_softc *sc = scr->scr_cookie;
	volatile struct cg6_fbc *fbc = sc->sc_fbc;

	uint32_t bg, latch = 0, bg8, fg8, pixel;
	int i, j, shift, x, y, wi, he, r, g, b, aval;
	int r1, g1, b1, r0, g0, b0, fgo, bgo;
	uint8_t *data8;
	int rv;

	if (sc->sc_mode != WSDISPLAYIO_MODE_EMUL) 
		return;

	if (!CHAR_IN_FONT(c, font))
		return;

	wi = font->fontwidth;
	he = font->fontheight;

	bg = ri->ri_devcmap[(attr >> 16) & 0xf];
	x = ri->ri_xorigin + col * wi;
	y = ri->ri_yorigin + row * he;
	if (c == 0x20) {
		cgsix_rectfill(sc, x, y, wi, he, bg);
		return;
	}

	rv = glyphcache_try(&sc->sc_gc, c, x, y, attr);
	if (rv == GC_OK)
		return;

	data8 = WSFONT_GLYPH(c, font);

	CG6_WAIT_READY(sc->sc_fbc);
	fbc->fbc_incx = 4;
	fbc->fbc_incy = 0;
	fbc->fbc_mode = GX_BLIT_NOSRC | GX_MODE_COLOR8;
	fbc->fbc_alu = CG6_ALU_COPY;
	fbc->fbc_clipmaxx = x + wi - 1;

	/*
	 * we need the RGB colours here, so get offsets into rasops_cmap
	 */
	fgo = ((attr >> 24) & 0xf) * 3;
	bgo = ((attr >> 16) & 0xf) * 3;

	r0 = rasops_cmap[bgo];
	r1 = rasops_cmap[fgo];
	g0 = rasops_cmap[bgo + 1];
	g1 = rasops_cmap[fgo + 1];
	b0 = rasops_cmap[bgo + 2];
	b1 = rasops_cmap[fgo + 2];
#define R3G3B2(r, g, b) ((r & 0xe0) | ((g >> 3) & 0x1c) | (b >> 6))
	bg8 = R3G3B2(r0, g0, b0);
	fg8 = R3G3B2(r1, g1, b1);

	for (i = 0; i < he; i++) {

		CG6_WAIT_READY(fbc);
		fbc->fbc_x0 = x;
		fbc->fbc_x1 = x + 3;
		fbc->fbc_y0 = y + i;

		shift = 24;
		for (j = 0; j < wi; j++) {
			aval = *data8;
			if (aval == 0) {
				pixel = bg8;
			} else if (aval == 255) {
				pixel = fg8;
			} else {
				r = aval * r1 + (255 - aval) * r0;
				g = aval * g1 + (255 - aval) * g0;
				b = aval * b1 + (255 - aval) * b0;
				pixel = ((r & 0xe000) >> 8) |
					((g & 0xe000) >> 11) |
					((b & 0xc000) >> 14);
			}
			data8++;

			latch |= pixel << shift;
			if (shift == 0) {
				fbc->fbc_font = latch;
				latch = 0;
				shift = 24;
			} else
				shift -= 8;
		}
		if (shift != 24)
			fbc->fbc_font = latch;
	}
	fbc->fbc_clipmaxx = 0x3fff;

	if (rv == GC_ADD) {
		glyphcache_add(&sc->sc_gc, c, x, y);
	}
}

void
cgsix_cursor(void *cookie, int on, int row, int col)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;
	struct cgsix_softc *sc = scr->scr_cookie;
	int x, y, wi, he;
	
	wi = ri->ri_font->fontwidth;
	he = ri->ri_font->fontheight;
	
	if (sc->sc_mode == WSDISPLAYIO_MODE_EMUL) {
		x = ri->ri_ccol * wi + ri->ri_xorigin;
		y = ri->ri_crow * he + ri->ri_yorigin;
		if (ri->ri_flg & RI_CURSOR) {
			cg6_invert(sc, x, y, wi, he);
			ri->ri_flg &= ~RI_CURSOR;
		}
		ri->ri_crow = row;
		ri->ri_ccol = col;
		if (on)
		{
			x = ri->ri_ccol * wi + ri->ri_xorigin;
			y = ri->ri_crow * he + ri->ri_yorigin;
			cg6_invert(sc, x, y, wi, he);
			ri->ri_flg |= RI_CURSOR;
		}
	} else
	{
		ri->ri_crow = row;
		ri->ri_ccol = col;
		ri->ri_flg &= ~RI_CURSOR;
	}
}

void
cgsix_clearscreen(struct cgsix_softc *sc)
{
	struct rasops_info *ri = &cg6_console_screen.scr_ri;
	
	if (sc->sc_mode == WSDISPLAYIO_MODE_EMUL) {
		volatile struct cg6_fbc *fbc = sc->sc_fbc;
		
		CG6_WAIT_READY(fbc);

		fbc->fbc_alu = CG6_ALU_FILL;
		fbc->fbc_mode = GX_BLIT_SRC | GX_MODE_COLOR8;

		fbc->fbc_fg = ri->ri_devcmap[sc->sc_bg];
		fbc->fbc_arectx = 0;
		fbc->fbc_arecty = 0;
		fbc->fbc_arectx = ri->ri_width - 1;
		fbc->fbc_arecty = ri->ri_height - 1;
		CG6_DRAW(fbc);
	}
}

#endif /* NWSDISPLAY > 0 */

#if (NWSDISPLAY > 0) || defined(RASTERCONSOLE)
void
cg6_invert(struct cgsix_softc *sc, int x, int y, int wi, int he)
{
	volatile struct cg6_fbc *fbc = sc->sc_fbc;
	
	CG6_WAIT_READY(fbc);

	fbc->fbc_alu = CG6_ALU_FLIP;
	fbc->fbc_mode = GX_BLIT_SRC | GX_MODE_COLOR8;
	fbc->fbc_arecty = y;
	fbc->fbc_arectx = x;
	fbc->fbc_arecty = y + he - 1;
	fbc->fbc_arectx = x + wi - 1;
	CG6_DRAW(fbc);
}

#endif

