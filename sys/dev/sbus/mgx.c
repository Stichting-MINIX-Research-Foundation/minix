/*	$NetBSD: mgx.c,v 1.4 2015/01/06 17:41:30 macallan Exp $ */

/*-
 * Copyright (c) 2014 Michael Lorenz
 * All rights reserved.
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

/* a console driver for the SSB 4096V-MGX graphics card */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: mgx.c,v 1.4 2015/01/06 17:41:30 macallan Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/conf.h>
#include <sys/kmem.h>

#include <sys/bus.h>
#include <machine/autoconf.h>

#include <dev/sbus/sbusvar.h>
#include <dev/sun/fbio.h>
#include <dev/sun/fbvar.h>

#include <dev/wscons/wsdisplayvar.h>
#include <dev/wscons/wsconsio.h>
#include <dev/wsfont/wsfont.h>
#include <dev/rasops/rasops.h>

#include <dev/wscons/wsdisplay_vconsvar.h>
#include <dev/wscons/wsdisplay_glyphcachevar.h>

#include <dev/ic/vgareg.h>
#include <dev/sbus/mgxreg.h>

#include "opt_wsemul.h"


struct mgx_softc {
	device_t	sc_dev;
	bus_space_tag_t sc_tag;
	bus_space_handle_t sc_blith;
	bus_space_handle_t sc_vgah;
	bus_addr_t	sc_paddr;
	void		*sc_fbaddr;
	int		sc_width;
	int		sc_height;
	int		sc_stride;
	int		sc_fbsize;
	int		sc_mode;
	uint32_t	sc_dec;
	u_char		sc_cmap_red[256];
	u_char		sc_cmap_green[256];
	u_char		sc_cmap_blue[256];
	void (*sc_putchar)(void *, int, int, u_int, long);
	struct vcons_screen sc_console_screen;
	struct wsscreen_descr sc_defaultscreen_descr;
	const struct wsscreen_descr *sc_screens[1];
	struct wsscreen_list sc_screenlist;
	struct vcons_data vd;
	glyphcache 	sc_gc;
};

static int	mgx_match(device_t, cfdata_t, void *);
static void	mgx_attach(device_t, device_t, void *);
static int	mgx_ioctl(void *, void *, u_long, void *, int,
				 struct lwp*);
static paddr_t	mgx_mmap(void *, void *, off_t, int);
static void	mgx_init_screen(void *, struct vcons_screen *, int,
				 long *);
static void	mgx_write_dac(struct mgx_softc *, int, int, int, int);
static void	mgx_setup(struct mgx_softc *, int);
static void	mgx_init_palette(struct mgx_softc *);
static int	mgx_putcmap(struct mgx_softc *, struct wsdisplay_cmap *);
static int 	mgx_getcmap(struct mgx_softc *, struct wsdisplay_cmap *);
static int	mgx_wait_engine(struct mgx_softc *);
static int	mgx_wait_fifo(struct mgx_softc *, unsigned int);

static void	mgx_bitblt(void *, int, int, int, int, int, int, int);
static void 	mgx_rectfill(void *, int, int, int, int, long);

static void	mgx_putchar(void *, int, int, u_int, long);
static void	mgx_cursor(void *, int, int, int);
static void	mgx_copycols(void *, int, int, int, int);
static void	mgx_erasecols(void *, int, int, int, long);
static void	mgx_copyrows(void *, int, int, int);
static void	mgx_eraserows(void *, int, int, long);

CFATTACH_DECL_NEW(mgx, sizeof(struct mgx_softc),
    mgx_match, mgx_attach, NULL, NULL);

struct wsdisplay_accessops mgx_accessops = {
	mgx_ioctl,
	mgx_mmap,
	NULL,	/* vcons_alloc_screen */
	NULL,	/* vcons_free_screen */
	NULL,	/* vcons_show_screen */
	NULL,	/* load_font */
	NULL,	/* polls */
	NULL,	/* scroll */
};

static int
mgx_match(device_t parent, cfdata_t cf, void *aux)
{
	struct sbus_attach_args *sa = aux;

	if (strcmp("SMSI,mgx", sa->sa_name) == 0)
		return 100;
	return 0;
}

/*
 * Attach a display.  We need to notice if it is the console, too.
 */
static void
mgx_attach(device_t parent, device_t self, void *args)
{
	struct mgx_softc *sc = device_private(self);
	struct sbus_attach_args *sa = args;
	struct wsemuldisplaydev_attach_args aa;
	struct rasops_info *ri;
	unsigned long defattr;
	bus_space_handle_t bh;
	int node = sa->sa_node;
	int isconsole;

	aprint_normal("\n");
	sc->sc_dev = self;
	sc->sc_tag = sa->sa_bustag;

	sc->sc_paddr = sbus_bus_addr(sa->sa_bustag, sa->sa_slot,
	    sa->sa_reg[8].oa_base);

	/* read geometry information from the device tree */
	sc->sc_width = prom_getpropint(sa->sa_node, "width", 1152);
	sc->sc_height = prom_getpropint(sa->sa_node, "height", 900);
	sc->sc_stride = prom_getpropint(sa->sa_node, "linebytes", 900);
	sc->sc_fbsize = sc->sc_height * sc->sc_stride;
	sc->sc_fbaddr = NULL;
	if (sc->sc_fbaddr == NULL) {
		if (sbus_bus_map(sa->sa_bustag,
			 sa->sa_slot,
			 sa->sa_reg[8].oa_base,
			 sc->sc_fbsize,
			 BUS_SPACE_MAP_LINEAR | BUS_SPACE_MAP_LARGE,
			 &bh) != 0) {
			aprint_error_dev(self, "cannot map framebuffer\n");
			return;
		}
		sc->sc_fbaddr = bus_space_vaddr(sa->sa_bustag, bh);
	}
		
	aprint_normal_dev(self, "%d x %d\n", sc->sc_width, sc->sc_height);

	if (sbus_bus_map(sa->sa_bustag,
			 sa->sa_slot,
			 sa->sa_reg[4].oa_base, 0x1000, 0,
			 &sc->sc_vgah) != 0) {
		aprint_error("%s: couldn't map VGA registers\n", 
		    device_xname(sc->sc_dev));
		return;
	}

	if (sbus_bus_map(sa->sa_bustag,
			 sa->sa_slot,
			 sa->sa_reg[5].oa_base + MGX_REG_ATREG_OFFSET, 0x1000,
			 0, &sc->sc_blith) != 0) {
		aprint_error("%s: couldn't map blitter registers\n", 
		    device_xname(sc->sc_dev));
		return;
	}

	mgx_setup(sc, 8);

	sc->sc_defaultscreen_descr = (struct wsscreen_descr) {
		"default",
		0, 0,
		NULL,
		8, 16,
		WSSCREEN_WSCOLORS | WSSCREEN_HILIT,
		NULL
	};
	sc->sc_screens[0] = &sc->sc_defaultscreen_descr;
	sc->sc_screenlist = (struct wsscreen_list){1, sc->sc_screens};

	isconsole = fb_is_console(node);

	sc->sc_mode = WSDISPLAYIO_MODE_EMUL;
	wsfont_init();

	vcons_init(&sc->vd, sc, &sc->sc_defaultscreen_descr, &mgx_accessops);
	sc->vd.init_screen = mgx_init_screen;

	vcons_init_screen(&sc->vd, &sc->sc_console_screen, 1, &defattr);
	sc->sc_console_screen.scr_flags |= VCONS_SCREEN_IS_STATIC;

	ri = &sc->sc_console_screen.scr_ri;

	sc->sc_defaultscreen_descr.nrows = ri->ri_rows;
	sc->sc_defaultscreen_descr.ncols = ri->ri_cols;
	sc->sc_defaultscreen_descr.textops = &ri->ri_ops;
	sc->sc_defaultscreen_descr.capabilities = ri->ri_caps;

	sc->sc_gc.gc_bitblt = mgx_bitblt;
	sc->sc_gc.gc_rectfill = mgx_rectfill;
	sc->sc_gc.gc_blitcookie = sc;
	sc->sc_gc.gc_rop = ROP_SRC;

	glyphcache_init(&sc->sc_gc,
	    sc->sc_height + 5,
	    (0x400000 / sc->sc_stride) - sc->sc_height - 5,
	    sc->sc_width,
	    ri->ri_font->fontwidth,
	    ri->ri_font->fontheight,
	    defattr);

	mgx_init_palette(sc);

	if(isconsole) {
		wsdisplay_cnattach(&sc->sc_defaultscreen_descr, ri, 0, 0,
		    defattr);
		vcons_replay_msgbuf(&sc->sc_console_screen);
	}

	aa.console = isconsole;
	aa.scrdata = &sc->sc_screenlist;
	aa.accessops = &mgx_accessops;
	aa.accesscookie = &sc->vd;

	config_found(self, &aa, wsemuldisplaydevprint);
}

static inline void
mgx_write_vga(struct mgx_softc *sc, uint32_t reg, uint8_t val)
{
	bus_space_write_1(sc->sc_tag, sc->sc_vgah, reg ^ 3, val);
}

static inline void
mgx_write_1(struct mgx_softc *sc, uint32_t reg, uint8_t val)
{
	bus_space_write_1(sc->sc_tag, sc->sc_blith, reg ^ 3, val);
}

static inline uint8_t
mgx_read_1(struct mgx_softc *sc, uint32_t reg)
{
	return bus_space_read_1(sc->sc_tag, sc->sc_blith, reg ^ 3);
}

static inline void
mgx_write_4(struct mgx_softc *sc, uint32_t reg, uint32_t val)
{
	bus_space_write_4(sc->sc_tag, sc->sc_blith, reg, val);
}


static void
mgx_write_dac(struct mgx_softc *sc, int idx, int r, int g, int b)
{
	mgx_write_vga(sc, VGA_BASE + VGA_DAC_ADDRW, idx);
	mgx_write_vga(sc, VGA_BASE + VGA_DAC_PALETTE, r);
	mgx_write_vga(sc, VGA_BASE + VGA_DAC_PALETTE, g);
	mgx_write_vga(sc, VGA_BASE + VGA_DAC_PALETTE, b);
}

static void
mgx_init_palette(struct mgx_softc *sc)
{
	struct rasops_info *ri = &sc->sc_console_screen.scr_ri;
	int i, j = 0;
	uint8_t cmap[768];

	rasops_get_cmap(ri, cmap, sizeof(cmap));
	for (i = 0; i < 256; i++) {
		sc->sc_cmap_red[i] = cmap[j];
		sc->sc_cmap_green[i] = cmap[j + 1];
		sc->sc_cmap_blue[i] = cmap[j + 2];
		mgx_write_dac(sc, i, cmap[j], cmap[j + 1], cmap[j + 2]);
		j += 3;
	}
}

static int
mgx_putcmap(struct mgx_softc *sc, struct wsdisplay_cmap *cm)
{
	u_char *r, *g, *b;
	u_int index = cm->index;
	u_int count = cm->count;
	int i, error;
	u_char rbuf[256], gbuf[256], bbuf[256];

	if (cm->index >= 256 || cm->count > 256 ||
	    (cm->index + cm->count) > 256)
		return EINVAL;
	error = copyin(cm->red, &rbuf[index], count);
	if (error)
		return error;
	error = copyin(cm->green, &gbuf[index], count);
	if (error)
		return error;
	error = copyin(cm->blue, &bbuf[index], count);
	if (error)
		return error;

	memcpy(&sc->sc_cmap_red[index], &rbuf[index], count);
	memcpy(&sc->sc_cmap_green[index], &gbuf[index], count);
	memcpy(&sc->sc_cmap_blue[index], &bbuf[index], count);

	r = &sc->sc_cmap_red[index];
	g = &sc->sc_cmap_green[index];
	b = &sc->sc_cmap_blue[index];
	
	for (i = 0; i < count; i++) {
		mgx_write_dac(sc, index, *r, *g, *b);
		index++;
		r++, g++, b++;
	}
	return 0;
}

static int
mgx_getcmap(struct mgx_softc *sc, struct wsdisplay_cmap *cm)
{
	u_int index = cm->index;
	u_int count = cm->count;
	int error;

	if (index >= 255 || count > 256 || index + count > 256)
		return EINVAL;
	
	error = copyout(&sc->sc_cmap_red[index],   cm->red,   count);
	if (error)
		return error;
	error = copyout(&sc->sc_cmap_green[index], cm->green, count);
	if (error)
		return error;
	error = copyout(&sc->sc_cmap_blue[index],  cm->blue,  count);
	if (error)
		return error;

	return 0;
}

static int
mgx_wait_engine(struct mgx_softc *sc)
{
	unsigned int i;
	uint8_t stat;

	for (i = 100000; i != 0; i--) {
		stat = mgx_read_1(sc, ATR_BLT_STATUS);
		if ((stat & (BLT_HOST_BUSY | BLT_ENGINE_BUSY)) == 0)
			break;
	}

	return i;
}

static int
mgx_wait_fifo(struct mgx_softc *sc, unsigned int nfifo)
{
	unsigned int i;
	uint8_t stat;

	for (i = 100000; i != 0; i--) {
		stat = mgx_read_1(sc, ATR_FIFO_STATUS);
		stat = (stat & FIFO_MASK) >> FIFO_SHIFT;
		if (stat >= nfifo)
			break;
		mgx_write_1(sc, ATR_FIFO_STATUS, 0);
	}

	return i;
}

static void
mgx_setup(struct mgx_softc *sc, int depth)
{
	/* wait for everything to go idle */
	if (mgx_wait_engine(sc) == 0)
		return;
	if (mgx_wait_fifo(sc, FIFO_AT24) == 0)
		return;
	/*
	 * Compute the invariant bits of the DEC register.
	 */

	switch (depth) {
		case 8:
			sc->sc_dec = DEC_DEPTH_8 << DEC_DEPTH_SHIFT;
			break;
		case 15:
		case 16:
			sc->sc_dec = DEC_DEPTH_16 << DEC_DEPTH_SHIFT;
			break;
		case 32:
			sc->sc_dec = DEC_DEPTH_32 << DEC_DEPTH_SHIFT;
			break;
		default:
			return; /* not supported */
	}

	switch (sc->sc_stride) {
		case 640:
			sc->sc_dec |= DEC_WIDTH_640 << DEC_WIDTH_SHIFT;
			break;
		case 800:
			sc->sc_dec |= DEC_WIDTH_800 << DEC_WIDTH_SHIFT;
			break;
		case 1024:
			sc->sc_dec |= DEC_WIDTH_1024 << DEC_WIDTH_SHIFT;
			break;
		case 1152:
			sc->sc_dec |= DEC_WIDTH_1152 << DEC_WIDTH_SHIFT;
			break;
		case 1280:
			sc->sc_dec |= DEC_WIDTH_1280 << DEC_WIDTH_SHIFT;
			break;
		case 1600:
			sc->sc_dec |= DEC_WIDTH_1600 << DEC_WIDTH_SHIFT;
			break;
		default:
			return; /* not supported */
	}
	mgx_write_1(sc, ATR_CLIP_CONTROL, 0);
	mgx_write_1(sc, ATR_BYTEMASK, 0xff);
}

static void
mgx_bitblt(void *cookie, int xs, int ys, int xd, int yd, int wi, int he,
             int rop)
{
	struct mgx_softc *sc = cookie;
	uint32_t dec = sc->sc_dec;

        dec |= (DEC_COMMAND_BLT << DEC_COMMAND_SHIFT) |
	       (DEC_START_DIMX << DEC_START_SHIFT);
	if (xs < xd) {
		xs += wi - 1;
		xd += wi - 1;
		dec |= DEC_DIR_X_REVERSE;
	}
	if (ys < yd) {
		ys += he - 1;
		yd += he - 1;
		dec |= DEC_DIR_Y_REVERSE;
	}
	mgx_wait_fifo(sc, 5);
	mgx_write_1(sc, ATR_ROP, rop);
	mgx_write_4(sc, ATR_DEC, dec);
	mgx_write_4(sc, ATR_SRC_XY, (ys << 16) | xs);
	mgx_write_4(sc, ATR_DST_XY, (yd << 16) | xd);
	mgx_write_4(sc, ATR_WH, (he << 16) | wi);
}
	
static void
mgx_rectfill(void *cookie, int x, int y, int wi, int he, long fg)
{
	struct mgx_softc *sc = cookie;
	struct vcons_screen *scr = sc->vd.active;
	uint32_t dec = sc->sc_dec;
	uint32_t col;

	if (scr == NULL)
		return;
	col = scr->scr_ri.ri_devcmap[fg];

	dec = sc->sc_dec;
	dec |= (DEC_COMMAND_RECT << DEC_COMMAND_SHIFT) |
	       (DEC_START_DIMX << DEC_START_SHIFT);
	mgx_wait_fifo(sc, 5);
	mgx_write_1(sc, ATR_ROP, ROP_SRC);
	mgx_write_4(sc, ATR_FG, col);
	mgx_write_4(sc, ATR_DEC, dec);
	mgx_write_4(sc, ATR_DST_XY, (y << 16) | x);
	mgx_write_4(sc, ATR_WH, (he << 16) | wi);
}

static void
mgx_putchar(void *cookie, int row, int col, u_int c, long attr)
{
	struct rasops_info *ri = cookie;
	struct wsdisplay_font *font = PICK_FONT(ri, c);
	struct vcons_screen *scr = ri->ri_hw;
	struct mgx_softc *sc = scr->scr_cookie;
	uint32_t fg, bg;
	int x, y, wi, he, rv;

	wi = font->fontwidth;
	he = font->fontheight;

	bg = (attr >> 16) & 0xf;
	fg = (attr >> 24) & 0xf;

	x = ri->ri_xorigin + col * wi;
	y = ri->ri_yorigin + row * he;

	if (c == 0x20) {
		mgx_rectfill(sc, x, y, wi, he, bg);
		if (attr & 1)
			mgx_rectfill(sc, x, y + he - 2, wi, 1, fg);
		return;
	}
	rv = glyphcache_try(&sc->sc_gc, c, x, y, attr);
	if (rv != GC_OK) {
		volatile uint32_t junk;

		mgx_wait_engine(sc);
		sc->sc_putchar(cookie, row, col, c, attr & ~1);
		junk = *(uint32_t *)sc->sc_fbaddr;
		__USE(junk);
		if (rv == GC_ADD)
			glyphcache_add(&sc->sc_gc, c, x, y);
	}
	if (attr & 1)
		mgx_rectfill(sc, x, y + he - 2, wi, 1, fg);
}

static void
mgx_cursor(void *cookie, int on, int row, int col)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;
	struct mgx_softc *sc = scr->scr_cookie;
	int x, y, wi,he;

	wi = ri->ri_font->fontwidth;
	he = ri->ri_font->fontheight;

	if (ri->ri_flg & RI_CURSOR) {
		x = ri->ri_ccol * wi + ri->ri_xorigin;
		y = ri->ri_crow * he + ri->ri_yorigin;
		mgx_bitblt(sc, x, y, x, y, wi, he, ROP_INV);
		ri->ri_flg &= ~RI_CURSOR;
	}

	ri->ri_crow = row;
	ri->ri_ccol = col;

	if (on)
	{
		x = ri->ri_ccol * wi + ri->ri_xorigin;
		y = ri->ri_crow * he + ri->ri_yorigin;
		mgx_bitblt(sc, x, y, x, y, wi, he, ROP_INV);
		ri->ri_flg |= RI_CURSOR;
	}
}

static void
mgx_copycols(void *cookie, int row, int srccol, int dstcol, int ncols)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;
	struct mgx_softc *sc = scr->scr_cookie;
	int32_t xs, xd, y, width, height;

	xs = ri->ri_xorigin + ri->ri_font->fontwidth * srccol;
	xd = ri->ri_xorigin + ri->ri_font->fontwidth * dstcol;
	y = ri->ri_yorigin + ri->ri_font->fontheight * row;
	width = ri->ri_font->fontwidth * ncols;
	height = ri->ri_font->fontheight;
	mgx_bitblt(sc, xs, y, xd, y, width, height, ROP_SRC);
}

static void
mgx_erasecols(void *cookie, int row, int startcol, int ncols, long fillattr)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;
	struct mgx_softc *sc = scr->scr_cookie;
	int32_t x, y, width, height, bg;

	x = ri->ri_xorigin + ri->ri_font->fontwidth * startcol;
	y = ri->ri_yorigin + ri->ri_font->fontheight * row;
	width = ri->ri_font->fontwidth * ncols;
	height = ri->ri_font->fontheight;
	bg = (fillattr >> 16) & 0xff;
	mgx_rectfill(sc, x, y, width, height, bg);
}

static void
mgx_copyrows(void *cookie, int srcrow, int dstrow, int nrows)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;
	struct mgx_softc *sc = scr->scr_cookie;
	int32_t x, ys, yd, width, height;

	x = ri->ri_xorigin;
	ys = ri->ri_yorigin + ri->ri_font->fontheight * srcrow;
	yd = ri->ri_yorigin + ri->ri_font->fontheight * dstrow;
	width = ri->ri_emuwidth;
	height = ri->ri_font->fontheight * nrows;
	mgx_bitblt(sc, x, ys, x, yd, width, height, ROP_SRC);
}

static void
mgx_eraserows(void *cookie, int row, int nrows, long fillattr)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;
	struct mgx_softc *sc = scr->scr_cookie;
	int32_t x, y, width, height, bg;

	if ((row == 0) && (nrows == ri->ri_rows)) {
		x = y = 0;
		width = ri->ri_width;
		height = ri->ri_height;
	} else {
		x = ri->ri_xorigin;
		y = ri->ri_yorigin + ri->ri_font->fontheight * row;
		width = ri->ri_emuwidth;
		height = ri->ri_font->fontheight * nrows;
	}
	bg = (fillattr >> 16) & 0xff;
	mgx_rectfill(sc, x, y, width, height, bg);
}

static void
mgx_init_screen(void *cookie, struct vcons_screen *scr,
    int existing, long *defattr)
{
	struct mgx_softc *sc = cookie;
	struct rasops_info *ri = &scr->scr_ri;

	ri->ri_depth = 8;
	ri->ri_width = sc->sc_width;
	ri->ri_height = sc->sc_height;
	ri->ri_stride = sc->sc_stride;
	ri->ri_flg = RI_CENTER | RI_ENABLE_ALPHA;

	if (ri->ri_depth == 8)
		ri->ri_flg |= RI_8BIT_IS_RGB;

	ri->ri_bits = sc->sc_fbaddr;

	rasops_init(ri, 0, 0);
	sc->sc_putchar = ri->ri_ops.putchar;

	ri->ri_caps = WSSCREEN_REVERSE | WSSCREEN_WSCOLORS;

	rasops_reconfig(ri, ri->ri_height / ri->ri_font->fontheight,
		    ri->ri_width / ri->ri_font->fontwidth);

	ri->ri_hw = scr;
	ri->ri_ops.putchar   = mgx_putchar;
	ri->ri_ops.cursor    = mgx_cursor;
	ri->ri_ops.copyrows  = mgx_copyrows;
	ri->ri_ops.eraserows = mgx_eraserows;
	ri->ri_ops.copycols  = mgx_copycols;
	ri->ri_ops.erasecols = mgx_erasecols;
}

static int
mgx_ioctl(void *v, void *vs, u_long cmd, void *data, int flag,
    struct lwp *l)
{
	struct vcons_data *vd = v;
	struct mgx_softc *sc = vd->cookie;
	struct wsdisplay_fbinfo *wdf;
	struct vcons_screen *ms = vd->active;

	switch (cmd) {
		case WSDISPLAYIO_GTYPE:
			*(u_int *)data = WSDISPLAY_TYPE_MGX;
			return 0;

		case WSDISPLAYIO_GINFO:
			wdf = (void *)data;
			wdf->height = sc->sc_height;
			wdf->width = sc->sc_width;
			wdf->depth = 8;
			wdf->cmsize = 256;
			return 0;

		case FBIOGVIDEO:
		case WSDISPLAYIO_GVIDEO:
			*(int *)data = 1;
			return 0;

		case WSDISPLAYIO_SVIDEO:
		case FBIOSVIDEO:
			return 0;

		case WSDISPLAYIO_LINEBYTES:
			{
				int *ret = (int *)data;
				*ret = sc->sc_stride;
			}
			return 0;

		case WSDISPLAYIO_SMODE:
			{
				int new_mode = *(int*)data;
				if (new_mode != sc->sc_mode)
				{
					sc->sc_mode = new_mode;
					if (new_mode == WSDISPLAYIO_MODE_EMUL)
					{
						mgx_setup(sc, 8);
						glyphcache_wipe(&sc->sc_gc);
						mgx_init_palette(sc);
						vcons_redraw_screen(ms);
					} else {
						mgx_setup(sc, 32);
					}
				}
			}
			return 0;

		case WSDISPLAYIO_GETCMAP:
			return mgx_getcmap(sc, (struct wsdisplay_cmap *)data);

		case WSDISPLAYIO_PUTCMAP:
			return mgx_putcmap(sc, (struct wsdisplay_cmap *)data);

		case WSDISPLAYIO_GET_FBINFO:
			{
				struct wsdisplayio_fbinfo *fbi = data;
				int ret;

				ret = wsdisplayio_get_fbinfo(&ms->scr_ri, fbi);
				fbi->fbi_fbsize = 0x400000;
				return ret;
			}
	}
	return EPASSTHROUGH;
}

static paddr_t
mgx_mmap(void *v, void *vs, off_t offset, int prot)
{
	struct vcons_data *vd = v;
	struct mgx_softc *sc = vd->cookie;

	/* regular fb mapping at 0 */
	if ((offset >= 0) && (offset < 0x400000)) {
		return bus_space_mmap(sc->sc_tag, sc->sc_paddr,
		    offset, prot, BUS_SPACE_MAP_LINEAR);
	}

	return -1;
}
