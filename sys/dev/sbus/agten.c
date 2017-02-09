/*	$NetBSD: agten.c,v 1.32 2013/10/19 21:00:32 mrg Exp $ */

/*-
 * Copyright (c) 2007 Michael Lorenz
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: agten.c,v 1.32 2013/10/19 21:00:32 mrg Exp $");

/*
 * a driver for the Fujitsu AG-10e SBus framebuffer
 *
 * this thing is Frankenstein's Monster among graphics boards.
 * it contains three graphics chips:
 * a GLint 300SX - 24bit stuff, double-buffered
 * an Imagine 128 which provides an 8bit overlay
 * a Weitek P9100 which provides WIDs
 * so here we need to mess only with the P9100 and the I128 - for X we just
 * hide the overlay and let the Xserver mess with the GLint
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/mutex.h>
#include <sys/ioctl.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/conf.h>

#include <dev/sun/fbio.h>
#include <dev/sun/fbvar.h>
#include <dev/sun/btreg.h>
#include <dev/sun/btvar.h>

#include <sys/bus.h>
#include <machine/autoconf.h>

#include <dev/sbus/sbusvar.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>
#include <dev/rasops/rasops.h>
#include <dev/wsfont/wsfont.h>

#include <dev/wscons/wsdisplay_vconsvar.h>
#include <dev/wscons/wsdisplay_glyphcachevar.h>

#include <dev/sbus/p9100reg.h>
#include <dev/ic/ibm561reg.h>
#include <dev/ic/i128reg.h>
#include <dev/ic/i128var.h>

#include "opt_agten.h"
#include "ioconf.h"

static int	agten_match(device_t, cfdata_t, void *);
static void	agten_attach(device_t, device_t, void *);

static int	agten_ioctl(void *, void *, u_long, void *, int, struct lwp *);
static paddr_t	agten_mmap(void *, void *, off_t, int);
static void	agten_init_screen(void *, struct vcons_screen *, int, long *);

struct agten_softc {
	device_t	sc_dev;		/* base device */
	struct fbdevice	sc_fb;		/* frame buffer device */

	struct vcons_screen sc_console_screen;
	struct wsscreen_descr sc_defaultscreen_descr;
	const struct wsscreen_descr *sc_screens[1];
	struct wsscreen_list sc_screenlist;

	bus_space_tag_t	sc_bustag;

	bus_space_handle_t 	sc_i128_fbh;
	bus_size_t		sc_i128_fbsz;
	bus_space_handle_t 	sc_i128_regh;
	bus_space_handle_t 	sc_p9100_regh;
	bus_addr_t		sc_glint_fb;
	bus_addr_t		sc_glint_regs;
	uint32_t		sc_glint_fbsz;

	uint32_t	sc_width;
	uint32_t	sc_height;	/* panel width / height */
	uint32_t	sc_stride;
	uint32_t	sc_depth;

	int sc_cursor_x;
	int sc_cursor_y;
	int sc_video;			/* video output enabled */

	/* some /dev/fb* stuff */
	int sc_fb_is_open;

	union	bt_cmap sc_cmap;	/* Brooktree color map */

	int sc_mode;
	uint32_t sc_bg;

	void (*sc_putchar)(void *, int, int, u_int, long);

	struct vcons_data vd;
	glyphcache sc_gc;
};

CFATTACH_DECL_NEW(agten, sizeof(struct agten_softc),
    agten_match, agten_attach, NULL, NULL);


static int	agten_putcmap(struct agten_softc *, struct wsdisplay_cmap *);
static int 	agten_getcmap(struct agten_softc *, struct wsdisplay_cmap *);
static int 	agten_putpalreg(struct agten_softc *, uint8_t, uint8_t,
			    uint8_t, uint8_t);
static void	agten_init(struct agten_softc *);
static void	agten_init_cmap(struct agten_softc *, struct rasops_info *);
static void	agten_gfx(struct agten_softc *);
static void	agten_set_video(struct agten_softc *, int);
static int	agten_get_video(struct agten_softc *);

static void	agten_bitblt(void *, int, int, int, int, int, int, int);
static void 	agten_rectfill(void *, int, int, int, int, long);

static void	agten_putchar(void *, int, int, u_int, long);
static void	agten_cursor(void *, int, int, int);
static void	agten_copycols(void *, int, int, int, int);
static void	agten_erasecols(void *, int, int, int, long);
static void	agten_copyrows(void *, int, int, int);
static void	agten_eraserows(void *, int, int, long);

static void	agten_move_cursor(struct agten_softc *, int, int);
static int	agten_do_cursor(struct agten_softc *sc,
				struct wsdisplay_cursor *);
static int	agten_do_sun_cursor(struct agten_softc *sc,
				struct fbcursor *);

static uint16_t util_interleave(uint8_t, uint8_t);
static uint16_t util_interleave_lin(uint8_t, uint8_t);

extern const u_char rasops_cmap[768];

struct wsdisplay_accessops agten_accessops = {
	agten_ioctl,
	agten_mmap,
	NULL,	/* alloc_screen */
	NULL,	/* free_screen */
	NULL,	/* show_screen */
	NULL, 	/* load_font */
	NULL,	/* pollc */
	NULL	/* scroll */
};

/* /dev/fb* stuff */

static int agten_fb_open(dev_t, int, int, struct lwp *);
static int agten_fb_close(dev_t, int, int, struct lwp *);
static int agten_fb_ioctl(dev_t, u_long, void *, int, struct lwp *);
static paddr_t agten_fb_mmap(dev_t, off_t, int);
static void agten_fb_unblank(device_t);

static struct fbdriver agtenfbdriver = {
	agten_fb_unblank, agten_fb_open, agten_fb_close, agten_fb_ioctl,
	nopoll, agten_fb_mmap, nokqfilter
};

static inline void
agten_write_dac(struct agten_softc *sc, int reg, uint8_t val)
{
	bus_space_write_4(sc->sc_bustag, sc->sc_p9100_regh,
	    0x200 + (reg << 2), (uint32_t)val << 16);
}

static inline void
agten_write_idx(struct agten_softc *sc, int offset)
{
	bus_space_write_4(sc->sc_bustag, sc->sc_p9100_regh,
	    0x200 + (IBM561_ADDR_LOW << 2), (offset & 0xff) << 16);
	bus_space_write_4(sc->sc_bustag, sc->sc_p9100_regh,
	    0x200 + (IBM561_ADDR_HIGH << 2), ((offset >> 8) & 0xff) << 16);
}

static inline void
agten_write_dac_10(struct agten_softc *sc, int reg, uint16_t val)
{
	agten_write_dac(sc, reg, (val >> 2) & 0xff);
	agten_write_dac(sc, reg, (val & 0x3) << 6);
}
	
static int
agten_match(device_t dev, cfdata_t cf, void *aux)
{
	struct sbus_attach_args *sa = aux;

	if (strcmp("PFU,aga", sa->sa_name) == 0)
		return 100;
	return 0;
}

static void
agten_attach(device_t parent, device_t dev, void *aux)
{
	struct agten_softc *sc = device_private(dev);
	struct sbus_attach_args *sa = aux;
	struct fbdevice *fb = &sc->sc_fb;
	struct wsemuldisplaydev_attach_args aa;
	struct rasops_info *ri;
	long defattr;
	uint32_t reg;
	int node = sa->sa_node;
	int console;
 
 	sc->sc_dev = dev;
	sc->sc_defaultscreen_descr = (struct wsscreen_descr){
		"default",
		0, 0,
		NULL,
		8, 16,
		WSSCREEN_WSCOLORS | WSSCREEN_HILIT,
		NULL
	};
	sc->sc_screens[0] = &sc->sc_defaultscreen_descr;
	sc->sc_screenlist = (struct wsscreen_list){1, sc->sc_screens};
	sc->sc_mode = WSDISPLAYIO_MODE_EMUL;
	sc->sc_fb_is_open = 0;
	sc->sc_video = -1;
	sc->sc_bustag = sa->sa_bustag;
	sc->sc_putchar = NULL;

	sc->sc_width = prom_getpropint(node, "ffb_width", 1152);
	sc->sc_height = prom_getpropint(node, "ffb_height", 900);
	sc->sc_depth = prom_getpropint(node, "ffb_depth", 8);
	sc->sc_stride = sc->sc_width * (sc->sc_depth >> 3);

	reg = prom_getpropint(node, "i128_fb_physaddr", -1);
	sc->sc_i128_fbsz = prom_getpropint(node, "i128_fb_size", -1);
	if (sbus_bus_map(sc->sc_bustag,
	    sa->sa_reg[0].oa_space, sa->sa_reg[0].oa_base + reg,
	    round_page(sc->sc_stride * sc->sc_height),
	    BUS_SPACE_MAP_LINEAR | BUS_SPACE_MAP_LARGE, 
	    &sc->sc_i128_fbh) != 0) {

		aprint_error_dev(dev, "unable to map the framebuffer\n");
		return;
	}
	fb->fb_pixels = bus_space_vaddr(sc->sc_bustag, sc->sc_i128_fbh);

	reg = prom_getpropint(node, "i128_reg_physaddr", -1);
	if (sbus_bus_map(sc->sc_bustag,
	    sa->sa_reg[0].oa_space, sa->sa_reg[0].oa_base + reg,
	    0x10000, 0, &sc->sc_i128_regh) != 0) {

		aprint_error_dev(dev, "unable to map I128 registers\n");
		return;
	}

	reg = prom_getpropint(node, "p9100_reg_physaddr", -1);
	if (sbus_bus_map(sc->sc_bustag,
	    sa->sa_reg[0].oa_space, sa->sa_reg[0].oa_base + reg,
	    0x8000, 0, &sc->sc_p9100_regh) != 0) {

		aprint_error_dev(dev, "unable to map P9100 registers\n");
		return;
	}

	reg = prom_getpropint(node, "glint_fb0_physaddr", -1);
	sc->sc_glint_fb = sbus_bus_addr(sc->sc_bustag,
	    sa->sa_reg[0].oa_space, sa->sa_reg[0].oa_base + reg);
	sc->sc_glint_fbsz = prom_getpropint(node, "glint_lb_size", -1);
	reg = prom_getpropint(node, "glint_reg_physaddr", -1);
	sc->sc_glint_regs = sbus_bus_addr(sc->sc_bustag,
	    sa->sa_reg[0].oa_space, sa->sa_reg[0].oa_base + reg);

#if 0
	bus_intr_establish(sc->sc_bustag, sa->sa_pri, IPL_BIO,
	    agten_intr, sc);
#endif

	printf(": %dx%d\n", sc->sc_width, sc->sc_height);
	agten_init(sc);

	console = fb_is_console(node);

	vcons_init(&sc->vd, sc, &sc->sc_defaultscreen_descr,
	    &agten_accessops);
	sc->vd.init_screen = agten_init_screen;

	ri = &sc->sc_console_screen.scr_ri;

	sc->sc_gc.gc_bitblt = agten_bitblt;
	sc->sc_gc.gc_rectfill = agten_rectfill;
	sc->sc_gc.gc_blitcookie = sc;
	sc->sc_gc.gc_rop = CR_COPY;

#if defined(AGTEN_DEBUG)
	sc->sc_height -= 200;
#endif

	if (console) {
		vcons_init_screen(&sc->vd, &sc->sc_console_screen, 1,
		    &defattr);
		sc->sc_console_screen.scr_flags |= VCONS_SCREEN_IS_STATIC;

		sc->sc_defaultscreen_descr.textops = &ri->ri_ops;
		sc->sc_defaultscreen_descr.capabilities = ri->ri_caps;
		sc->sc_defaultscreen_descr.nrows = ri->ri_rows;
		sc->sc_defaultscreen_descr.ncols = ri->ri_cols;
		glyphcache_init(&sc->sc_gc,
		    sc->sc_height + 5,
		    (0x400000 / sc->sc_stride) - sc->sc_height - 5,
		    sc->sc_width,
		    ri->ri_font->fontwidth,
		    ri->ri_font->fontheight,
		    defattr);

		wsdisplay_cnattach(&sc->sc_defaultscreen_descr, ri, 0, 0,
		    defattr);
		i128_rectfill(sc->sc_bustag, sc->sc_i128_regh, 0, 0,
		    sc->sc_width, sc->sc_height,
		    ri->ri_devcmap[(defattr >> 16) & 0xff]);
		vcons_replay_msgbuf(&sc->sc_console_screen);
	} else {
		/*
		 * since we're not the console we can postpone the rest
		 * until someone actually allocates a screen for us
		 */
		if (sc->sc_console_screen.scr_ri.ri_rows == 0) {
			/* do some minimal setup to avoid weirdnesses later */
			vcons_init_screen(&sc->vd, &sc->sc_console_screen, 1,
			    &defattr);
		} else
			(*ri->ri_ops.allocattr)(ri, 0, 0, 0, &defattr);

		glyphcache_init(&sc->sc_gc,
		    sc->sc_height + 5,
		    (0x400000 / sc->sc_stride) - sc->sc_height - 5,
		    sc->sc_width,
		    ri->ri_font->fontwidth,
		    ri->ri_font->fontheight,
		    defattr);
	}

	/* Initialize the default color map. */
	agten_init_cmap(sc, ri);

	aa.console = console;
	aa.scrdata = &sc->sc_screenlist;
	aa.accessops = &agten_accessops;
	aa.accesscookie = &sc->vd;

	config_found(sc->sc_dev, &aa, wsemuldisplaydevprint);

	fb->fb_driver = &agtenfbdriver;
	fb->fb_device = sc->sc_dev;
	fb->fb_flags = device_cfdata(sc->sc_dev)->cf_flags & FB_USERMASK;
	fb->fb_type.fb_type = FBTYPE_AG10E;
	fb->fb_type.fb_cmsize = 256;	/* doesn't matter, we're always 24bit */
	fb->fb_type.fb_size = sc->sc_glint_fbsz;
	fb->fb_type.fb_width = sc->sc_width;
	fb->fb_type.fb_height = sc->sc_height;
	fb->fb_type.fb_depth = 32;
	fb->fb_linebytes = sc->sc_stride << 2;
	fb_attach(fb, console);
	agten_set_video(sc, 1);	/* make sure video's on */
}

static int
agten_ioctl(void *v, void *vs, u_long cmd, void *data, int flag,
	struct lwp *l)
{
	struct vcons_data *vd = v;
	struct agten_softc *sc = vd->cookie;
	struct wsdisplay_fbinfo *wdf;
	struct vcons_screen *ms = vd->active;

	switch (cmd) {

		case WSDISPLAYIO_GTYPE:
			*(u_int *)data = WSDISPLAY_TYPE_AG10;
			return 0;

		case WSDISPLAYIO_GINFO:
			if (ms == NULL)
				return ENODEV;
			wdf = (void *)data;
			wdf->height = ms->scr_ri.ri_height;
			wdf->width = ms->scr_ri.ri_width;
			wdf->depth = 32;
			wdf->cmsize = 256;
			return 0;

		case WSDISPLAYIO_GVIDEO:
			*(int *)data = sc->sc_video;
			return 0;

		case WSDISPLAYIO_SVIDEO:
			agten_set_video(sc, *(int *)data);
			return 0;

		case WSDISPLAYIO_GETCMAP:
			return agten_getcmap(sc,
			    (struct wsdisplay_cmap *)data);

		case WSDISPLAYIO_PUTCMAP:
			return agten_putcmap(sc,
			    (struct wsdisplay_cmap *)data);

		case WSDISPLAYIO_LINEBYTES:
			*(u_int *)data = sc->sc_stride << 2;
			return 0;

		case WSDISPLAYIO_SMODE:
			{
				int new_mode = *(int*)data;
				if (new_mode != sc->sc_mode) {
					sc->sc_mode = new_mode;
					if(new_mode == WSDISPLAYIO_MODE_EMUL) {
						agten_init(sc);
						agten_init_cmap(sc,
						    &ms->scr_ri);
						vcons_redraw_screen(ms);
					} else {
						agten_gfx(sc);
					}
				}
			}
			return 0;

		case WSDISPLAYIO_GCURPOS:
			{
				struct wsdisplay_curpos *cp = (void *)data;

				cp->x = sc->sc_cursor_x;
				cp->y = sc->sc_cursor_y;
			}
			return 0;

		case WSDISPLAYIO_SCURPOS:
			{
				struct wsdisplay_curpos *cp = (void *)data;

				agten_move_cursor(sc, cp->x, cp->y);
			}
			return 0;

		case WSDISPLAYIO_GCURMAX:
			{
				struct wsdisplay_curpos *cp = (void *)data;

				cp->x = 64;
				cp->y = 64;
			}
			return 0;

		case WSDISPLAYIO_SCURSOR:
			{
				struct wsdisplay_cursor *cursor = (void *)data;

				return agten_do_cursor(sc, cursor);
			}
	}
	return EPASSTHROUGH;
}

static paddr_t
agten_mmap(void *v, void *vs, off_t offset, int prot)
{
	struct vcons_data *vd = v;
	struct agten_softc *sc = vd->cookie;

	if (offset < sc->sc_glint_fbsz)
		return bus_space_mmap(sc->sc_bustag, sc->sc_glint_fb, offset,
		    prot, BUS_SPACE_MAP_LINEAR);
	return -1;
}

static void
agten_init_screen(void *cookie, struct vcons_screen *scr,
    int existing, long *defattr)
{
	struct agten_softc *sc = cookie;
	struct rasops_info *ri = &scr->scr_ri;

	ri->ri_depth = sc->sc_depth;
	ri->ri_width = sc->sc_width;
	ri->ri_height = sc->sc_height;
	ri->ri_stride = sc->sc_stride;
	ri->ri_flg = RI_CENTER | RI_FULLCLEAR | RI_8BIT_IS_RGB | RI_ENABLE_ALPHA;

	ri->ri_bits = (char *)sc->sc_fb.fb_pixels;

	if (existing) {
		ri->ri_flg |= RI_CLEAR;
	}

	rasops_init(ri, 0, 0);
	sc->sc_putchar = ri->ri_ops.putchar;

	ri->ri_caps = WSSCREEN_WSCOLORS;

	rasops_reconfig(ri, sc->sc_height / ri->ri_font->fontheight,
		    sc->sc_width / ri->ri_font->fontwidth);

	ri->ri_hw = scr;
	ri->ri_ops.putchar   = agten_putchar;
	ri->ri_ops.cursor    = agten_cursor;
	ri->ri_ops.copyrows  = agten_copyrows;
	ri->ri_ops.eraserows = agten_eraserows;
	ri->ri_ops.copycols  = agten_copycols;
	ri->ri_ops.erasecols = agten_erasecols;

}

static int
agten_putcmap(struct agten_softc *sc, struct wsdisplay_cmap *cm)
{
	u_int index = cm->index;
	u_int count = cm->count;
	int i, error;
	u_char rbuf[256], gbuf[256], bbuf[256];
	u_char *r, *g, *b;

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

	r = &rbuf[index];
	g = &gbuf[index];
	b = &bbuf[index];

	for (i = 0; i < count; i++) {
		agten_putpalreg(sc, index, *r, *g, *b);
		index++;
		r++, g++, b++;
	}
	return 0;
}

static int
agten_getcmap(struct agten_softc *sc, struct wsdisplay_cmap *cm)
{
	u_int index = cm->index;
	u_int count = cm->count;
	int error, i;
	uint8_t red[256], green[256], blue[256];

	if (index >= 255 || count > 256 || index + count > 256)
		return EINVAL;

	i = index;
	while (i < (index + count)) {
		red[i] = sc->sc_cmap.cm_map[i][0];
		green[i] = sc->sc_cmap.cm_map[i][1];
		blue[i] = sc->sc_cmap.cm_map[i][2];
		i++;
	}
	error = copyout(&red[index],   cm->red,   count);
	if (error)
		return error;
	error = copyout(&green[index], cm->green, count);
	if (error)
		return error;
	error = copyout(&blue[index],  cm->blue,  count);
	if (error)
		return error;

	return 0;
}

static int
agten_putpalreg(struct agten_softc *sc, uint8_t idx, uint8_t r, uint8_t g,
    uint8_t b)
{

	sc->sc_cmap.cm_map[idx][0] = r;
	sc->sc_cmap.cm_map[idx][1] = g;
	sc->sc_cmap.cm_map[idx][2] = b;
	agten_write_idx(sc, IBM561_CMAP_TABLE + idx);
	agten_write_dac(sc, IBM561_CMD_CMAP, r);
	agten_write_dac(sc, IBM561_CMD_CMAP, g);
	agten_write_dac(sc, IBM561_CMD_CMAP, b);
	return 0;
}

static void
agten_init(struct agten_softc *sc)
{
	int i;
	uint32_t src, srcw;

	/* then we set up a linear LUT for 24bit colour */
	agten_write_idx(sc, IBM561_CMAP_TABLE + 256);
	for (i = 0; i < 256; i++) {
		agten_write_dac(sc, IBM561_CMD_CMAP, i);
		agten_write_dac(sc, IBM561_CMD_CMAP, i);
		agten_write_dac(sc, IBM561_CMD_CMAP, i);
	}
	
	/* and the linear gamma maps */
	agten_write_idx(sc, IBM561_RED_GAMMA_TABLE);
	for (i = 0; i < 0x3ff; i+= 4)
		agten_write_dac_10(sc, IBM561_CMD_GAMMA, i);
	agten_write_idx(sc, IBM561_GREEN_GAMMA_TABLE);
	for (i = 0; i < 0x3ff; i+= 4)
		agten_write_dac_10(sc, IBM561_CMD_GAMMA, i);
	agten_write_idx(sc, IBM561_BLUE_GAMMA_TABLE);
	for (i = 0; i < 0x3ff; i+= 4)
		agten_write_dac_10(sc, IBM561_CMD_GAMMA, i);

	/* enable outputs, RGB mode */
	agten_write_idx(sc, IBM561_CONFIG_REG3);
	agten_write_dac(sc, IBM561_CMD, CR3_SERIAL_CLK_CTRL | CR3_RGB);

	/* MUX 4:1 basic, 8bit overlay, 8bit WIDs */
	agten_write_idx(sc, IBM561_CONFIG_REG1);
	agten_write_dac(sc, IBM561_CMD, CR1_MODE_4_1_BASIC | CR1_OVL_8BPP | 
	    CR1_WID_8);

	/* use external clock, enable video output */
	agten_write_idx(sc, IBM561_CONFIG_REG2);
	agten_write_dac(sc, IBM561_CMD, CR2_ENABLE_CLC | CR2_PLL_REF_SELECT |
	    CR2_PIXEL_CLOCK_SELECT | CR2_ENABLE_RGB_OUTPUT);

	/* now set up some window attributes */
	
	/* 
	 * direct colour, 24 bit, transparency off, LUT from 0x100
	 * we need to use direct colour and a linear LUT because for some
	 * reason true color mode gives messed up colours
	 */
	agten_write_idx(sc, IBM561_FB_WINTYPE);
	agten_write_dac_10(sc, IBM561_CMD_FB_WAT, 0x100 | FB_PIXEL_24BIT | 
	    FB_MODE_DIRECT);

	/* use gamma LUTs, no crosshair, 0 is transparent */
	agten_write_idx(sc, IBM561_AUXFB_WINTYPE);
	agten_write_dac(sc, IBM561_CMD_FB_WAT, 0x0);	

	/* overlay is 8 bit, opaque */
	agten_write_idx(sc, IBM561_OL_WINTYPE);
	agten_write_dac_10(sc, IBM561_CMD_FB_WAT, 0x00);

	/* now we fill the WID fb with zeroes */
	src = 0;
	srcw = sc->sc_width << 16 | sc->sc_height;
	bus_space_write_4(sc->sc_bustag, sc->sc_p9100_regh, FOREGROUND_COLOR,
	    0x0);
	bus_space_write_4(sc->sc_bustag, sc->sc_p9100_regh, BACKGROUND_COLOR,
	    0x0);
	bus_space_write_4(sc->sc_bustag, sc->sc_p9100_regh, RASTER_OP, ROP_PAT);
	bus_space_write_4(sc->sc_bustag, sc->sc_p9100_regh, COORD_INDEX, 0);
	bus_space_write_4(sc->sc_bustag, sc->sc_p9100_regh, RECT_RTW_XY, src);
	bus_space_write_4(sc->sc_bustag, sc->sc_p9100_regh, RECT_RTW_XY, srcw);
	(void)bus_space_read_4(sc->sc_bustag, sc->sc_p9100_regh, COMMAND_QUAD);

	/* initialize the cursor registers */
	
	/* initialize the Imagine 128 */
	i128_init(sc->sc_bustag, sc->sc_i128_regh, sc->sc_stride, 8);
}

static void
agten_init_cmap(struct agten_softc *sc, struct rasops_info *ri)
{
	int i, j;
	uint8_t cmap[768];

	rasops_get_cmap(ri, cmap, 768);
	j = 0;
	for (i = 0; i < 256; i++) {

		agten_putpalreg(sc, i, cmap[j], cmap[j + 1], cmap[j + 2]);
		j += 3;
	}
}

static void
agten_gfx(struct agten_softc *sc)
{
	/* enable overlay transparency on colour 0x00 */
	agten_write_idx(sc, IBM561_OL_WINTYPE);
	agten_write_dac_10(sc, IBM561_CMD_FB_WAT, OL_MODE_TRANSP_ENABLE);

	/* then blit the overlay full of 0x00 */
	i128_rectfill(sc->sc_bustag, sc->sc_i128_regh, 0, 0, sc->sc_width,
	    sc->sc_height, 0);

	/* ... so we can see the 24bit framebuffer */	
}	

static void
agten_set_video(struct agten_softc *sc, int flag)
{
	uint8_t reg = 
	    CR2_ENABLE_CLC | CR2_PLL_REF_SELECT | CR2_PIXEL_CLOCK_SELECT;

	if (flag == sc->sc_video)
		return;

	agten_write_idx(sc, IBM561_CONFIG_REG2);
	agten_write_dac(sc, IBM561_CMD, flag ? reg | CR2_ENABLE_RGB_OUTPUT :
	    reg);

	sc->sc_video = flag;
}

static int
agten_get_video(struct agten_softc *sc)
{

	return sc->sc_video;
}

static void
agten_bitblt(void *cookie, int xs, int ys, int xd, int yd, int wi, int he,
             int rop)
{
	struct agten_softc *sc = cookie;

	i128_bitblt(sc->sc_bustag, sc->sc_i128_regh,
	    xs, ys, xd, yd, wi, he, rop);
}
	
static void
agten_rectfill(void *cookie, int x, int y, int wi, int he, long fg)
{
	struct agten_softc *sc = cookie;
	struct vcons_screen *scr = sc->vd.active;
	uint32_t col;

	if (scr == NULL)
		return;
	col = scr->scr_ri.ri_devcmap[fg];
	i128_rectfill(sc->sc_bustag, sc->sc_i128_regh, x, y, wi, he, col);	
}

static void
agten_putchar(void *cookie, int row, int col, u_int c, long attr)
{
	struct rasops_info *ri = cookie;
	struct wsdisplay_font *font = PICK_FONT(ri, c);
	struct vcons_screen *scr = ri->ri_hw;
	struct agten_softc *sc = scr->scr_cookie;
	uint32_t fg, bg;
	int x, y, wi, he, rv;

	wi = font->fontwidth;
	he = font->fontheight;

	bg = ri->ri_devcmap[(attr >> 16) & 0xf];
	fg = ri->ri_devcmap[(attr >> 24) & 0xf];

	x = ri->ri_xorigin + col * wi;
	y = ri->ri_yorigin + row * he;

	if (c == 0x20) {
		i128_rectfill(sc->sc_bustag, sc->sc_i128_regh, x, y, wi, he,
		    bg);
		if (attr & 1)
			i128_rectfill(sc->sc_bustag, sc->sc_i128_regh, x,
			    y + he - 2, wi, 1, fg);
		return;
	}
	rv = glyphcache_try(&sc->sc_gc, c, x, y, attr);
	if (rv == GC_OK)
		return;
	i128_sync(sc->sc_bustag, sc->sc_i128_regh);
	sc->sc_putchar(cookie, row, col, c, attr & ~1);

	if (rv == GC_ADD) {
		glyphcache_add(&sc->sc_gc, c, x, y);
	} else {
		if (attr & 1)
			i128_rectfill(sc->sc_bustag, sc->sc_i128_regh, x,
			    y + he - 2, wi, 1, fg);
	}
}

static void
agten_cursor(void *cookie, int on, int row, int col)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;
	struct agten_softc *sc = scr->scr_cookie;
	int x, y, wi,he;

	wi = ri->ri_font->fontwidth;
	he = ri->ri_font->fontheight;

	if (ri->ri_flg & RI_CURSOR) {
		x = ri->ri_ccol * wi + ri->ri_xorigin;
		y = ri->ri_crow * he + ri->ri_yorigin;
		i128_bitblt(sc->sc_bustag, sc->sc_i128_regh, x, y, x, y, wi, he,
		    CR_COPY_INV);
		ri->ri_flg &= ~RI_CURSOR;
	}

	ri->ri_crow = row;
	ri->ri_ccol = col;

	if (on)
	{
		x = ri->ri_ccol * wi + ri->ri_xorigin;
		y = ri->ri_crow * he + ri->ri_yorigin;
		i128_bitblt(sc->sc_bustag, sc->sc_i128_regh, x, y, x, y, wi, he,
		    CR_COPY_INV);
		ri->ri_flg |= RI_CURSOR;
	}
}

static void
agten_copycols(void *cookie, int row, int srccol, int dstcol, int ncols)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;
	struct agten_softc *sc = scr->scr_cookie;
	int32_t xs, xd, y, width, height;

	xs = ri->ri_xorigin + ri->ri_font->fontwidth * srccol;
	xd = ri->ri_xorigin + ri->ri_font->fontwidth * dstcol;
	y = ri->ri_yorigin + ri->ri_font->fontheight * row;
	width = ri->ri_font->fontwidth * ncols;
	height = ri->ri_font->fontheight;
	i128_bitblt(sc->sc_bustag, sc->sc_i128_regh, xs, y, xd, y, width,
	    height, CR_COPY);
}

static void
agten_erasecols(void *cookie, int row, int startcol, int ncols, long fillattr)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;
	struct agten_softc *sc = scr->scr_cookie;
	int32_t x, y, width, height, bg;

	x = ri->ri_xorigin + ri->ri_font->fontwidth * startcol;
	y = ri->ri_yorigin + ri->ri_font->fontheight * row;
	width = ri->ri_font->fontwidth * ncols;
	height = ri->ri_font->fontheight;
	bg = (uint32_t)ri->ri_devcmap[(fillattr >> 16) & 0xff];
	i128_rectfill(sc->sc_bustag, sc->sc_i128_regh, x, y, width, height, bg);
}

static void
agten_copyrows(void *cookie, int srcrow, int dstrow, int nrows)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;
	struct agten_softc *sc = scr->scr_cookie;
	int32_t x, ys, yd, width, height;

	x = ri->ri_xorigin;
	ys = ri->ri_yorigin + ri->ri_font->fontheight * srcrow;
	yd = ri->ri_yorigin + ri->ri_font->fontheight * dstrow;
	width = ri->ri_emuwidth;
	height = ri->ri_font->fontheight * nrows;
	i128_bitblt(sc->sc_bustag, sc->sc_i128_regh, x, ys, x, yd, width,
	    height, CR_COPY);
}

static void
agten_eraserows(void *cookie, int row, int nrows, long fillattr)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;
	struct agten_softc *sc = scr->scr_cookie;
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
	bg = (uint32_t)ri->ri_devcmap[(fillattr >> 16) & 0xff];
	i128_rectfill(sc->sc_bustag, sc->sc_i128_regh, x, y, width, height, bg);
}

static void
agten_move_cursor(struct agten_softc *sc, int x, int y)
{

	sc->sc_cursor_x = x;
	sc->sc_cursor_y = y;
	agten_write_idx(sc, IBM561_CURSOR_X_REG);
	agten_write_dac(sc, IBM561_CMD, x & 0xff);
	agten_write_dac(sc, IBM561_CMD, (x >> 8) & 0xff);
	agten_write_dac(sc, IBM561_CMD, y & 0xff);
	agten_write_dac(sc, IBM561_CMD, (y >> 8) & 0xff);
}

static int
agten_do_cursor(struct agten_softc *sc, struct wsdisplay_cursor *cur)
{
	if (cur->which & WSDISPLAY_CURSOR_DOCUR) {

		agten_write_idx(sc, IBM561_CURS_CNTL_REG);
		agten_write_dac(sc, IBM561_CMD, cur->enable ?
		    CURS_ENABLE : 0);
	}
	if (cur->which & WSDISPLAY_CURSOR_DOHOT) {

		agten_write_idx(sc, IBM561_HOTSPOT_X_REG);
		agten_write_dac(sc, IBM561_CMD, cur->hot.x);
		agten_write_dac(sc, IBM561_CMD, cur->hot.y);
	}
	if (cur->which & WSDISPLAY_CURSOR_DOPOS) {

		agten_move_cursor(sc, cur->pos.x, cur->pos.y);
	}
	if (cur->which & WSDISPLAY_CURSOR_DOCMAP) {
		int i;
	
		agten_write_idx(sc, IBM561_CURSOR_LUT + cur->cmap.index + 2);
		for (i = 0; i < cur->cmap.count; i++) {
			agten_write_dac(sc, IBM561_CMD_CMAP, cur->cmap.red[i]);
			agten_write_dac(sc, IBM561_CMD_CMAP, 
			    cur->cmap.green[i]);
			agten_write_dac(sc, IBM561_CMD_CMAP, cur->cmap.blue[i]);
		}
	}
	if (cur->which & WSDISPLAY_CURSOR_DOSHAPE) {
		int i;
		uint16_t tmp;

		agten_write_idx(sc, IBM561_CURSOR_BITMAP);
		for (i = 0; i < 512; i++) {
			tmp = util_interleave(cur->mask[i], cur->image[i]);
			agten_write_dac(sc, IBM561_CMD, (tmp >> 8) & 0xff);
			agten_write_dac(sc, IBM561_CMD, tmp & 0xff);
		}
	}
	return 0;
}

static int
agten_do_sun_cursor(struct agten_softc *sc, struct fbcursor *cur)
{
	if (cur->set & FB_CUR_SETCUR) {

		agten_write_idx(sc, IBM561_CURS_CNTL_REG);
		agten_write_dac(sc, IBM561_CMD, cur->enable ?
		    CURS_ENABLE : 0);
	}
	if (cur->set & FB_CUR_SETHOT) {

		agten_write_idx(sc, IBM561_HOTSPOT_X_REG);
		agten_write_dac(sc, IBM561_CMD, cur->hot.x);
		agten_write_dac(sc, IBM561_CMD, cur->hot.y);
	}
	if (cur->set & FB_CUR_SETPOS) {

		agten_move_cursor(sc, cur->pos.x, cur->pos.y);
	}
	if (cur->set & FB_CUR_SETCMAP) {
		int i;
	
		agten_write_idx(sc, IBM561_CURSOR_LUT + cur->cmap.index + 2);
		for (i = 0; i < cur->cmap.count; i++) {
			agten_write_dac(sc, IBM561_CMD_CMAP, cur->cmap.red[i]);
			agten_write_dac(sc, IBM561_CMD_CMAP, 
			    cur->cmap.green[i]);
			agten_write_dac(sc, IBM561_CMD_CMAP, cur->cmap.blue[i]);
		}
	}
	if (cur->set & FB_CUR_SETSHAPE) {
		int i;
		uint16_t tmp;

		agten_write_idx(sc, IBM561_CURSOR_BITMAP);
		for (i = 0; i < 512; i++) {
			tmp = util_interleave_lin(cur->mask[i], cur->image[i]);
			agten_write_dac(sc, IBM561_CMD, (tmp >> 8) & 0xff);
			agten_write_dac(sc, IBM561_CMD, tmp & 0xff);
		}
	}
	return 0;
}

uint16_t
util_interleave(uint8_t b1, uint8_t b2)
{
	int i;
	uint16_t ret = 0;
	uint16_t mask = 0x8000;
	uint8_t mask8 = 0x01;

	for (i = 0; i < 8; i++) {
		if (b1 & mask8)
			ret |= mask;
		mask = mask >> 1;
		if (b2 & mask8)
			ret |= mask;
		mask = mask >> 1;
		mask8 = mask8 << 1;
	}
	return ret;
}

uint16_t
util_interleave_lin(uint8_t b1, uint8_t b2)
{
	int i;
	uint16_t ret = 0;
	uint16_t mask = 0x8000;
	uint8_t mask8 = 0x80;

	for (i = 0; i < 8; i++) {
		if (b1 & mask8)
			ret |= mask;
		mask = mask >> 1;
		if (b2 & mask8)
			ret |= mask;
		mask = mask >> 1;
		mask8 = mask8 >> 1;
	}
	return ret;
}

/* and now the /dev/fb* stuff */
static void
agten_fb_unblank(device_t dev)
{
	struct agten_softc *sc = device_private(dev);

	agten_init(sc);
	agten_set_video(sc, 1);
}

static int
agten_fb_open(dev_t dev, int flags, int mode, struct lwp *l)
{
	struct agten_softc *sc;

	sc = device_lookup_private(&agten_cd, minor(dev));
	if (sc == NULL)
		return (ENXIO);
	if (sc->sc_fb_is_open)
		return 0;

	sc->sc_fb_is_open++;
	agten_gfx(sc);

	return (0);
}

static int
agten_fb_close(dev_t dev, int flags, int mode, struct lwp *l)
{
	struct agten_softc *sc;

	sc = device_lookup_private(&agten_cd, minor(dev));

	sc->sc_fb_is_open--;
	if (sc->sc_fb_is_open < 0)
		sc->sc_fb_is_open = 0;

	if (sc->sc_fb_is_open == 0) {
		agten_init(sc);
		vcons_redraw_screen(sc->vd.active);
	}

	return (0);
}

static int
agten_fb_ioctl(dev_t dev, u_long cmd, void *data, int flags, struct lwp *l)
{
	struct agten_softc *sc = device_lookup_private(&agten_cd, minor(dev));
	struct fbgattr *fba;
	int error;

	switch (cmd) {

	case FBIOGTYPE:
		*(struct fbtype *)data = sc->sc_fb.fb_type;
		break;

	case FBIOGATTR:
		fba = (struct fbgattr *)data;
		fba->real_type = sc->sc_fb.fb_type.fb_type;
		fba->owner = 0;		/* XXX ??? */
		fba->fbtype = sc->sc_fb.fb_type;
		fba->sattr.flags = 0;
		fba->sattr.emu_type = sc->sc_fb.fb_type.fb_type;
		fba->sattr.dev_specific[0] = -1;
		fba->emu_types[0] = sc->sc_fb.fb_type.fb_type;
		fba->emu_types[1] = -1;
		break;

	case FBIOGETCMAP:
#define p ((struct fbcmap *)data)
		return (bt_getcmap(p, &sc->sc_cmap, 256, 1));

	case FBIOPUTCMAP:
		/* copy to software map */
		error = bt_putcmap(p, &sc->sc_cmap, 256, 1);
		if (error)
			return (error);
		/* now blast them into the chip */
		/* don't bother - we're 24bit */
#undef p
		break;

	case FBIOGVIDEO:
		*(int *)data = agten_get_video(sc);
		break;

	case FBIOSVIDEO:
		agten_set_video(sc, *(int *)data);
		break;

/* these are for both FBIOSCURSOR and FBIOGCURSOR */
#define p ((struct fbcursor *)data)
#define pc (&sc->sc_cursor)

	case FBIOGCURSOR:
		/* does anyone use this ioctl?! */
		p->set = FB_CUR_SETALL;	/* close enough, anyway */
		p->enable = 1;
		p->pos.x = sc->sc_cursor_x;
		p->pos.y = sc->sc_cursor_y;
		p->size.x = 64;
		p->size.y = 64;
		break;

	case FBIOSCURSOR:
		agten_do_sun_cursor(sc, p);
	break;

#undef p
#undef cc

	case FBIOGCURPOS:
	{
		struct fbcurpos *cp = (struct fbcurpos *)data;
		cp->x = sc->sc_cursor_x;
		cp->y = sc->sc_cursor_y;
	}
	break;

	case FBIOSCURPOS:
	{
		struct fbcurpos *cp = (struct fbcurpos *)data;
		agten_move_cursor(sc, cp->x, cp->y);
	}
	break;

	case FBIOGCURMAX:
		/* max cursor size is 64x64 */
		((struct fbcurpos *)data)->x = 64;
		((struct fbcurpos *)data)->y = 64;
		break;

	default:
		return (ENOTTY);
	}
	return (0);
}

static paddr_t
agten_fb_mmap(dev_t dev, off_t off, int prot)
{
	struct agten_softc *sc = device_lookup_private(&agten_cd, minor(dev));

	/*
	 * mappings are subject to change
	 * for now we put the framebuffer at offset 0 and the GLint registers
	 * right after that. We may want to expose more register ranges and
	 * probably will want to map the 2nd framebuffer as well
	 */

	if (off < 0)
		return EINVAL;

	if (off >= sc->sc_glint_fbsz + 0x10000)
		return EINVAL;

	if (off < sc->sc_glint_fbsz) {
		return (bus_space_mmap(sc->sc_bustag,
			sc->sc_glint_fb,
			off,
			prot,
			BUS_SPACE_MAP_LINEAR));
	}

	off -= sc->sc_glint_fbsz;
	if (off < 0x10000) {
		return (bus_space_mmap(sc->sc_bustag,
			sc->sc_glint_regs,
			off,
			prot,
			BUS_SPACE_MAP_LINEAR));
	}
	return EINVAL;
}
