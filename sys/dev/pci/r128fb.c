/*	$NetBSD: r128fb.c,v 1.39 2015/09/16 16:52:54 macallan Exp $	*/

/*
 * Copyright (c) 2007, 2012 Michael Lorenz
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
 * A console driver for ATI Rage 128 graphics controllers
 * tested on macppc only so far
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: r128fb.c,v 1.39 2015/09/16 16:52:54 macallan Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/lwp.h>
#include <sys/kauth.h>

#include <dev/videomode/videomode.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>
#include <dev/pci/pciio.h>
#include <dev/pci/r128fbreg.h>

#include <dev/wscons/wsdisplayvar.h>
#include <dev/wscons/wsconsio.h>
#include <dev/wsfont/wsfont.h>
#include <dev/rasops/rasops.h>
#include <dev/wscons/wsdisplay_vconsvar.h>
#include <dev/pci/wsdisplay_pci.h>
#include <dev/wscons/wsdisplay_glyphcachevar.h>

#include <dev/i2c/i2cvar.h>

#include "opt_r128fb.h"
#include "opt_vcons.h"

#ifdef R128FB_DEBUG
#define DPRINTF printf
#else
#define DPRINTF while(0) printf
#endif

struct r128fb_softc {
	device_t sc_dev;

	pci_chipset_tag_t sc_pc;
	pcitag_t sc_pcitag;

	bus_space_tag_t sc_memt;
	bus_space_tag_t sc_iot;

	bus_space_handle_t sc_regh;
	bus_addr_t sc_fb, sc_reg;
	bus_size_t sc_fbsize, sc_regsize;

	int sc_width, sc_height, sc_depth, sc_stride;
	int sc_locked, sc_have_backlight, sc_bl_level, sc_bl_on;
	struct vcons_screen sc_console_screen;
	struct wsscreen_descr sc_defaultscreen_descr;
	const struct wsscreen_descr *sc_screens[1];
	struct wsscreen_list sc_screenlist;
	struct vcons_data vd;
	int sc_mode;
	u_char sc_cmap_red[256];
	u_char sc_cmap_green[256];
	u_char sc_cmap_blue[256];
	/* engine stuff */
	uint32_t sc_master_cntl;
	glyphcache sc_gc;
};

static int	r128fb_match(device_t, cfdata_t, void *);
static void	r128fb_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(r128fb, sizeof(struct r128fb_softc),
    r128fb_match, r128fb_attach, NULL, NULL);

static int	r128fb_ioctl(void *, void *, u_long, void *, int,
			     struct lwp *);
static paddr_t	r128fb_mmap(void *, void *, off_t, int);
static void	r128fb_init_screen(void *, struct vcons_screen *, int, long *);

static int	r128fb_putcmap(struct r128fb_softc *, struct wsdisplay_cmap *);
static int 	r128fb_getcmap(struct r128fb_softc *, struct wsdisplay_cmap *);
static void	r128fb_restore_palette(struct r128fb_softc *);
static int 	r128fb_putpalreg(struct r128fb_softc *, uint8_t, uint8_t,
			    uint8_t, uint8_t);

static void	r128fb_init(struct r128fb_softc *);
static void	r128fb_flush_engine(struct r128fb_softc *);
static void	r128fb_rectfill(struct r128fb_softc *, int, int, int, int,
			    uint32_t);
static void	r128fb_bitblt(void *, int, int, int, int, int,
			    int, int);

static void	r128fb_cursor(void *, int, int, int);
static void	r128fb_putchar(void *, int, int, u_int, long);
static void	r128fb_putchar_aa(void *, int, int, u_int, long);
static void	r128fb_copycols(void *, int, int, int, int);
static void	r128fb_erasecols(void *, int, int, int, long);
static void	r128fb_copyrows(void *, int, int, int);
static void	r128fb_eraserows(void *, int, int, long);

static void	r128fb_brightness_up(device_t);
static void	r128fb_brightness_down(device_t);
/* set backlight level */
static void	r128fb_set_backlight(struct r128fb_softc *, int);
/* turn backlight on and off without messing with the level */
static void	r128fb_switch_backlight(struct r128fb_softc *, int);

struct wsdisplay_accessops r128fb_accessops = {
	r128fb_ioctl,
	r128fb_mmap,
	NULL,	/* alloc_screen */
	NULL,	/* free_screen */
	NULL,	/* show_screen */
	NULL, 	/* load_font */
	NULL,	/* pollc */
	NULL	/* scroll */
};

static inline void
r128fb_wait(struct r128fb_softc *sc, int slots)
{
	uint32_t reg;

	do {
		reg = (bus_space_read_4(sc->sc_memt, sc->sc_regh,
		    R128_GUI_STAT) & R128_GUI_FIFOCNT_MASK);
	} while (reg <= slots);
}

static void
r128fb_flush_engine(struct r128fb_softc *sc)
{
	uint32_t reg;

	reg = bus_space_read_4(sc->sc_memt, sc->sc_regh, R128_PC_NGUI_CTLSTAT);
	reg |= R128_PC_FLUSH_ALL;
	bus_space_write_4(sc->sc_memt, sc->sc_regh, R128_PC_NGUI_CTLSTAT, reg);
	do {
		reg = bus_space_read_4(sc->sc_memt, sc->sc_regh,
		    R128_PC_NGUI_CTLSTAT);
	} while (reg & R128_PC_BUSY);
}

static int
r128fb_match(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa = (struct pci_attach_args *)aux;

	if (PCI_CLASS(pa->pa_class) != PCI_CLASS_DISPLAY)
		return 0;
	if (PCI_VENDOR(pa->pa_id) != PCI_VENDOR_ATI)
		return 0;

	/* only cards tested on so far - likely need a list */
	if ((PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_ATI_RAGE1AGP4XT) ||
	    (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_ATI_RAGE3AGP4XT) ||
	    (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_ATI_RAGEGLPCI) ||
	    (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_ATI_RAGE_MOB_M3_AGP))
		return 100;
	return (0);
}

static void
r128fb_attach(device_t parent, device_t self, void *aux)
{
	struct r128fb_softc	*sc = device_private(self);
	struct pci_attach_args	*pa = aux;
	struct rasops_info	*ri;
	bus_space_tag_t		tag;
	struct wsemuldisplaydev_attach_args aa;
	prop_dictionary_t	dict;
	unsigned long		defattr;
	bool			is_console = FALSE;
	int			i, j;
	uint32_t		reg, flags;
	uint8_t			cmap[768];

	sc->sc_pc = pa->pa_pc;
	sc->sc_pcitag = pa->pa_tag;
	sc->sc_memt = pa->pa_memt;
	sc->sc_iot = pa->pa_iot;
	sc->sc_dev = self;

	pci_aprint_devinfo(pa, NULL);

	/* fill in parameters from properties */
	dict = device_properties(self);
	if (!prop_dictionary_get_uint32(dict, "width", &sc->sc_width)) {
		aprint_error("%s: no width property\n", device_xname(self));
		return;
	}
	if (!prop_dictionary_get_uint32(dict, "height", &sc->sc_height)) {
		aprint_error("%s: no height property\n", device_xname(self));
		return;
	}

#ifdef GLYPHCACHE_DEBUG
	/* leave some visible VRAM unused so we can see the glyph cache */
	sc->sc_height -= 200;
#endif

	if (!prop_dictionary_get_uint32(dict, "depth", &sc->sc_depth)) {
		aprint_error("%s: no depth property\n", device_xname(self));
		return;
	}
	if (!prop_dictionary_get_uint32(dict, "linebytes", &sc->sc_stride)) {
		aprint_error("%s: no linebytes property\n",
		    device_xname(self));
		return;
	}

	prop_dictionary_get_bool(dict, "is_console", &is_console);

	if (pci_mapreg_info(pa->pa_pc, pa->pa_tag, 0x10, PCI_MAPREG_TYPE_MEM,
	    &sc->sc_fb, &sc->sc_fbsize, &flags)) {
		aprint_error("%s: failed to map the frame buffer.\n",
		    device_xname(sc->sc_dev));
	}

	if (pci_mapreg_map(pa, 0x18, PCI_MAPREG_TYPE_MEM, 0,
	    &tag, &sc->sc_regh, &sc->sc_reg, &sc->sc_regsize)) {
		aprint_error("%s: failed to map registers.\n",
		    device_xname(sc->sc_dev));
	}

	aprint_normal("%s: %d MB aperture at 0x%08x\n", device_xname(self),
	    (int)(sc->sc_fbsize >> 20), (uint32_t)sc->sc_fb);

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
	sc->sc_locked = 0;

	vcons_init(&sc->vd, sc, &sc->sc_defaultscreen_descr,
	    &r128fb_accessops);
	sc->vd.init_screen = r128fb_init_screen;

	/* init engine here */
	r128fb_init(sc);

	ri = &sc->sc_console_screen.scr_ri;

	sc->sc_gc.gc_bitblt = r128fb_bitblt;
	sc->sc_gc.gc_blitcookie = sc;
	sc->sc_gc.gc_rop = R128_ROP3_S;
	if (is_console) {
		vcons_init_screen(&sc->vd, &sc->sc_console_screen, 1,
		    &defattr);
		sc->sc_console_screen.scr_flags |= VCONS_SCREEN_IS_STATIC;

		r128fb_rectfill(sc, 0, 0, sc->sc_width, sc->sc_height,
		    ri->ri_devcmap[(defattr >> 16) & 0xff]);
		sc->sc_defaultscreen_descr.textops = &ri->ri_ops;
		sc->sc_defaultscreen_descr.capabilities = ri->ri_caps;
		sc->sc_defaultscreen_descr.nrows = ri->ri_rows;
		sc->sc_defaultscreen_descr.ncols = ri->ri_cols;
		glyphcache_init(&sc->sc_gc, sc->sc_height + 5,
				(0x800000 / sc->sc_stride) - sc->sc_height - 5,
				sc->sc_width,
				ri->ri_font->fontwidth,
				ri->ri_font->fontheight,
				defattr);
		wsdisplay_cnattach(&sc->sc_defaultscreen_descr, ri, 0, 0,
		    defattr);
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
		glyphcache_init(&sc->sc_gc, sc->sc_height + 5,
				(0x800000 / sc->sc_stride) - sc->sc_height - 5,
				sc->sc_width,
				ri->ri_font->fontwidth,
				ri->ri_font->fontheight,
				defattr);
	}

	j = 0;
	rasops_get_cmap(ri, cmap, sizeof(cmap));
	for (i = 0; i < 256; i++) {
		sc->sc_cmap_red[i] = cmap[j];
		sc->sc_cmap_green[i] = cmap[j + 1];
		sc->sc_cmap_blue[i] = cmap[j + 2];
		r128fb_putpalreg(sc, i, cmap[j], cmap[j + 1], cmap[j + 2]);
		j += 3;
	}

	/* no suspend/resume support yet */
	pmf_device_register(sc->sc_dev, NULL, NULL);

	reg = bus_space_read_4(sc->sc_memt, sc->sc_regh, R128_LVDS_GEN_CNTL);
	DPRINTF("R128_LVDS_GEN_CNTL: %08x\n", reg);
	if (reg & R128_LVDS_ON) {
		sc->sc_have_backlight = 1;
		sc->sc_bl_on = 1;
		sc->sc_bl_level = 255 -
		    ((reg & R128_LEVEL_MASK) >> R128_LEVEL_SHIFT);
		pmf_event_register(sc->sc_dev, PMFE_DISPLAY_BRIGHTNESS_UP,
		    r128fb_brightness_up, TRUE);
		pmf_event_register(sc->sc_dev, PMFE_DISPLAY_BRIGHTNESS_DOWN,
		    r128fb_brightness_down, TRUE);
		aprint_verbose("%s: LVDS output is active, enabling backlight"
			       " control\n", device_xname(self));
	} else
		sc->sc_have_backlight = 0;	

	aa.console = is_console;
	aa.scrdata = &sc->sc_screenlist;
	aa.accessops = &r128fb_accessops;
	aa.accesscookie = &sc->vd;

	config_found(sc->sc_dev, &aa, wsemuldisplaydevprint);
}

static int
r128fb_ioctl(void *v, void *vs, u_long cmd, void *data, int flag,
	struct lwp *l)
{
	struct vcons_data *vd = v;
	struct r128fb_softc *sc = vd->cookie;
	struct wsdisplay_fbinfo *wdf;
	struct vcons_screen *ms = vd->active;
	struct wsdisplay_param  *param;

	switch (cmd) {
	case WSDISPLAYIO_GTYPE:
		*(u_int *)data = WSDISPLAY_TYPE_PCIMISC;
		return 0;

	/* PCI config read/write passthrough. */
	case PCI_IOC_CFGREAD:
	case PCI_IOC_CFGWRITE:
		return pci_devioctl(sc->sc_pc, sc->sc_pcitag,
		    cmd, data, flag, l);

	case WSDISPLAYIO_GET_BUSID:
		return wsdisplayio_busid_pci(sc->sc_dev, sc->sc_pc, 
		    sc->sc_pcitag, data);

	case WSDISPLAYIO_GINFO:
		if (ms == NULL)
			return ENODEV;
		wdf = (void *)data;
		wdf->height = ms->scr_ri.ri_height;
		wdf->width = ms->scr_ri.ri_width;
		wdf->depth = ms->scr_ri.ri_depth;
		wdf->cmsize = 256;
		return 0;

	case WSDISPLAYIO_GETCMAP:
		return r128fb_getcmap(sc,
		    (struct wsdisplay_cmap *)data);

	case WSDISPLAYIO_PUTCMAP:
		return r128fb_putcmap(sc,
		    (struct wsdisplay_cmap *)data);

	case WSDISPLAYIO_LINEBYTES:
		*(u_int *)data = sc->sc_stride;
		return 0;

	case WSDISPLAYIO_SMODE: {
		int new_mode = *(int*)data;
		if (new_mode != sc->sc_mode) {
			sc->sc_mode = new_mode;
			if(new_mode == WSDISPLAYIO_MODE_EMUL) {
				r128fb_init(sc);
				r128fb_restore_palette(sc);
				glyphcache_wipe(&sc->sc_gc);
				r128fb_rectfill(sc, 0, 0, sc->sc_width,
				    sc->sc_height, ms->scr_ri.ri_devcmap[
				    (ms->scr_defattr >> 16) & 0xff]);
				vcons_redraw_screen(ms);
			}
		}
		}
		return 0;

	case WSDISPLAYIO_GETPARAM:
		param = (struct wsdisplay_param *)data;
		if (sc->sc_have_backlight == 0)
			return EPASSTHROUGH;
		switch (param->param) {
		case WSDISPLAYIO_PARAM_BRIGHTNESS:
			param->min = 0;
			param->max = 255;
			param->curval = sc->sc_bl_level;
			return 0;
		case WSDISPLAYIO_PARAM_BACKLIGHT:
			param->min = 0;
			param->max = 1;
			param->curval = sc->sc_bl_on;
			return 0;
		}
		return EPASSTHROUGH;

	case WSDISPLAYIO_SETPARAM:
		param = (struct wsdisplay_param *)data;
		if (sc->sc_have_backlight == 0)
			return EPASSTHROUGH;
		switch (param->param) {
		case WSDISPLAYIO_PARAM_BRIGHTNESS:
			r128fb_set_backlight(sc, param->curval);
			return 0;
		case WSDISPLAYIO_PARAM_BACKLIGHT:
			r128fb_switch_backlight(sc,  param->curval);
			return 0;
		}
		return EPASSTHROUGH;
	
	case WSDISPLAYIO_GET_EDID: {
		struct wsdisplayio_edid_info *d = data;
		return wsdisplayio_get_edid(sc->sc_dev, d);
	}

	case WSDISPLAYIO_GET_FBINFO: {
		struct wsdisplayio_fbinfo *fbi = data;
		return wsdisplayio_get_fbinfo(&ms->scr_ri, fbi);
	}
	}
	return EPASSTHROUGH;
}

static paddr_t
r128fb_mmap(void *v, void *vs, off_t offset, int prot)
{
	struct vcons_data *vd = v;
	struct r128fb_softc *sc = vd->cookie;
	paddr_t pa;

	/* 'regular' framebuffer mmap()ing */
	if (offset < sc->sc_fbsize) {
		pa = bus_space_mmap(sc->sc_memt, sc->sc_fb + offset, 0, prot,
		    BUS_SPACE_MAP_LINEAR);
		return pa;
	}

	/*
	 * restrict all other mappings to processes with superuser privileges
	 * or the kernel itself
	 */
	if (kauth_authorize_machdep(kauth_cred_get(), KAUTH_MACHDEP_UNMANAGEDMEM,
	    NULL, NULL, NULL, NULL) != 0) {
		aprint_normal("%s: mmap() rejected.\n",
		    device_xname(sc->sc_dev));
		return -1;
	}

	if ((offset >= sc->sc_fb) && (offset < (sc->sc_fb + sc->sc_fbsize))) {
		pa = bus_space_mmap(sc->sc_memt, offset, 0, prot,
		    BUS_SPACE_MAP_LINEAR);
		return pa;
	}

	if ((offset >= sc->sc_reg) && 
	    (offset < (sc->sc_reg + sc->sc_regsize))) {
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

#ifdef OFB_ALLOW_OTHERS
	if (offset >= 0x80000000) {
		pa = bus_space_mmap(sc->sc_memt, offset, 0, prot,
		    BUS_SPACE_MAP_LINEAR);
		return pa;
	}
#endif
	return -1;
}

static void
r128fb_init_screen(void *cookie, struct vcons_screen *scr,
    int existing, long *defattr)
{
	struct r128fb_softc *sc = cookie;
	struct rasops_info *ri = &scr->scr_ri;

	ri->ri_depth = sc->sc_depth;
	ri->ri_width = sc->sc_width;
	ri->ri_height = sc->sc_height;
	ri->ri_stride = sc->sc_stride;
	ri->ri_flg = RI_CENTER;
	if (sc->sc_depth == 8)
		ri->ri_flg |= RI_8BIT_IS_RGB | RI_ENABLE_ALPHA;

	rasops_init(ri, 0, 0);
	ri->ri_caps = WSSCREEN_WSCOLORS;
#ifdef VCONS_DRAW_INTR
	scr->scr_flags |= VCONS_DONT_READ;
#endif

	rasops_reconfig(ri, sc->sc_height / ri->ri_font->fontheight,
		    sc->sc_width / ri->ri_font->fontwidth);

	ri->ri_hw = scr;
	ri->ri_ops.copyrows = r128fb_copyrows;
	ri->ri_ops.copycols = r128fb_copycols;
	ri->ri_ops.eraserows = r128fb_eraserows;
	ri->ri_ops.erasecols = r128fb_erasecols;
	ri->ri_ops.cursor = r128fb_cursor;
	if (FONT_IS_ALPHA(ri->ri_font)) {
		ri->ri_ops.putchar = r128fb_putchar_aa;
	} else
		ri->ri_ops.putchar = r128fb_putchar;
}

static int
r128fb_putcmap(struct r128fb_softc *sc, struct wsdisplay_cmap *cm)
{
	u_char *r, *g, *b;
	u_int index = cm->index;
	u_int count = cm->count;
	int i, error;
	u_char rbuf[256], gbuf[256], bbuf[256];

#ifdef R128FB_DEBUG
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
		r128fb_putpalreg(sc, index, *r, *g, *b);
		index++;
		r++, g++, b++;
	}
	return 0;
}

static int
r128fb_getcmap(struct r128fb_softc *sc, struct wsdisplay_cmap *cm)
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
r128fb_restore_palette(struct r128fb_softc *sc)
{
	int i;

	for (i = 0; i < (1 << sc->sc_depth); i++) {
		r128fb_putpalreg(sc, i, sc->sc_cmap_red[i],
		    sc->sc_cmap_green[i], sc->sc_cmap_blue[i]);
	}
}

static int
r128fb_putpalreg(struct r128fb_softc *sc, uint8_t idx, uint8_t r, uint8_t g,
    uint8_t b)
{
	uint32_t reg;

	/* whack the DAC */
	reg = (r << 16) | (g << 8) | b;
	bus_space_write_4(sc->sc_memt, sc->sc_regh, R128_PALETTE_INDEX, idx);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, R128_PALETTE_DATA, reg);
	return 0;
}

static void
r128fb_init(struct r128fb_softc *sc)
{
	uint32_t datatype, d, reg;

	r128fb_flush_engine(sc);

	r128fb_wait(sc, 9);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, R128_CRTC_OFFSET, 0);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, R128_DEFAULT_OFFSET, 0);
	/* pitch is in units of 8 pixels */
	bus_space_write_4(sc->sc_memt, sc->sc_regh, R128_DEFAULT_PITCH,
	    sc->sc_width >> 3);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, R128_AUX_SC_CNTL, 0);
	bus_space_write_4(sc->sc_memt, sc->sc_regh,
	    R128_DEFAULT_SC_BOTTOM_RIGHT,
	    R128_DEFAULT_SC_RIGHT_MAX | R128_DEFAULT_SC_BOTTOM_MAX);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, R128_SC_TOP_LEFT, 0);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, R128_SC_BOTTOM_RIGHT,
	    R128_DEFAULT_SC_RIGHT_MAX | R128_DEFAULT_SC_BOTTOM_MAX);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, 
	    R128_DEFAULT_SC_BOTTOM_RIGHT,
	    R128_DEFAULT_SC_RIGHT_MAX | R128_DEFAULT_SC_BOTTOM_MAX);

#if 0
#if BYTE_ORDER == BIG_ENDIAN
	bus_space_write_4(sc->sc_memt, sc->sc_regh, R128_DP_DATATYPE,
	    R128_HOST_BIG_ENDIAN_EN);
#else
	bus_space_write_4(sc->sc_memt, sc->sc_regh, R128_DP_DATATYPE, 0);
#endif
#endif
	r128fb_wait(sc, 7);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, R128_SRC_PITCH,
	    sc->sc_width >> 3);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, R128_DST_PITCH,
	    sc->sc_width >> 3);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, R128_SRC_OFFSET, 0);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, R128_DST_OFFSET, 0);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, R128_DP_WRITE_MASK,
	    0xffffffff);

	switch (sc->sc_depth) {
		case 8:
			datatype = R128_GMC_DST_8BPP_CI;
			d = R128_CRTC_COLOR_8BIT;
			break;
		case 15:
			datatype = R128_GMC_DST_15BPP;
			d = R128_CRTC_COLOR_15BIT;
			break;
		case 16:
			datatype = R128_GMC_DST_16BPP;
			d = R128_CRTC_COLOR_16BIT;
			break;
		case 24:
			datatype = R128_GMC_DST_24BPP;
			d = R128_CRTC_COLOR_24BIT;
			break;
		case 32:
			datatype = R128_GMC_DST_32BPP;
			d = R128_CRTC_COLOR_32BIT;
			break;
		default:
			aprint_error("%s: unsupported depth %d\n",
			    device_xname(sc->sc_dev), sc->sc_depth);
			return;
	}
	sc->sc_master_cntl = R128_GMC_CLR_CMP_CNTL_DIS |
	    R128_GMC_AUX_CLIP_DIS | datatype;
	reg = bus_space_read_4(sc->sc_memt, sc->sc_regh, R128_CRTC_GEN_CNTL);
	DPRINTF("depth: %d\n", reg & R128_CRTC_PIX_WIDTH);
	reg &= ~R128_CRTC_PIX_WIDTH;
	reg |= d;
	bus_space_write_4(sc->sc_memt, sc->sc_regh, R128_CRTC_GEN_CNTL, reg);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, R128_CRTC_PITCH, sc->sc_width >> 3);
	r128fb_flush_engine(sc);
}

static void
r128fb_rectfill(struct r128fb_softc *sc, int x, int y, int wi, int he,
     uint32_t colour)
{

	r128fb_wait(sc, 5);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, R128_DP_GUI_MASTER_CNTL,
	    R128_GMC_BRUSH_SOLID_COLOR |
	    R128_GMC_SRC_DATATYPE_COLOR |
	    R128_ROP3_P |
	    sc->sc_master_cntl);

	bus_space_write_4(sc->sc_memt, sc->sc_regh, R128_DP_BRUSH_FRGD_CLR,
	    colour);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, R128_DP_CNTL,
	    R128_DST_X_LEFT_TO_RIGHT | R128_DST_Y_TOP_TO_BOTTOM);
	/* now feed it coordinates */
	bus_space_write_4(sc->sc_memt, sc->sc_regh, R128_DST_X_Y, 
	    (x << 16) | y);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, R128_DST_WIDTH_HEIGHT,
	    (wi << 16) | he);
}

static void
r128fb_bitblt(void *cookie, int xs, int ys, int xd, int yd,
    int wi, int he, int rop)
{
	struct r128fb_softc *sc = cookie;
	uint32_t dp_cntl = 0;

	r128fb_wait(sc, 5);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, R128_DP_GUI_MASTER_CNTL,
	    R128_GMC_BRUSH_SOLID_COLOR |
	    R128_GMC_SRC_DATATYPE_COLOR |
	    rop |
	    R128_DP_SRC_SOURCE_MEMORY |
	    sc->sc_master_cntl);

	if (yd <= ys) {
		dp_cntl = R128_DST_Y_TOP_TO_BOTTOM;
	} else {
		ys += he - 1;
		yd += he - 1;
	}
	if (xd <= xs) {
		dp_cntl |= R128_DST_X_LEFT_TO_RIGHT;
	} else {
		xs += wi - 1;
		xd += wi - 1;
	}
	bus_space_write_4(sc->sc_memt, sc->sc_regh, R128_DP_CNTL, dp_cntl);

	/* now feed it coordinates */
	bus_space_write_4(sc->sc_memt, sc->sc_regh, R128_SRC_X_Y, 
	    (xs << 16) | ys);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, R128_DST_X_Y, 
	    (xd << 16) | yd);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, R128_DST_WIDTH_HEIGHT,
	    (wi << 16) | he);
}

static void
r128fb_cursor(void *cookie, int on, int row, int col)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;
	struct r128fb_softc *sc = scr->scr_cookie;
	int x, y, wi, he;
	
	wi = ri->ri_font->fontwidth;
	he = ri->ri_font->fontheight;
	
	if (sc->sc_mode == WSDISPLAYIO_MODE_EMUL) {
		x = ri->ri_ccol * wi + ri->ri_xorigin;
		y = ri->ri_crow * he + ri->ri_yorigin;
		if (ri->ri_flg & RI_CURSOR) {
			r128fb_bitblt(sc, x, y, x, y, wi, he, R128_ROP3_Dn);
			ri->ri_flg &= ~RI_CURSOR;
		}
		ri->ri_crow = row;
		ri->ri_ccol = col;
		if (on) {
			x = ri->ri_ccol * wi + ri->ri_xorigin;
			y = ri->ri_crow * he + ri->ri_yorigin;
			r128fb_bitblt(sc, x, y, x, y, wi, he, R128_ROP3_Dn);
			ri->ri_flg |= RI_CURSOR;
		}
	} else {
		scr->scr_ri.ri_crow = row;
		scr->scr_ri.ri_ccol = col;
		scr->scr_ri.ri_flg &= ~RI_CURSOR;
	}

}

static void
r128fb_putchar(void *cookie, int row, int col, u_int c, long attr)
{
	struct rasops_info *ri = cookie;
	struct wsdisplay_font *font = PICK_FONT(ri, c);
	struct vcons_screen *scr = ri->ri_hw;
	struct r128fb_softc *sc = scr->scr_cookie;
	void *data;
	uint32_t fg, bg;
	int i, x, y, wi, he, offset;

	if (sc->sc_mode != WSDISPLAYIO_MODE_EMUL) 
		return;

	if (!CHAR_IN_FONT(c, font))
		return;

	wi = font->fontwidth;
	he = font->fontheight;

	bg = ri->ri_devcmap[(attr >> 16) & 0xf];
	fg = ri->ri_devcmap[(attr >> 24) & 0xf];
	x = ri->ri_xorigin + col * wi;
	y = ri->ri_yorigin + row * he;

	if (c == 0x20) {
		r128fb_rectfill(sc, x, y, wi, he, bg);
		return;
	}

	data = WSFONT_GLYPH(c, font);

	r128fb_wait(sc, 8);

	bus_space_write_4(sc->sc_memt, sc->sc_regh, 
	    R128_DP_GUI_MASTER_CNTL,
	    R128_GMC_BRUSH_SOLID_COLOR |
	    R128_GMC_SRC_DATATYPE_MONO_FG_BG |
	    R128_ROP3_S |
	    R128_DP_SRC_SOURCE_HOST_DATA |
	    R128_GMC_DST_CLIPPING |
	    sc->sc_master_cntl);

	bus_space_write_4(sc->sc_memt, sc->sc_regh, 
	    R128_DP_CNTL, 
	    R128_DST_Y_TOP_TO_BOTTOM | 
	    R128_DST_X_LEFT_TO_RIGHT);

	bus_space_write_4(sc->sc_memt, sc->sc_regh, 
	    R128_DP_SRC_FRGD_CLR, fg);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, 
	    R128_DP_SRC_BKGD_CLR, bg);

	/*
	 * The Rage 128 doesn't have anything to skip pixels
	 * when colour expanding but all coordinates
	 * are signed so we just clip the leading bytes and 
	 * trailing bits away
	 */
	bus_space_write_4(sc->sc_memt, sc->sc_regh, 
	    R128_SC_RIGHT, x + wi - 1);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, 
	    R128_SC_LEFT, x);

	/* needed? */
	bus_space_write_4(sc->sc_memt, sc->sc_regh, R128_SRC_X_Y, 0);

	offset = 32 - (font->stride << 3);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, R128_DST_X_Y,
	    ((x - offset) << 16) | y);
	bus_space_write_4(sc->sc_memt, sc->sc_regh,
	    R128_DST_WIDTH_HEIGHT, (32 << 16) | he);

	r128fb_wait(sc, he);
	switch (font->stride) {
		case 1: {
			uint8_t *data8 = data;
			uint32_t reg;
			for (i = 0; i < he; i++) {
				reg = *data8;
				bus_space_write_stream_4(sc->sc_memt, 
				    sc->sc_regh, R128_HOST_DATA0, reg);
				data8++;
			}
			break;
		}
		case 2: {
			uint16_t *data16 = data;
			uint32_t reg;
			for (i = 0; i < he; i++) {
				reg = *data16;
				bus_space_write_stream_4(sc->sc_memt, 
				    sc->sc_regh, R128_HOST_DATA0, reg);
				data16++;
			}
			break;
		}
	}
}

static void
r128fb_putchar_aa(void *cookie, int row, int col, u_int c, long attr)
{
	struct rasops_info *ri = cookie;
	struct wsdisplay_font *font = PICK_FONT(ri, c);
	struct vcons_screen *scr = ri->ri_hw;
	struct r128fb_softc *sc = scr->scr_cookie;
	uint32_t bg, latch = 0, bg8, fg8, pixel;
	int i, x, y, wi, he, r, g, b, aval;
	int r1, g1, b1, r0, g0, b0, fgo, bgo;
	uint8_t *data8;
	int rv, cnt = 0;

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
		r128fb_rectfill(sc, x, y, wi, he, bg);
		return;
	}

	rv = glyphcache_try(&sc->sc_gc, c, x, y, attr);
	if (rv == GC_OK)
		return;

	data8 = WSFONT_GLYPH(c, font);

	r128fb_wait(sc, 5);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, 
	    R128_DP_GUI_MASTER_CNTL,
	    R128_GMC_BRUSH_SOLID_COLOR |
	    R128_GMC_SRC_DATATYPE_COLOR |
	    R128_ROP3_S |
	    R128_DP_SRC_SOURCE_HOST_DATA |
	    sc->sc_master_cntl);

	bus_space_write_4(sc->sc_memt, sc->sc_regh, 
	    R128_DP_CNTL, 
	    R128_DST_Y_TOP_TO_BOTTOM | 
	    R128_DST_X_LEFT_TO_RIGHT);

	/* needed? */
	bus_space_write_4(sc->sc_memt, sc->sc_regh, R128_SRC_X_Y, 0);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, R128_DST_X_Y,
	    (x << 16) | y);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, R128_DST_WIDTH_HEIGHT,
	    (wi << 16) | he);

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

	r128fb_wait(sc, 16);

	for (i = 0; i < ri->ri_fontscale; i++) {
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
		latch = (latch << 8) | pixel;
		/* write in 32bit chunks */
		if ((i & 3) == 3) {
			bus_space_write_stream_4(sc->sc_memt, sc->sc_regh,
			    R128_HOST_DATA0, latch);
			/*
			 * not strictly necessary, old data should be shifted 
			 * out 
			 */
			latch = 0;
			cnt++;
			if (cnt > 15) {
				r128fb_wait(sc, 16);
				cnt = 0;
			}
		}
		data8++;
	}
	/* if we have pixels left in latch write them out */
	if ((i & 3) != 0) {
		latch = latch << ((4 - (i & 3)) << 3);	
		bus_space_write_stream_4(sc->sc_memt, sc->sc_regh,
				    R128_HOST_DATA0, latch);
	}
	if (rv == GC_ADD) {
		glyphcache_add(&sc->sc_gc, c, x, y);
	}
}

static void
r128fb_copycols(void *cookie, int row, int srccol, int dstcol, int ncols)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;
	struct r128fb_softc *sc = scr->scr_cookie;
	int32_t xs, xd, y, width, height;
	
	if ((sc->sc_locked == 0) && (sc->sc_mode == WSDISPLAYIO_MODE_EMUL)) {
		xs = ri->ri_xorigin + ri->ri_font->fontwidth * srccol;
		xd = ri->ri_xorigin + ri->ri_font->fontwidth * dstcol;
		y = ri->ri_yorigin + ri->ri_font->fontheight * row;
		width = ri->ri_font->fontwidth * ncols;
		height = ri->ri_font->fontheight;
		r128fb_bitblt(sc, xs, y, xd, y, width, height, R128_ROP3_S);
	}
}

static void
r128fb_erasecols(void *cookie, int row, int startcol, int ncols, long fillattr)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;
	struct r128fb_softc *sc = scr->scr_cookie;
	int32_t x, y, width, height, fg, bg, ul;
	
	if ((sc->sc_locked == 0) && (sc->sc_mode == WSDISPLAYIO_MODE_EMUL)) {
		x = ri->ri_xorigin + ri->ri_font->fontwidth * startcol;
		y = ri->ri_yorigin + ri->ri_font->fontheight * row;
		width = ri->ri_font->fontwidth * ncols;
		height = ri->ri_font->fontheight;
		rasops_unpack_attr(fillattr, &fg, &bg, &ul);

		r128fb_rectfill(sc, x, y, width, height, ri->ri_devcmap[bg]);
	}
}

static void
r128fb_copyrows(void *cookie, int srcrow, int dstrow, int nrows)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;
	struct r128fb_softc *sc = scr->scr_cookie;
	int32_t x, ys, yd, width, height;

	if ((sc->sc_locked == 0) && (sc->sc_mode == WSDISPLAYIO_MODE_EMUL)) {
		x = ri->ri_xorigin;
		ys = ri->ri_yorigin + ri->ri_font->fontheight * srcrow;
		yd = ri->ri_yorigin + ri->ri_font->fontheight * dstrow;
		width = ri->ri_emuwidth;
		height = ri->ri_font->fontheight * nrows;
		r128fb_bitblt(sc, x, ys, x, yd, width, height, R128_ROP3_S);
	}
}

static void
r128fb_eraserows(void *cookie, int row, int nrows, long fillattr)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;
	struct r128fb_softc *sc = scr->scr_cookie;
	int32_t x, y, width, height, fg, bg, ul;
	
	if ((sc->sc_locked == 0) && (sc->sc_mode == WSDISPLAYIO_MODE_EMUL)) {
		x = ri->ri_xorigin;
		y = ri->ri_yorigin + ri->ri_font->fontheight * row;
		width = ri->ri_emuwidth;
		height = ri->ri_font->fontheight * nrows;
		rasops_unpack_attr(fillattr, &fg, &bg, &ul);

		r128fb_rectfill(sc, x, y, width, height, ri->ri_devcmap[bg]);
	}
}

static void
r128fb_set_backlight(struct r128fb_softc *sc, int level)
{
	uint32_t reg;

	/*
	 * should we do nothing when backlight is off, should we just store the
	 * level and use it when turning back on or should we just flip sc_bl_on
	 * and turn the backlight on?
	 * For now turn it on so a crashed screensaver can't get the user stuck
	 * with a dark screen as long as hotkeys work
	 */
	if (level > 255) level = 255;
	if (level < 0) level = 0;
	if (level == sc->sc_bl_level)
		return;
	sc->sc_bl_level = level;
	if (sc->sc_bl_on == 0)
		sc->sc_bl_on = 1;
	level = 255 - level;
	reg = bus_space_read_4(sc->sc_memt, sc->sc_regh, R128_LVDS_GEN_CNTL);
	reg &= ~R128_LEVEL_MASK;
	reg |= (level << R128_LEVEL_SHIFT);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, R128_LVDS_GEN_CNTL, reg);
	DPRINTF("backlight level: %d reg %08x\n", level, reg);
}

static void
r128fb_switch_backlight(struct r128fb_softc *sc, int on)
{
	uint32_t reg;
	int level;

	if (on == sc->sc_bl_on)
		return;
	sc->sc_bl_on = on;
	reg = bus_space_read_4(sc->sc_memt, sc->sc_regh, R128_LVDS_GEN_CNTL);
	reg &= ~R128_LEVEL_MASK;
	level = on ? 255 - sc->sc_bl_level : 255;
	reg |= level << R128_LEVEL_SHIFT;
	bus_space_write_4(sc->sc_memt, sc->sc_regh, R128_LVDS_GEN_CNTL, reg);
	DPRINTF("backlight state: %d reg %08x\n", on, reg);
}
	

static void
r128fb_brightness_up(device_t dev)
{
	struct r128fb_softc *sc = device_private(dev);

	r128fb_set_backlight(sc, sc->sc_bl_level + 8);
}

static void
r128fb_brightness_down(device_t dev)
{
	struct r128fb_softc *sc = device_private(dev);

	r128fb_set_backlight(sc, sc->sc_bl_level - 8);
}
