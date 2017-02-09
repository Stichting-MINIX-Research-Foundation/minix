/*	$NetBSD: ct65550.c,v 1.10 2014/01/04 16:37:05 macallan Exp $	*/

/*
 * Copyright (c) 2006 Michael Lorenz
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * A console driver for Chips & Technologies 65550 graphics controllers
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ct65550.c,v 1.10 2014/01/04 16:37:05 macallan Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/kauth.h>
#include <sys/bus.h>
#include <dev/videomode/videomode.h>

#include <dev/ic/ct65550reg.h>
#include <dev/ic/ct65550var.h>

#include "opt_wsemul.h"
#include "opt_chipsfb.h"

static struct vcons_screen chipsfb_console_screen;

extern const u_char rasops_cmap[768];

static void 	chipsfb_init(struct chipsfb_softc *);

static void	chipsfb_cursor(void *, int, int, int);
static void	chipsfb_copycols(void *, int, int, int, int);
static void	chipsfb_erasecols(void *, int, int, int, long);
static void	chipsfb_copyrows(void *, int, int, int);
static void	chipsfb_eraserows(void *, int, int, long);

#if 0
static int	chipsfb_allocattr(void *, int, int, int, long *);
static void	chipsfb_scroll(void *, void *, int);
static int	chipsfb_load_font(void *, void *, struct wsdisplay_font *);
#endif

static int	chipsfb_putcmap(struct chipsfb_softc *,
			    struct wsdisplay_cmap *);
static int 	chipsfb_getcmap(struct chipsfb_softc *,
			    struct wsdisplay_cmap *);
static int 	chipsfb_putpalreg(struct chipsfb_softc *, uint8_t, uint8_t,
			    uint8_t, uint8_t);

static void	chipsfb_bitblt(struct chipsfb_softc *, int, int, int, int,
			    int, int, uint8_t);
static void	chipsfb_rectfill(struct chipsfb_softc *, int, int, int, int,
			    int);
static void	chipsfb_putchar(void *, int, int, u_int, long);
static void	chipsfb_setup_mono(struct chipsfb_softc *, int, int, int,
			    int, uint32_t, uint32_t);
static void	chipsfb_feed(struct chipsfb_softc *, int, uint8_t *);

#if 0
static void	chipsfb_showpal(struct chipsfb_softc *);
#endif
static void	chipsfb_restore_palette(struct chipsfb_softc *);

static void	chipsfb_wait_idle(struct chipsfb_softc *);

struct wsscreen_descr chipsfb_defaultscreen = {
	"default",	/* name */
	0, 0,		/* ncols, nrows */
	NULL,		/* textops */
	8, 16,		/* fontwidth, fontheight */
	WSSCREEN_WSCOLORS | WSSCREEN_HILIT, /* capabilities */
	NULL,		/* modecookie */
};

const struct wsscreen_descr *_chipsfb_scrlist[] = {
	&chipsfb_defaultscreen,
	/* XXX other formats, graphics screen? */
};

struct wsscreen_list chipsfb_screenlist = {
	sizeof(_chipsfb_scrlist) / sizeof(struct wsscreen_descr *), _chipsfb_scrlist
};

static int	chipsfb_ioctl(void *, void *, u_long, void *, int,
		    struct lwp *);
static paddr_t	chipsfb_mmap(void *, void *, off_t, int);
static void	chipsfb_clearscreen(struct chipsfb_softc *);
static void	chipsfb_init_screen(void *, struct vcons_screen *, int,
			    long *);


struct wsdisplay_accessops chipsfb_accessops = {
	chipsfb_ioctl,
	chipsfb_mmap,
	NULL,	/* vcons_alloc_screen */
	NULL,	/* vcons_free_screen */
	NULL,	/* vcons_show_screen */
	NULL,	/* load_font */
	NULL,	/* polls */
	NULL,	/* scroll */
};

/*
 * Inline functions for getting access to register aperture.
 */
static inline void
chipsfb_write32(struct chipsfb_softc *sc, uint32_t reg, uint32_t val)
{
	bus_space_write_4(sc->sc_memt, sc->sc_mmregh, reg, val);
}

static inline uint32_t
chipsfb_read32(struct chipsfb_softc *sc, uint32_t reg)
{
	return bus_space_read_4(sc->sc_memt, sc->sc_mmregh, reg);
}

static inline void
chipsfb_write_vga(struct chipsfb_softc *sc, uint32_t reg,  uint8_t val)
{
	bus_space_write_1(sc->sc_iot, sc->sc_ioregh, reg, val);
}

static inline uint8_t
chipsfb_read_vga(struct chipsfb_softc *sc, uint32_t reg)
{
	return bus_space_read_1(sc->sc_iot, sc->sc_ioregh, reg);
}

static inline uint8_t
chipsfb_read_indexed(struct chipsfb_softc *sc, uint32_t reg, uint8_t index)
{

	chipsfb_write_vga(sc, reg & 0xfffe, index);
	return chipsfb_read_vga(sc, reg | 0x0001);
}

__unused static inline void
chipsfb_write_indexed(struct chipsfb_softc *sc, uint32_t reg, uint8_t index,
    uint8_t val)
{

	chipsfb_write_vga(sc, reg & 0xfffe, index);
	chipsfb_write_vga(sc, reg | 0x0001, val);
}

static void
chipsfb_wait_idle(struct chipsfb_softc *sc)
{

#ifdef CHIPSFB_DEBUG
	chipsfb_write32(sc, CT_OFF_FB + (800 * 598) - 4, 0);
#endif

	/* spin until the blitter is idle */
	while ((chipsfb_read32(sc, CT_BLT_CONTROL) & BLT_IS_BUSY) != 0) {
	}

#ifdef CHIPSFB_DEBUG
	chipsfb_write32(sc, CT_OFF_FB + (800 * 598) - 4, 0xffffffff);
#endif
}

void
chipsfb_do_attach(struct chipsfb_softc *sc)
{
	struct wsemuldisplaydev_attach_args aa;
	struct rasops_info *ri;
	prop_dictionary_t dict;
	ulong defattr;
	bool console = false;
	int width, height, i, j;
	uint32_t bg, fg, ul;
	uint8_t cmap[768];

	dict = device_properties(sc->sc_dev);
	sc->sc_mode = WSDISPLAYIO_MODE_EMUL;
	sc->sc_dacw = -1;

#ifdef CHIPSFB_DEBUG
	printf(prop_dictionary_externalize(dict));
#endif
	chipsfb_init(sc);

	width = height = -1;

	/* detect panel size */
	width = chipsfb_read_indexed(sc, CT_FP_INDEX, FP_HSIZE_LSB);
	width |= (chipsfb_read_indexed(sc, CT_FP_INDEX, FP_HORZ_OVERFLOW_1)
	    & 0x0f) << 8;
	width = (width + 1) * 8;
	height = chipsfb_read_indexed(sc, CT_FP_INDEX, FP_VSIZE_LSB);
	height |= (chipsfb_read_indexed(sc, CT_FP_INDEX, FP_VERT_OVERFLOW_1)
	    & 0x0f) << 8;
	height++;
	if ((width < 640) || ( width > 1280) || (height < 480) ||
	    (height > 1024)) {
		/* no sane values in the panel registers */
		width = height = -1;
	} else
		aprint_verbose("Panel size: %d x %d\n", width, height);

	if (!prop_dictionary_get_uint32(dict, "width", &sc->width))
		sc->width = width;
	if (!prop_dictionary_get_uint32(dict, "height", &sc->height))
		sc->height = height;
	if (!prop_dictionary_get_uint32(dict, "depth", &sc->bits_per_pixel))
		sc->bits_per_pixel = 8;
	if (!prop_dictionary_get_uint32(dict, "linebytes", &sc->linebytes))
		sc->linebytes = (sc->width * sc->bits_per_pixel) >> 3;

	prop_dictionary_get_bool(dict, "is_console", &console);

#ifdef notyet
	/* XXX this should at least be configurable via kernel config */
	chipsfb_set_videomode(sc, &videomode_list[16]);
#endif

	vcons_init(&sc->vd, sc, &chipsfb_defaultscreen, &chipsfb_accessops);
	sc->vd.init_screen = chipsfb_init_screen;

	ri = &chipsfb_console_screen.scr_ri;
	if (console) {
		vcons_init_screen(&sc->vd, &chipsfb_console_screen, 1,
		    &defattr);
		chipsfb_console_screen.scr_flags |= VCONS_SCREEN_IS_STATIC;

		chipsfb_defaultscreen.textops = &ri->ri_ops;
		chipsfb_defaultscreen.capabilities = ri->ri_caps;
		chipsfb_defaultscreen.nrows = ri->ri_rows;
		chipsfb_defaultscreen.ncols = ri->ri_cols;
		wsdisplay_cnattach(&chipsfb_defaultscreen, ri, 0, 0, defattr);
	} else {
		if (chipsfb_console_screen.scr_ri.ri_rows == 0) {
			/* do some minimal setup to avoid weirdnesses later */
			vcons_init_screen(&sc->vd, &chipsfb_console_screen, 1,
			    &defattr);
		} else
			(*ri->ri_ops.allocattr)(ri, 0, 0, 0, &defattr);

	}

	rasops_unpack_attr(defattr, &fg, &bg, &ul);
	sc->sc_bg = ri->ri_devcmap[bg];
	chipsfb_clearscreen(sc);

	if (console)
		vcons_replay_msgbuf(&chipsfb_console_screen);

	aprint_normal_dev(sc->sc_dev, "%d MB aperture, %d MB VRAM at 0x%08x\n",
	    (u_int)(sc->sc_fbsize >> 20),
	    (int)sc->memsize >> 20, (u_int)sc->sc_fb);
#ifdef CHIPSFB_DEBUG
	aprint_debug("fb: %08lx\n", (ulong)ri->ri_bits);
#endif

	j = 0;
	rasops_get_cmap(ri, cmap, sizeof(cmap));
	for (i = 0; i < 256; i++) {
		chipsfb_putpalreg(sc, i, cmap[j], cmap[j + 1], cmap[j + 2]);
		j += 3;
	}
	
	aa.console = console;
	aa.scrdata = &chipsfb_screenlist;
	aa.accessops = &chipsfb_accessops;
	aa.accesscookie = &sc->vd;

	config_found(sc->sc_dev, &aa, wsemuldisplaydevprint);
}

static int
chipsfb_putpalreg(struct chipsfb_softc *sc, uint8_t index, uint8_t r,
    uint8_t g, uint8_t b)
{

	sc->sc_cmap_red[index] = r;
	sc->sc_cmap_green[index] = g;
	sc->sc_cmap_blue[index] = b;

	chipsfb_write_vga(sc, CT_DACMASK, 0xff);
	chipsfb_write_vga(sc, CT_WRITEINDEX, index);
	chipsfb_write_vga(sc, CT_DACDATA, r);
	chipsfb_write_vga(sc, CT_DACDATA, g);
	chipsfb_write_vga(sc, CT_DACDATA, b);

	return 0;
}

static int
chipsfb_putcmap(struct chipsfb_softc *sc, struct wsdisplay_cmap *cm)
{
	u_char *r, *g, *b;
	u_int index = cm->index;
	u_int count = cm->count;
	int i, error;
	u_char rbuf[256], gbuf[256], bbuf[256];

#ifdef CHIPSFB_DEBUG
	aprint_debug("putcmap: %d %d\n",index, count);
#endif
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
		chipsfb_putpalreg(sc, index, *r, *g, *b);
		index++;
		r++, g++, b++;
	}
	return 0;
}

static int
chipsfb_getcmap(struct chipsfb_softc *sc, struct wsdisplay_cmap *cm)
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

static void
chipsfb_clearscreen(struct chipsfb_softc *sc)
{
	chipsfb_rectfill(sc, 0, 0, sc->width, sc->height, sc->sc_bg);
}

/*
 * wsdisplay_emulops
 */

static void
chipsfb_cursor(void *cookie, int on, int row, int col)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;
	struct chipsfb_softc *sc = scr->scr_cookie;
	int x, y, wi, he;

	wi = ri->ri_font->fontwidth;
	he = ri->ri_font->fontheight;

	if (sc->sc_mode == WSDISPLAYIO_MODE_EMUL) {
		x = ri->ri_ccol * wi + ri->ri_xorigin;
		y = ri->ri_crow * he + ri->ri_yorigin;
		if (ri->ri_flg & RI_CURSOR) {
			chipsfb_bitblt(sc, x, y, x, y, wi, he, ROP_NOT_DST);
			ri->ri_flg &= ~RI_CURSOR;
		}
		ri->ri_crow = row;
		ri->ri_ccol = col;
		if (on) {
			x = ri->ri_ccol * wi + ri->ri_xorigin;
			y = ri->ri_crow * he + ri->ri_yorigin;
			chipsfb_bitblt(sc, x, y, x, y, wi, he, ROP_NOT_DST);
			ri->ri_flg |= RI_CURSOR;
		}
	} else {
		ri->ri_flg &= ~RI_CURSOR;
		ri->ri_crow = row;
		ri->ri_ccol = col;
	}
}

#if 0
int
chipsfb_mapchar(void *cookie, int uni, u_int *index)
{
	return 0;
}
#endif

static void
chipsfb_copycols(void *cookie, int row, int srccol, int dstcol, int ncols)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;
	struct chipsfb_softc *sc = scr->scr_cookie;
	int32_t xs, xd, y, width, height;

	if (sc->sc_mode == WSDISPLAYIO_MODE_EMUL) {
		xs = ri->ri_xorigin + ri->ri_font->fontwidth * srccol;
		xd = ri->ri_xorigin + ri->ri_font->fontwidth * dstcol;
		y = ri->ri_yorigin + ri->ri_font->fontheight * row;
		width = ri->ri_font->fontwidth * ncols;
		height = ri->ri_font->fontheight;
		chipsfb_bitblt(sc, xs, y, xd, y, width, height, ROP_COPY);
	}
}

static void
chipsfb_erasecols(void *cookie, int row, int startcol, int ncols,
    long fillattr)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;
	struct chipsfb_softc *sc = scr->scr_cookie;
	int32_t x, y, width, height, fg, bg, ul;

	if (sc->sc_mode == WSDISPLAYIO_MODE_EMUL) {
		x = ri->ri_xorigin + ri->ri_font->fontwidth * startcol;
		y = ri->ri_yorigin + ri->ri_font->fontheight * row;
		width = ri->ri_font->fontwidth * ncols;
		height = ri->ri_font->fontheight;
		rasops_unpack_attr(fillattr, &fg, &bg, &ul);

		chipsfb_rectfill(sc, x, y, width, height, ri->ri_devcmap[bg]);
	}
}

static void
chipsfb_copyrows(void *cookie, int srcrow, int dstrow, int nrows)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;
	struct chipsfb_softc *sc = scr->scr_cookie;
	int32_t x, ys, yd, width, height;

	if (sc->sc_mode == WSDISPLAYIO_MODE_EMUL) {
		x = ri->ri_xorigin;
		ys = ri->ri_yorigin + ri->ri_font->fontheight * srcrow;
		yd = ri->ri_yorigin + ri->ri_font->fontheight * dstrow;
		width = ri->ri_emuwidth;
		height = ri->ri_font->fontheight * nrows;
		chipsfb_bitblt(sc, x, ys, x, yd, width, height, ROP_COPY);
	}
}

static void
chipsfb_eraserows(void *cookie, int row, int nrows, long fillattr)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;
	struct chipsfb_softc *sc = scr->scr_cookie;
	int32_t x, y, width, height, fg, bg, ul;

	if (sc->sc_mode == WSDISPLAYIO_MODE_EMUL) {
		rasops_unpack_attr(fillattr, &fg, &bg, &ul);
		if ((row == 0) && (nrows == ri->ri_rows)) {
			/* clear the whole screen */
			chipsfb_rectfill(sc, 0, 0, ri->ri_width,
			    ri->ri_height, ri->ri_devcmap[bg]);
		} else {
			x = ri->ri_xorigin;
			y = ri->ri_yorigin + ri->ri_font->fontheight * row;
			width = ri->ri_emuwidth;
			height = ri->ri_font->fontheight * nrows;
			chipsfb_rectfill(sc, x, y, width, height,
			    ri->ri_devcmap[bg]);
		}
	}
}

static void
chipsfb_bitblt(struct chipsfb_softc *sc, int xs, int ys, int xd, int yd,
    int width, int height, uint8_t rop)
{
	uint32_t src, dst, cmd = rop, stride, size;

	cmd |= BLT_PAT_IS_SOLID;

	/* we assume 8 bit for now */
	src = xs + ys * sc->linebytes;
	dst = xd + yd * sc->linebytes;

	if (xs < xd) {
		/* right-to-left operation */
		cmd |= BLT_START_RIGHT;
		src += width - 1;
		dst += width - 1;
	}

	if (ys < yd) {
		/* bottom-to-top operation */
		cmd |= BLT_START_BOTTOM;
		src += (height - 1) * sc->linebytes;
		dst += (height - 1) * sc->linebytes;
	}

	stride = (sc->linebytes << 16) | sc->linebytes;
	size = (height << 16) | width;

	chipsfb_wait_idle(sc);
	chipsfb_write32(sc, CT_BLT_STRIDE, stride);
	chipsfb_write32(sc, CT_BLT_SRCADDR, src);
	chipsfb_write32(sc, CT_BLT_DSTADDR, dst);
	chipsfb_write32(sc, CT_BLT_CONTROL, cmd);
	chipsfb_write32(sc, CT_BLT_SIZE, size);
#ifdef CHIPSFB_WAIT
	chipsfb_wait_idle(sc);
#endif
}

static void
chipsfb_rectfill(struct chipsfb_softc *sc, int x, int y, int width,
    int height, int colour)
{
	uint32_t dst, cmd, stride, size;

	cmd = BLT_PAT_IS_SOLID | BLT_PAT_IS_MONO | ROP_PAT;

	/* we assume 8 bit for now */
	dst = x + y * sc->linebytes;

	stride = (sc->linebytes << 16) | sc->linebytes;
	size = (height << 16) | width;

	chipsfb_wait_idle(sc);
	chipsfb_write32(sc, CT_BLT_STRIDE, stride);
	chipsfb_write32(sc, CT_BLT_SRCADDR, dst);
	chipsfb_write32(sc, CT_BLT_DSTADDR, dst);
	chipsfb_write32(sc, CT_BLT_CONTROL, cmd);
	chipsfb_write32(sc, CT_BLT_BG, colour);
	chipsfb_write32(sc, CT_BLT_FG, colour);
	chipsfb_write32(sc, CT_BLT_SIZE, size);
#ifdef CHIPSFB_WAIT
	chipsfb_wait_idle(sc);
#endif
}

static void
chipsfb_putchar(void *cookie, int row, int col, u_int c, long attr)
{
	struct rasops_info *ri = cookie;
	struct wsdisplay_font *font = PICK_FONT(ri, c);
	struct vcons_screen *scr = ri->ri_hw;
	struct chipsfb_softc *sc = scr->scr_cookie;

	if (__predict_false((unsigned int)row > ri->ri_rows ||
	    (unsigned int)col > ri->ri_cols))
		return;

	if (sc->sc_mode == WSDISPLAYIO_MODE_EMUL) {
		uint8_t *data;
		int fg, bg, uc;
		int x, y, wi, he;

		wi = font->fontwidth;
		he = font->fontheight;

		if (!CHAR_IN_FONT(c, font))
			return;
		bg = (u_char)ri->ri_devcmap[(attr >> 16) & 0xf];
		fg = (u_char)ri->ri_devcmap[(attr >> 24) & 0xf];
		x = ri->ri_xorigin + col * wi;
		y = ri->ri_yorigin + row * he;
		if (c == 0x20) {
			chipsfb_rectfill(sc, x, y, wi, he, bg);
		} else {
			uc = c - font->firstchar;
			data = (uint8_t *)font->data + uc *
			    ri->ri_fontscale;
			chipsfb_setup_mono(sc, x, y, wi, he, fg, bg);
			chipsfb_feed(sc, font->stride * he, data);
		}
	}
}

static void
chipsfb_setup_mono(struct chipsfb_softc *sc, int xd, int yd, int width,
    int height, uint32_t fg, uint32_t bg)
{
	uint32_t dst, cmd, stride, size;

	cmd = BLT_PAT_IS_SOLID | BLT_SRC_IS_CPU | BLT_SRC_IS_MONO | ROP_COPY;

	/* we assume 8 bit for now */
	dst = xd + yd * sc->linebytes;

	stride = (sc->linebytes << 16);
	size = (height << 16) | width;

	chipsfb_wait_idle(sc);
	chipsfb_write32(sc, CT_BLT_STRIDE, stride);
	chipsfb_write32(sc, CT_BLT_EXPCTL, MONO_SRC_ALIGN_BYTE);
	chipsfb_write32(sc, CT_BLT_DSTADDR, dst);
	chipsfb_write32(sc, CT_BLT_SRCADDR, 0);
	chipsfb_write32(sc, CT_BLT_CONTROL, cmd);
	chipsfb_write32(sc, CT_BLT_BG, bg);
	chipsfb_write32(sc, CT_BLT_FG, fg);
	chipsfb_write32(sc, CT_BLT_SIZE, size);
}

static void
chipsfb_feed(struct chipsfb_softc *sc, int count, uint8_t *data)
{
	int i;
	uint32_t latch = 0, bork;
	int shift = 0;

	for (i = 0; i < count; i++) {
		bork = data[i];
		latch |= (bork << shift);
		if (shift == 24) {
			chipsfb_write32(sc, CT_OFF_DATA - CT_OFF_BITBLT, latch);
			latch = 0;
			shift = 0;
		} else
			shift += 8;
	}

	if (shift != 0) {
		chipsfb_write32(sc, CT_OFF_DATA - CT_OFF_BITBLT, latch);
	}

	/* apparently the chip wants 64bit-aligned data or it won't go idle */
	if ((count + 3) & 0x04) {
		chipsfb_write32(sc, CT_OFF_DATA - CT_OFF_BITBLT, 0);
	}
#ifdef CHIPSFB_WAIT
	chipsfb_wait_idle(sc);
#endif
}

#if 0
static void
chipsfb_showpal(struct chipsfb_softc *sc)
{
	int i, x = 0;

	for (i = 0; i < 16; i++) {
		chipsfb_rectfill(sc, x, 0, 64, 64, i);
		x += 64;
	}
}
#endif

#if 0
static int
chipsfb_allocattr(void *cookie, int fg, int bg, int flags, long *attrp)
{

	return 0;
}
#endif

static void
chipsfb_restore_palette(struct chipsfb_softc *sc)
{
	int i;

	for (i = 0; i < 256; i++) {
		chipsfb_putpalreg(sc,
		   i,
		   sc->sc_cmap_red[i],
		   sc->sc_cmap_green[i],
		   sc->sc_cmap_blue[i]);
	}
}

/*
 * wsdisplay_accessops
 */

static int
chipsfb_ioctl(void *v, void *vs, u_long cmd, void *data, int flag,
	struct lwp *l)
{
	struct vcons_data *vd = v;
	struct chipsfb_softc *sc = vd->cookie;
	struct wsdisplay_fbinfo *wdf;
	struct vcons_screen *ms = vd->active;

	switch (cmd) {
	case WSDISPLAYIO_GTYPE:
		*(u_int *)data = WSDISPLAY_TYPE_PCIMISC;
		return 0;

	case WSDISPLAYIO_GINFO:
		wdf = (void *)data;
		wdf->height = ms->scr_ri.ri_height;
		wdf->width = ms->scr_ri.ri_width;
		wdf->depth = ms->scr_ri.ri_depth;
		wdf->cmsize = 256;
		return 0;

	case WSDISPLAYIO_GETCMAP:
		return chipsfb_getcmap(sc,
		    (struct wsdisplay_cmap *)data);

	case WSDISPLAYIO_PUTCMAP:
		return chipsfb_putcmap(sc,
		    (struct wsdisplay_cmap *)data);


	case WSDISPLAYIO_SMODE: {
		int new_mode = *(int*)data;
		if (new_mode != sc->sc_mode) {
			sc->sc_mode = new_mode;
			if(new_mode == WSDISPLAYIO_MODE_EMUL) {
				chipsfb_restore_palette(sc);
				vcons_redraw_screen(ms);
			}
		}
		}
		return 0;
	
	case WSDISPLAYIO_GET_FBINFO: {
		struct wsdisplayio_fbinfo *fbi = data;
		return wsdisplayio_get_fbinfo(&ms->scr_ri, fbi);
	}

	default:
		if (sc->sc_ioctl != NULL)
			return sc->sc_ioctl(v, vs, cmd, data, flag, l);
	}
	return EPASSTHROUGH;
}

static paddr_t
chipsfb_mmap(void *v, void *vs, off_t offset, int prot)
{
	struct vcons_data *vd = v;
	struct chipsfb_softc *sc = vd->cookie;
	paddr_t pa;

	if (sc->sc_mmap != NULL)
		return sc->sc_mmap(v, vs, offset, prot);

	/* 'regular' framebuffer mmap()ing */
	if (offset < sc->memsize) {
		pa = bus_space_mmap(sc->sc_memt, offset, 0, prot,
		    BUS_SPACE_MAP_LINEAR | BUS_SPACE_MAP_PREFETCHABLE);
		return pa;
	}

	/*
	 * restrict all other mappings to processes with superuser privileges
	 * or the kernel itself
	 */
	if (kauth_authorize_machdep(kauth_cred_get(), KAUTH_MACHDEP_UNMANAGEDMEM,
	    NULL, NULL, NULL, NULL) != 0) {
		aprint_normal_dev(sc->sc_dev, "mmap() rejected.\n");
		return -1;
	}

	if ((offset >= sc->sc_fb) && (offset < (sc->sc_fb + sc->sc_fbsize))) {
		pa = bus_space_mmap(sc->sc_memt, offset, 0, prot,
		    BUS_SPACE_MAP_LINEAR);
		return pa;
	}

#ifdef PCI_MAGIC_IO_RANGE
	/* allow mapping of IO space */
	if ((offset >= PCI_MAGIC_IO_RANGE) &&
	    (offset < PCI_MAGIC_IO_RANGE + 0x10000)) {
		pa = bus_space_mmap(sc->sc_iot, offset - PCI_MAGIC_IO_RANGE,
		    0, prot, BUS_SPACE_MAP_LINEAR);
		return pa;
	}
#endif

	return -1;
}

static void
chipsfb_init_screen(void *cookie, struct vcons_screen *scr,
    int existing, long *defattr)
{
	struct chipsfb_softc *sc = cookie;
	struct rasops_info *ri = &scr->scr_ri;

	ri->ri_depth = sc->bits_per_pixel;
	ri->ri_width = sc->width;
	ri->ri_height = sc->height;
	ri->ri_stride = sc->width;
	ri->ri_flg = RI_CENTER | RI_FULLCLEAR | RI_8BIT_IS_RGB;

	ri->ri_bits = bus_space_vaddr(sc->sc_memt, sc->sc_fbh);

#ifdef CHIPSFB_DEBUG
	aprint_debug("addr: %08lx\n", (ulong)ri->ri_bits);
#endif
	if (existing) {
		ri->ri_flg |= RI_CLEAR;
	}

	rasops_init(ri, 0, 0);
	ri->ri_caps = WSSCREEN_WSCOLORS;

	rasops_reconfig(ri, sc->height / ri->ri_font->fontheight,
		    sc->width / ri->ri_font->fontwidth);

	ri->ri_hw = scr;
	ri->ri_ops.copyrows = chipsfb_copyrows;
	ri->ri_ops.copycols = chipsfb_copycols;
	ri->ri_ops.eraserows = chipsfb_eraserows;
	ri->ri_ops.erasecols = chipsfb_erasecols;
	ri->ri_ops.cursor = chipsfb_cursor;
	ri->ri_ops.putchar = chipsfb_putchar;
}

#if 0
int
chipsfb_load_font(void *v, void *cookie, struct wsdisplay_font *data)
{

	return 0;
}
#endif

static void
chipsfb_init(struct chipsfb_softc *sc)
{

	chipsfb_wait_idle(sc);

	/* setup the blitter */
}

uint32_t
chipsfb_probe_vram(struct chipsfb_softc *sc)
{
	uint32_t ofs = 0x00080000;	/* 512kB */

	/*
	 * advance in 0.5MB steps, see if we can read back what we wrote and
	 * if what we wrote to 0 is left untouched. Max. fb size is 4MB so
	 * we voluntarily stop there.
	 */
	bus_space_write_4(sc->sc_memt, sc->sc_fbh, 0, 0xf0f0f0f0);
	bus_space_write_4(sc->sc_memt, sc->sc_fbh, ofs, 0x0f0f0f0f);
	while ((bus_space_read_4(sc->sc_memt, sc->sc_fbh, 0) == 0xf0f0f0f0) &&
	    (bus_space_read_4(sc->sc_memt, sc->sc_fbh, ofs) == 0x0f0f0f0f) &&
	    (ofs < 0x00400000)) {

		ofs += 0x00080000;
		bus_space_write_4(sc->sc_memt, sc->sc_fbh, ofs, 0x0f0f0f0f);
	}

	return ofs;
}
