/*	$NetBSD: pm2fb.c,v 1.28 2015/09/16 16:52:54 macallan Exp $	*/

/*
 * Copyright (c) 2009, 2012 Michael Lorenz
 * 		 2014 Naruaki Etomi
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
 * A console driver for Permedia 2 graphics controllers
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: pm2fb.c,v 1.28 2015/09/16 16:52:54 macallan Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/lwp.h>
#include <sys/kauth.h>
#include <sys/atomic.h>

#include <dev/videomode/videomode.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>
#include <dev/pci/pciio.h>
#include <dev/pci/pm2reg.h>

#include <dev/wscons/wsdisplayvar.h>
#include <dev/wscons/wsconsio.h>
#include <dev/wsfont/wsfont.h>
#include <dev/rasops/rasops.h>
#include <dev/wscons/wsdisplay_vconsvar.h>
#include <dev/wscons/wsdisplay_glyphcachevar.h>
#include <dev/pci/wsdisplay_pci.h>

#include <dev/i2c/i2cvar.h>
#include <dev/i2c/i2c_bitbang.h>
#include <dev/i2c/ddcvar.h>
#include <dev/videomode/videomode.h>
#include <dev/videomode/edidvar.h>
#include <dev/videomode/edidreg.h>

#include "opt_pm2fb.h"

#ifdef PM2FB_DEBUG
#define DPRINTF aprint_error
#else
#define DPRINTF while (0) printf
#endif

#if BYTE_ORDER == LITTLE_ENDIAN
/*
 * XXX
 * A temporary workaround for unaligned blits on little endian hardware.
 * This makes pm2fb_bitblt() work well on little endian hardware to get
 * scrolling right, but not much more. Unaligned blits ( as in, where the lower
 * 2 bits of the source and destination X coordinates don't match ) are still
 * wrong so the glyph cache is also disabled.  
 */
#define BITBLT_LE_WORKAROUND
#endif

struct pm2fb_softc {
	device_t sc_dev;

	pci_chipset_tag_t sc_pc;
	pcitag_t sc_pcitag;

	bus_space_tag_t sc_memt;
	bus_space_tag_t sc_iot;

	bus_space_handle_t sc_regh;
	bus_addr_t sc_fb, sc_reg;
	bus_size_t sc_fbsize, sc_regsize;

	int sc_width, sc_height, sc_depth, sc_stride;
	int sc_locked;
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
	uint32_t sc_pprod;
	int sc_is_pm2;
	/* i2c stuff */
	struct i2c_controller sc_i2c;
	uint8_t sc_edid_data[128];
	struct edid_info sc_ei;
	const struct videomode *sc_videomode;
	glyphcache sc_gc;
};

static int	pm2fb_match(device_t, cfdata_t, void *);
static void	pm2fb_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(pm2fb, sizeof(struct pm2fb_softc),
    pm2fb_match, pm2fb_attach, NULL, NULL);

extern const u_char rasops_cmap[768];

static int	pm2fb_ioctl(void *, void *, u_long, void *, int,
			     struct lwp *);
static paddr_t	pm2fb_mmap(void *, void *, off_t, int);
static void	pm2fb_init_screen(void *, struct vcons_screen *, int, long *);

static int	pm2fb_putcmap(struct pm2fb_softc *, struct wsdisplay_cmap *);
static int 	pm2fb_getcmap(struct pm2fb_softc *, struct wsdisplay_cmap *);
static void	pm2fb_init_palette(struct pm2fb_softc *);
static int 	pm2fb_putpalreg(struct pm2fb_softc *, uint8_t, uint8_t,
			    uint8_t, uint8_t);

static void	pm2fb_init(struct pm2fb_softc *);
static inline void pm2fb_wait(struct pm2fb_softc *, int);
static void	pm2fb_flush_engine(struct pm2fb_softc *);
static void	pm2fb_rectfill(struct pm2fb_softc *, int, int, int, int,
			    uint32_t);
static void	pm2fb_rectfill_a(void *, int, int, int, int, long);
static void	pm2fb_bitblt(void *, int, int, int, int, int, int, int);

static void	pm2fb_cursor(void *, int, int, int);
static void	pm2fb_putchar(void *, int, int, u_int, long);
static void	pm2fb_putchar_aa(void *, int, int, u_int, long);
static void	pm2fb_copycols(void *, int, int, int, int);
static void	pm2fb_erasecols(void *, int, int, int, long);
static void	pm2fb_copyrows(void *, int, int, int);
static void	pm2fb_eraserows(void *, int, int, long);

struct wsdisplay_accessops pm2fb_accessops = {
	pm2fb_ioctl,
	pm2fb_mmap,
	NULL,	/* alloc_screen */
	NULL,	/* free_screen */
	NULL,	/* show_screen */
	NULL, 	/* load_font */
	NULL,	/* pollc */
	NULL	/* scroll */
};

/* I2C glue */
static int pm2fb_i2c_acquire_bus(void *, int);
static void pm2fb_i2c_release_bus(void *, int);
static int pm2fb_i2c_send_start(void *, int);
static int pm2fb_i2c_send_stop(void *, int);
static int pm2fb_i2c_initiate_xfer(void *, i2c_addr_t, int);
static int pm2fb_i2c_read_byte(void *, uint8_t *, int);
static int pm2fb_i2c_write_byte(void *, uint8_t, int);

/* I2C bitbang glue */
static void pm2fb_i2cbb_set_bits(void *, uint32_t);
static void pm2fb_i2cbb_set_dir(void *, uint32_t);
static uint32_t pm2fb_i2cbb_read(void *);

static void pm2_setup_i2c(struct pm2fb_softc *);

static const struct i2c_bitbang_ops pm2fb_i2cbb_ops = {
	pm2fb_i2cbb_set_bits,
	pm2fb_i2cbb_set_dir,
	pm2fb_i2cbb_read,
	{
		PM2_DD_SDA_IN,
		PM2_DD_SCL_IN,
		0,
		0
	}
};

/* mode setting stuff */
static int pm2fb_set_pll(struct pm2fb_softc *, int);
static int pm2vfb_set_pll(struct pm2fb_softc *, int);
static uint8_t pm2fb_read_dac(struct pm2fb_softc *, int);
static void pm2fb_write_dac(struct pm2fb_softc *, int, uint8_t);
static void pm2fb_set_mode(struct pm2fb_softc *, const struct videomode *);

const struct {
	int vendor;
	int product;
	int flags;
} pm2fb_pci_devices[] = {
	{
		PCI_VENDOR_3DLABS,
		PCI_PRODUCT_3DLABS_PERMEDIA2V,
		0
	},
	{
		PCI_VENDOR_TI,
		PCI_PRODUCT_TI_TVP4020,
		1 	
	},
	{
		0,
		0,
		0
	}
};

/* this table is from xf86-video-glint */
#define PARTPROD(a,b,c) (((a)<<6) | ((b)<<3) | (c))
int partprodPermedia[] = {
	-1,
	PARTPROD(0,0,1), PARTPROD(0,1,1), PARTPROD(1,1,1), PARTPROD(1,1,2),
	PARTPROD(1,2,2), PARTPROD(2,2,2), PARTPROD(1,2,3), PARTPROD(2,2,3),
	PARTPROD(1,3,3), PARTPROD(2,3,3), PARTPROD(1,2,4), PARTPROD(3,3,3),
	PARTPROD(1,3,4), PARTPROD(2,3,4),              -1, PARTPROD(3,3,4), 
	PARTPROD(1,4,4), PARTPROD(2,4,4),              -1, PARTPROD(3,4,4), 
	             -1, PARTPROD(2,3,5),              -1, PARTPROD(4,4,4), 
	PARTPROD(1,4,5), PARTPROD(2,4,5), PARTPROD(3,4,5),              -1,
	             -1,              -1,              -1, PARTPROD(4,4,5), 
	PARTPROD(1,5,5), PARTPROD(2,5,5),              -1, PARTPROD(3,5,5), 
	             -1,              -1,              -1, PARTPROD(4,5,5), 
	             -1,              -1,              -1, PARTPROD(3,4,6),
	             -1,              -1,              -1, PARTPROD(5,5,5), 
	PARTPROD(1,5,6), PARTPROD(2,5,6),              -1, PARTPROD(3,5,6),
	             -1,              -1,              -1, PARTPROD(4,5,6),
	             -1,              -1,              -1,              -1,
	             -1,              -1,              -1, PARTPROD(5,5,6),
	             -1,              -1,              -1,              -1,
	             -1,              -1,              -1,              -1,
	             -1,              -1,              -1,              -1,
	             -1,              -1,              -1,              -1,
	             -1,              -1,              -1,              -1,
	             -1,              -1,              -1,              -1,
	             -1,              -1,              -1,              -1,
	             -1,              -1,              -1,              -1,
	             -1,              -1,              -1,              -1,
	             -1,              -1,              -1,              -1,
	             -1,              -1,              -1,              -1,
	             -1,              -1,              -1,              -1,
	             -1,              -1,              -1,              -1,
	             -1,              -1,              -1,              -1,
	             -1,              -1,              -1,              -1,
	             -1,              -1,              -1,              -1,
		     0};

static inline void
pm2fb_wait(struct pm2fb_softc *sc, int slots)
{
	uint32_t reg;

	do {
		reg = bus_space_read_4(sc->sc_memt, sc->sc_regh, 
			PM2_INPUT_FIFO_SPACE);
	} while (reg <= slots);
}

static void
pm2fb_flush_engine(struct pm2fb_softc *sc)
{

	pm2fb_wait(sc, 2);

	bus_space_write_4(sc->sc_memt, sc->sc_regh, PM2_RE_FILTER_MODE,
	    PM2FLT_PASS_SYNC);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, PM2_RE_SYNC, 0);
	do {
		while (bus_space_read_4(sc->sc_memt, sc->sc_regh, 
			PM2_OUTPUT_FIFO_WORDS) == 0);
	} while (bus_space_read_4(sc->sc_memt, sc->sc_regh, PM2_OUTPUT_FIFO) != 
	    PM2_SYNC_TAG);
}

static int
pm2fb_match(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa = (struct pci_attach_args *)aux;
	int i;

	if (PCI_CLASS(pa->pa_class) != PCI_CLASS_DISPLAY)
		return 0;

	for (i = 0; pm2fb_pci_devices[i].vendor; i++) {
		if ((PCI_VENDOR(pa->pa_id) == pm2fb_pci_devices[i].vendor &&
		     PCI_PRODUCT(pa->pa_id) == pm2fb_pci_devices[i].product)) 
			return 100;
	}

	return (0);
}

static void
pm2fb_attach(device_t parent, device_t self, void *aux)
{
	struct pm2fb_softc	*sc = device_private(self);
	struct pci_attach_args	*pa = aux;
	struct rasops_info	*ri;
	struct wsemuldisplaydev_attach_args aa;
	prop_dictionary_t	dict;
	unsigned long		defattr;
	bool			is_console = FALSE;
	uint32_t		flags;
	int			i;

	sc->sc_pc = pa->pa_pc;
	sc->sc_pcitag = pa->pa_tag;
	sc->sc_memt = pa->pa_memt;
	sc->sc_iot = pa->pa_iot;
	sc->sc_dev = self;

	for (i = 0; pm2fb_pci_devices[i].vendor; i++) {
		if (PCI_PRODUCT(pa->pa_id) == pm2fb_pci_devices[i].product) {
			sc->sc_is_pm2 = pm2fb_pci_devices[i].flags ;
			break;
		}
	}

	pci_aprint_devinfo(pa, NULL);

	/*
	 * fill in parameters from properties
	 * if we can't get a usable mode via DDC2 we'll use this to pick one,
	 * which is why we fill them in with some conservative values that 
	 * hopefully work as a last resort
	 */
	dict = device_properties(self);
	if (!prop_dictionary_get_uint32(dict, "width", &sc->sc_width)) {
		aprint_error("%s: no width property\n", device_xname(self));
		sc->sc_width = 1024;
	}
	if (!prop_dictionary_get_uint32(dict, "height", &sc->sc_height)) {
		aprint_error("%s: no height property\n", device_xname(self));
		sc->sc_height = 768;
	}
	if (!prop_dictionary_get_uint32(dict, "depth", &sc->sc_depth)) {
		aprint_error("%s: no depth property\n", device_xname(self));
		sc->sc_depth = 8;
	}

	/*
	 * don't look at the linebytes property - The Raptor firmware lies
	 * about it. Get it from width * depth >> 3 instead.
	 */

	sc->sc_stride = sc->sc_width * (sc->sc_depth >> 3);

	prop_dictionary_get_bool(dict, "is_console", &is_console);

	pci_mapreg_info(pa->pa_pc, pa->pa_tag, 0x14, PCI_MAPREG_TYPE_MEM,
	    &sc->sc_fb, &sc->sc_fbsize, &flags);

	if (pci_mapreg_map(pa, 0x10, PCI_MAPREG_TYPE_MEM, 0,
	    &sc->sc_memt, &sc->sc_regh, &sc->sc_reg, &sc->sc_regsize)) {
		aprint_error("%s: failed to map registers.\n",
		    device_xname(sc->sc_dev));
	}

	/*
	 * XXX yeah, casting the fb address to uint32_t is formally wrong
	 * but as far as I know there are no PM2 with 64bit BARs
	 */
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

	pm2_setup_i2c(sc);

	vcons_init(&sc->vd, sc, &sc->sc_defaultscreen_descr,
	    &pm2fb_accessops);
	sc->vd.init_screen = pm2fb_init_screen;

	/* init engine here */
	pm2fb_init(sc);

	ri = &sc->sc_console_screen.scr_ri;

	sc->sc_gc.gc_bitblt = pm2fb_bitblt;
	sc->sc_gc.gc_rectfill = pm2fb_rectfill_a;
	sc->sc_gc.gc_blitcookie = sc;
	sc->sc_gc.gc_rop = 3;

#ifdef PM2FB_DEBUG
	/*
	 * leave some room at the bottom of the screen for various blitter
	 * tests and in order to make the glyph cache visible
	 */
	sc->sc_height -= 200;
#endif

	if (is_console) {
		vcons_init_screen(&sc->vd, &sc->sc_console_screen, 1,
		    &defattr);
		sc->sc_console_screen.scr_flags |= VCONS_SCREEN_IS_STATIC;

		pm2fb_rectfill(sc, 0, 0, sc->sc_width, sc->sc_height,
		    ri->ri_devcmap[(defattr >> 16) & 0xff]);
		sc->sc_defaultscreen_descr.textops = &ri->ri_ops;
		sc->sc_defaultscreen_descr.capabilities = ri->ri_caps;
		sc->sc_defaultscreen_descr.nrows = ri->ri_rows;
		sc->sc_defaultscreen_descr.ncols = ri->ri_cols;

		glyphcache_init(&sc->sc_gc, sc->sc_height + 5,
			min(2047, (sc->sc_fbsize / sc->sc_stride))
			 - sc->sc_height - 5,
			sc->sc_width,
			ri->ri_font->fontwidth,
			ri->ri_font->fontheight,
			defattr);
		wsdisplay_cnattach(&sc->sc_defaultscreen_descr, ri, 0, 0,
		    defattr);
		vcons_replay_msgbuf(&sc->sc_console_screen);
	} else {
		if (sc->sc_console_screen.scr_ri.ri_rows == 0) {
			/* do some minimal setup to avoid weirdnesses later */
			vcons_init_screen(&sc->vd, &sc->sc_console_screen, 1, 
			   &defattr);
		} else
			(*ri->ri_ops.allocattr)(ri, 0, 0, 0, &defattr);
		glyphcache_init(&sc->sc_gc, sc->sc_height + 5,
			   min(2047, (sc->sc_fbsize / sc->sc_stride))
			    - sc->sc_height - 5,
			   sc->sc_width,
			   ri->ri_font->fontwidth,
			   ri->ri_font->fontheight,
			   defattr);
	}

	pm2fb_init_palette(sc);
	
	aa.console = is_console;
	aa.scrdata = &sc->sc_screenlist;
	aa.accessops = &pm2fb_accessops;
	aa.accesscookie = &sc->vd;

	config_found(sc->sc_dev, &aa, wsemuldisplaydevprint);

#ifdef PM2FB_DEBUG
	/*
	 * draw a pattern to check if pm2fb_bitblt() gets the alignment stuff
	 * right
	 */
	pm2fb_rectfill(sc, 0, sc->sc_height, sc->sc_width, 200, 0xffffffff);
	pm2fb_rectfill(sc, 0, sc->sc_height, 300, 10, 0);
	pm2fb_rectfill(sc, 10, sc->sc_height, 200, 10, 0xe0e0e0e0);
	for (i = 1; i < 20; i++) {
		pm2fb_bitblt(sc, 0, sc->sc_height, 
			i, sc->sc_height + 10 * i,
			300, 10, 3);
		pm2fb_bitblt(sc, i, sc->sc_height, 
			400, sc->sc_height + 10 * i,
			300, 10, 3);
	}
#endif
}

static int
pm2fb_ioctl(void *v, void *vs, u_long cmd, void *data, int flag,
	struct lwp *l)
{
	struct vcons_data *vd = v;
	struct pm2fb_softc *sc = vd->cookie;
	struct wsdisplay_fbinfo *wdf;
	struct vcons_screen *ms = vd->active;

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
		return pm2fb_getcmap(sc,
		    (struct wsdisplay_cmap *)data);

	case WSDISPLAYIO_PUTCMAP:
		return pm2fb_putcmap(sc,
		    (struct wsdisplay_cmap *)data);

	case WSDISPLAYIO_LINEBYTES:
		*(u_int *)data = sc->sc_stride;
		return 0;

	case WSDISPLAYIO_SMODE: {
		int new_mode = *(int*)data;
		if (new_mode != sc->sc_mode) {
			sc->sc_mode = new_mode;
			if(new_mode == WSDISPLAYIO_MODE_EMUL) {
				/* first set the video mode */
				if (sc->sc_videomode != NULL) {
					pm2fb_set_mode(sc, sc->sc_videomode);
				}
				/* then initialize the drawing engine */
				pm2fb_init(sc);
				pm2fb_init_palette(sc);
				/* clean out the glyph cache */
				glyphcache_wipe(&sc->sc_gc);
				/* and redraw everything */
				vcons_redraw_screen(ms);
			} else
				pm2fb_flush_engine(sc);
		}
		}
		return 0;
	case WSDISPLAYIO_GET_EDID: {
		struct wsdisplayio_edid_info *d = data;
		d->data_size = 128;
		if (d->buffer_size < 128)
			return EAGAIN;
		return copyout(sc->sc_edid_data, d->edid_data, 128);
	}

	case WSDISPLAYIO_GET_FBINFO: {
		struct wsdisplayio_fbinfo *fbi = data;
		return wsdisplayio_get_fbinfo(&ms->scr_ri, fbi);
	}
	}
	return EPASSTHROUGH;
}

static paddr_t
pm2fb_mmap(void *v, void *vs, off_t offset, int prot)
{
	struct vcons_data *vd = v;
	struct pm2fb_softc *sc = vd->cookie;
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
	if (kauth_authorize_machdep(kauth_cred_get(), 
	    KAUTH_MACHDEP_UNMANAGEDMEM,
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

	return -1;
}

static void
pm2fb_init_screen(void *cookie, struct vcons_screen *scr,
    int existing, long *defattr)
{
	struct pm2fb_softc *sc = cookie;
	struct rasops_info *ri = &scr->scr_ri;

	ri->ri_depth = sc->sc_depth;
	ri->ri_width = sc->sc_width;
	ri->ri_height = sc->sc_height;
	ri->ri_stride = sc->sc_stride;
	ri->ri_flg = RI_CENTER;
	if (sc->sc_depth == 8)
		ri->ri_flg |= RI_8BIT_IS_RGB | RI_ENABLE_ALPHA;

	rasops_init(ri, 0, 0);
	ri->ri_caps = WSSCREEN_WSCOLORS | WSSCREEN_UNDERLINE;

	rasops_reconfig(ri, sc->sc_height / ri->ri_font->fontheight,
		    sc->sc_width / ri->ri_font->fontwidth);

	ri->ri_hw = scr;
	ri->ri_ops.copyrows = pm2fb_copyrows;
	ri->ri_ops.copycols = pm2fb_copycols;
	ri->ri_ops.cursor = pm2fb_cursor;
	ri->ri_ops.eraserows = pm2fb_eraserows;
	ri->ri_ops.erasecols = pm2fb_erasecols;
	if (FONT_IS_ALPHA(ri->ri_font)) {
		ri->ri_ops.putchar = pm2fb_putchar_aa;
	} else
		ri->ri_ops.putchar = pm2fb_putchar;
}

static int
pm2fb_putcmap(struct pm2fb_softc *sc, struct wsdisplay_cmap *cm)
{
	u_char *r, *g, *b;
	u_int index = cm->index;
	u_int count = cm->count;
	int i, error;
	u_char rbuf[256], gbuf[256], bbuf[256];

#ifdef PM2FB_DEBUG
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
		pm2fb_putpalreg(sc, index, *r, *g, *b);
		index++;
		r++, g++, b++;
	}
	return 0;
}

static int
pm2fb_getcmap(struct pm2fb_softc *sc, struct wsdisplay_cmap *cm)
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
pm2fb_init_palette(struct pm2fb_softc *sc)
{
	struct rasops_info *ri = &sc->sc_console_screen.scr_ri;
	int i, j = 0;
	uint8_t cmap[768];

	rasops_get_cmap(ri, cmap, sizeof(cmap));
	for (i = 0; i < 256; i++) {
		sc->sc_cmap_red[i] = cmap[j];
		sc->sc_cmap_green[i] = cmap[j + 1];
		sc->sc_cmap_blue[i] = cmap[j + 2];
		pm2fb_putpalreg(sc, i, cmap[j], cmap[j + 1], cmap[j + 2]);
		j += 3;
	}
}

static int
pm2fb_putpalreg(struct pm2fb_softc *sc, uint8_t idx, uint8_t r, uint8_t g,
    uint8_t b)
{
	bus_space_write_1(sc->sc_memt, sc->sc_regh, PM2_DAC_PAL_WRITE_IDX, idx);
	bus_space_write_1(sc->sc_memt, sc->sc_regh, PM2_DAC_DATA, r);
	bus_space_write_1(sc->sc_memt, sc->sc_regh, PM2_DAC_DATA, g);
	bus_space_write_1(sc->sc_memt, sc->sc_regh, PM2_DAC_DATA, b);
	return 0;
}

static uint8_t
pm2fb_read_dac(struct pm2fb_softc *sc, int reg)
{
	if (sc->sc_is_pm2) {
		bus_space_write_1(sc->sc_memt, sc->sc_regh,
		    PM2_DAC_PAL_WRITE_IDX, reg);
		return bus_space_read_1(sc->sc_memt, sc->sc_regh,
		    PM2_DAC_INDEX_DATA);
	} else {
		bus_space_write_1(sc->sc_memt, sc->sc_regh,
		    PM2V_DAC_INDEX_LOW, reg & 0xff);
		bus_space_write_1(sc->sc_memt, sc->sc_regh,
		    PM2V_DAC_INDEX_HIGH, (reg >> 8) & 0xff);
		return bus_space_read_1(sc->sc_memt, sc->sc_regh,
		    PM2V_DAC_INDEX_DATA);
	}	
}

static void
pm2fb_write_dac(struct pm2fb_softc *sc, int reg, uint8_t data)
{
	pm2fb_wait(sc, 3);
	if (sc->sc_is_pm2) {
		pm2fb_wait(sc, 2);
		bus_space_write_1(sc->sc_memt, sc->sc_regh,
		    PM2_DAC_PAL_WRITE_IDX, reg);
		bus_space_write_1(sc->sc_memt, sc->sc_regh,
		    PM2_DAC_INDEX_DATA, data);
	} else {
		pm2fb_wait(sc, 3);
		bus_space_write_1(sc->sc_memt, sc->sc_regh,
		    PM2V_DAC_INDEX_LOW, reg & 0xff);
		bus_space_write_1(sc->sc_memt, sc->sc_regh,
		    PM2V_DAC_INDEX_HIGH, (reg >> 8) & 0xff);
		bus_space_write_1(sc->sc_memt, sc->sc_regh,
		    PM2V_DAC_INDEX_DATA, data);
	}	
}

static void
pm2fb_init(struct pm2fb_softc *sc)
{
	pm2fb_flush_engine(sc);

	pm2fb_wait(sc, 9);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, PM2_SCREEN_BASE, 0);
	/* set aperture endianness */
#if BYTE_ORDER == BIG_ENDIAN
	bus_space_write_4(sc->sc_memt, sc->sc_regh, PM2_APERTURE1_CONTROL,
		PM2_AP_BYTESWAP | PM2_AP_HALFWORDSWAP);	
#else
	bus_space_write_4(sc->sc_memt, sc->sc_regh, PM2_APERTURE1_CONTROL, 0);
#endif	
#if 0
	bus_space_write_4(sc->sc_memt, sc->sc_regh, PM2_BYPASS_MASK, 
		0xffffffff);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, PM2_FB_WRITE_MASK, 
		0xffffffff);
#endif
	bus_space_write_4(sc->sc_memt, sc->sc_regh, PM2_HW_WRITEMASK, 
		0xffffffff);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, PM2_SW_WRITEMASK, 
		0xffffffff);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, PM2_RE_WRITE_MODE, 
		PM2WM_WRITE_EN);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, PM2_RE_SCREENSIZE,
	    (sc->sc_height << 16) | sc->sc_width);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, PM2_RE_SCISSOR_MODE, 
	    PM2SC_SCREEN_EN);
	pm2fb_wait(sc, 8);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, PM2_RE_DITHER_MODE, 0);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, PM2_RE_ALPHA_MODE, 0);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, PM2_RE_DDA_MODE, 0);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, PM2_RE_TEX_COLOUR_MODE, 0);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, PM2_RE_TEX_ADDRESS_MODE, 0);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, PM2_RE_TEX_READ_MODE, 0);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, PM2_RE_TEX_LUT_MODE, 0);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, PM2_RE_YUV_MODE, 0);
	pm2fb_wait(sc, 8);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, PM2_RE_DEPTH_MODE, 0);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, PM2_RE_DEPTH, 0);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, PM2_RE_STENCIL_MODE, 0);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, PM2_RE_STIPPLE_MODE, 0);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, PM2_RE_ROP_MODE, 0);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, PM2_RE_WINDOW_ORIGIN, 0);
#if 0
	sc->sc_pprod = bus_space_read_4(sc->sc_memt, sc->sc_regh, 
	    PM2_FB_READMODE) &
	    (PM2FB_PP0_MASK | PM2FB_PP1_MASK | PM2FB_PP2_MASK);
#endif
	sc->sc_pprod = partprodPermedia[sc->sc_stride >> 5];

	bus_space_write_4(sc->sc_memt, sc->sc_regh, PM2_FB_READMODE, 
	    sc->sc_pprod);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, PM2_RE_TEXMAP_FORMAT, 
	    sc->sc_pprod);
	pm2fb_wait(sc, 9);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, PM2_RE_DY, 1 << 16);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, PM2_RE_DXDOM, 0);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, PM2_RE_STARTXDOM, 0);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, PM2_RE_STARTXSUB, 0);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, PM2_RE_STARTY, 0);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, PM2_RE_COUNT, 0);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, PM2_RE_SCISSOR_MINYX, 0);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, PM2_RE_SCISSOR_MAXYX,
	    0x0fff0fff);
	/*
	 * another scissor we need to disable in order to blit into off-screen
	 * memory
	 */
	bus_space_write_4(sc->sc_memt, sc->sc_regh, PM2_RE_SCREENSIZE,
	    0x0fff0fff);

	switch(sc->sc_depth) {
		case 8:
			bus_space_write_4(sc->sc_memt, sc->sc_regh,
			    PM2_RE_PIXEL_SIZE, PM2PS_8BIT);
			break;
		case 16:
			bus_space_write_4(sc->sc_memt, sc->sc_regh,
			    PM2_RE_PIXEL_SIZE, PM2PS_16BIT);
			break;
		case 32:
			bus_space_write_4(sc->sc_memt, sc->sc_regh,
			    PM2_RE_PIXEL_SIZE, PM2PS_32BIT);
			break;
	}
	pm2fb_flush_engine(sc);
	DPRINTF("pixel size: %08x\n", 
	    bus_space_read_4(sc->sc_memt, sc->sc_regh, PM2_RE_PIXEL_SIZE));
}

static void
pm2fb_rectfill(struct pm2fb_softc *sc, int x, int y, int wi, int he,
     uint32_t colour)
{

	pm2fb_wait(sc, 7);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, PM2_RE_DDA_MODE, 0);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, PM2_RE_MODE, 0);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, PM2_RE_CONFIG,
	    PM2RECFG_WRITE_EN);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, PM2_RE_BLOCK_COLOUR,
	    colour);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, PM2_RE_RECT_START,
	    (y << 16) | x);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, PM2_RE_RECT_SIZE,
	    (he << 16) | wi);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, PM2_RE_RENDER,
	    PM2RE_RECTANGLE | PM2RE_INC_X | PM2RE_INC_Y | PM2RE_FASTFILL);
}

static void
pm2fb_rectfill_a(void *cookie, int x, int y, int wi, int he, long attr)
{
	struct pm2fb_softc *sc = cookie;

	pm2fb_rectfill(sc, x, y, wi, he,
	    sc->vd.active->scr_ri.ri_devcmap[(attr >> 24 & 0xf)]);
}

static void
pm2fb_bitblt(void *cookie, int xs, int ys, int xd, int yd,
    int wi, int he, int rop)
{
	struct pm2fb_softc *sc = cookie;
	uint32_t dir = 0;
	int rxd, rwi, rxdelta;

	if (yd <= ys) {
		dir |= PM2RE_INC_Y;
	}
	if (xd <= xs) {
		dir |= PM2RE_INC_X;
	}
	pm2fb_wait(sc, 8);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, PM2_RE_DDA_MODE, 0);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, PM2_RE_MODE, 0);
	if (sc->sc_depth == 8) {
		int adjust;
		/*
		 * use packed mode for some extra speed
		 * this copies 32bit quantities even in 8 bit mode, so we need
		 * to adjust for cases where the lower two bits in source and
		 * destination X don't align, and/or where the width isn't a
		 * multiple of 4
		 */
		if (rop == 3) {
			bus_space_write_4(sc->sc_memt, sc->sc_regh,
			    PM2_RE_CONFIG,
			    PM2RECFG_READ_SRC | PM2RECFG_WRITE_EN |
			    PM2RECFG_ROP_EN | PM2RECFG_PACKED | (rop << 6));
		} else {
			bus_space_write_4(sc->sc_memt, sc->sc_regh,
			    PM2_RE_CONFIG,
			    PM2RECFG_READ_SRC | PM2RECFG_READ_DST |
			    PM2RECFG_WRITE_EN | PM2RECFG_PACKED |
			    PM2RECFG_ROP_EN | (rop << 6));
		}
		rxd = xd >> 2;
		rwi = (wi + 7) >> 2;
		rxdelta = (xs & 0xffc) - (xd & 0xffc);
		/* adjust for non-aligned x */
#ifdef BITBLT_LE_WORKAROUND
		/* I have no idea why this seems to work */
		adjust = 1;
#else
		adjust = ((xd & 3) - (xs & 3));
#endif
		bus_space_write_4(sc->sc_memt, sc->sc_regh,
		    PM2_RE_PACKEDDATA_LIMIT,
		    (xd << 16) | (xd + wi) | (adjust << 29));
		
	} else {
		/* we're in 16 or 32bit mode */
		if (rop == 3) {
			bus_space_write_4(sc->sc_memt, sc->sc_regh,
			    PM2_RE_CONFIG,
			    PM2RECFG_READ_SRC | PM2RECFG_WRITE_EN |
			    PM2RECFG_ROP_EN | PM2RECFG_PACKED | (rop << 6));
		} else {
			bus_space_write_4(sc->sc_memt, sc->sc_regh,
			    PM2_RE_CONFIG,
			    PM2RECFG_READ_SRC | PM2RECFG_READ_DST |
			    PM2RECFG_WRITE_EN | PM2RECFG_PACKED |
			    PM2RECFG_ROP_EN | (rop << 6));
		}
		rxd = xd;
		rwi = wi;
		rxdelta = xs - xd;
	}		
	bus_space_write_4(sc->sc_memt, sc->sc_regh, PM2_RE_RECT_START,
	    (yd << 16) | rxd);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, PM2_RE_RECT_SIZE,
	    (he << 16) | rwi);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, PM2_RE_SOURCE_DELTA,
	    (((ys - yd) & 0xfff) << 16) | (rxdelta & 0xfff));
	bus_space_write_4(sc->sc_memt, sc->sc_regh, PM2_RE_RENDER,
	    PM2RE_RECTANGLE | dir);
}

static void
pm2fb_cursor(void *cookie, int on, int row, int col)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;
	struct pm2fb_softc *sc = scr->scr_cookie;
	int x, y, wi, he;
	
	wi = ri->ri_font->fontwidth;
	he = ri->ri_font->fontheight;
	
	if (sc->sc_mode == WSDISPLAYIO_MODE_EMUL) {
		x = ri->ri_ccol * wi + ri->ri_xorigin;
		y = ri->ri_crow * he + ri->ri_yorigin;
		if (ri->ri_flg & RI_CURSOR) {
			pm2fb_bitblt(sc, x, y, x, y, wi, he, 12);
			ri->ri_flg &= ~RI_CURSOR;
		}
		ri->ri_crow = row;
		ri->ri_ccol = col;
		if (on) {
			x = ri->ri_ccol * wi + ri->ri_xorigin;
			y = ri->ri_crow * he + ri->ri_yorigin;
			pm2fb_bitblt(sc, x, y, x, y, wi, he, 12);
			ri->ri_flg |= RI_CURSOR;
		}
	} else {
		scr->scr_ri.ri_crow = row;
		scr->scr_ri.ri_ccol = col;
		scr->scr_ri.ri_flg &= ~RI_CURSOR;
	}

}

static void
pm2fb_putchar(void *cookie, int row, int col, u_int c, long attr)
{
	struct rasops_info *ri = cookie;
	struct wsdisplay_font *font = PICK_FONT(ri, c);
	struct vcons_screen *scr = ri->ri_hw;
	struct pm2fb_softc *sc = scr->scr_cookie;
	uint32_t mode;

	if (sc->sc_mode == WSDISPLAYIO_MODE_EMUL) {
		void *data;
		uint32_t fg, bg;
		int uc, i;
		int x, y, wi, he;

		wi = font->fontwidth;
		he = font->fontheight;

		if (!CHAR_IN_FONT(c, font))
			return;
		bg = ri->ri_devcmap[(attr >> 16) & 0xf];
		fg = ri->ri_devcmap[(attr >> 24) & 0xf];
		x = ri->ri_xorigin + col * wi;
		y = ri->ri_yorigin + row * he;
		if (c == 0x20) {
			pm2fb_rectfill(sc, x, y, wi, he, bg);
		} else {
			uc = c - font->firstchar;
			data = (uint8_t *)font->data + uc * ri->ri_fontscale;

			mode = PM2RM_MASK_MIRROR;
#if BYTE_ORDER == LITTLE_ENDIAN
			switch (ri->ri_font->stride) {
				case 1:
					mode |= 4 << 7;
					break;
				case 2:
					mode |= 3 << 7;
					break;
			}
#else
			switch (ri->ri_font->stride) {
				case 1:
					mode |= 3 << 7;
					break;
				case 2:
					mode |= 2 << 7;
					break;
			}
#endif
			pm2fb_wait(sc, 8);

			bus_space_write_4(sc->sc_memt, sc->sc_regh,
			    PM2_RE_MODE, mode);
			bus_space_write_4(sc->sc_memt, sc->sc_regh, 
			    PM2_RE_CONFIG, PM2RECFG_WRITE_EN);
			bus_space_write_4(sc->sc_memt, sc->sc_regh, 
			    PM2_RE_BLOCK_COLOUR, bg);
			bus_space_write_4(sc->sc_memt, sc->sc_regh, 
			    PM2_RE_RECT_START, (y << 16) | x);
			bus_space_write_4(sc->sc_memt, sc->sc_regh, 
			    PM2_RE_RECT_SIZE, (he << 16) | wi);
			bus_space_write_4(sc->sc_memt, sc->sc_regh, 
			    PM2_RE_RENDER,
			    PM2RE_RECTANGLE |
			    PM2RE_INC_X | PM2RE_INC_Y | PM2RE_FASTFILL);
			bus_space_write_4(sc->sc_memt, sc->sc_regh, 
			    PM2_RE_BLOCK_COLOUR, fg);
			bus_space_write_4(sc->sc_memt, sc->sc_regh, 
			    PM2_RE_RENDER,
			    PM2RE_RECTANGLE | PM2RE_SYNC_ON_MASK |
			    PM2RE_INC_X | PM2RE_INC_Y | PM2RE_FASTFILL);

			pm2fb_wait(sc, he);
			switch (ri->ri_font->stride) {
			case 1: {
				uint8_t *data8 = data;
				uint32_t reg;
				for (i = 0; i < he; i++) {
					reg = *data8;
					bus_space_write_4(sc->sc_memt, 
					    sc->sc_regh,
					    PM2_RE_BITMASK, reg);
					data8++;
				}
				break;
				}
			case 2: {
				uint16_t *data16 = data;
				uint32_t reg;
				for (i = 0; i < he; i++) {
					reg = *data16;
					bus_space_write_4(sc->sc_memt, 
					    sc->sc_regh,
					    PM2_RE_BITMASK, reg);
					data16++;
				}
				break;
			}
			}
		}
		if (attr & 1)
			pm2fb_rectfill(sc, x, y + he - 2, wi, 1, fg);
	}
}

static void
pm2fb_putchar_aa(void *cookie, int row, int col, u_int c, long attr)
{
	struct rasops_info *ri = cookie;
	struct wsdisplay_font *font = PICK_FONT(ri, c);
	struct vcons_screen *scr = ri->ri_hw;
	struct pm2fb_softc *sc = scr->scr_cookie;
	uint32_t bg, fg, /*latch = 0,*/ bg8, fg8, pixel;
	int i, x, y, wi, he, r, g, b, aval;
	int r1, g1, b1, r0, g0, b0, fgo, bgo;
	uint8_t *data8;
	int rv = GC_NOPE, cnt = 0;

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
		pm2fb_rectfill(sc, x, y, wi, he, bg);
		if (attr & 1)
			pm2fb_rectfill(sc, x, y + he - 2, wi, 1, fg);
		return;
	}

#ifdef BITBLT_LE_WORKAROUND
	rv = GC_NOPE;
#else
	rv = glyphcache_try(&sc->sc_gc, c, x, y, attr);
	if (rv == GC_OK)
		return;
#endif

	data8 = WSFONT_GLYPH(c, font);

	pm2fb_wait(sc, 5);
#if 0
	/*
	 * TODO:
	 * - use packed mode here as well, instead of writing each pixel separately
	 * - see if we can trick the chip into doing the alpha blending for us
	 */
	x = x >> 2;
	wi = (wi + 3) >> 2;
#endif
	bus_space_write_4(sc->sc_memt, sc->sc_regh, PM2_RE_MODE, 0);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, PM2_RE_CONFIG,
			    PM2RECFG_WRITE_EN /*| PM2RECFG_PACKED*/);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, 
			    PM2_RE_RECT_START, (y << 16) | x);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, 
			    PM2_RE_RECT_SIZE, (he << 16) | wi);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, 
			    PM2_RE_RENDER,
			    PM2RE_RECTANGLE | PM2RE_SYNC_ON_HOST |
			    PM2RE_INC_X | PM2RE_INC_Y);
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

	pm2fb_wait(sc, 200);

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
#if 0
		latch = (latch << 8) | pixel;
		/* write in 32bit chunks */
		if ((i & 3) == 3) {
			bus_space_write_stream_4(sc->sc_memt, sc->sc_regh,
			    PM2_RE_DATA, latch);
			/*
			 * not strictly necessary, old data should be shifted 
			 * out 
			 */
			latch = 0;
			cnt++;
			if (cnt > 190) {
				pm2fb_wait(sc, 200);
				cnt = 0;
			}
		}
#else
		bus_space_write_4(sc->sc_memt, sc->sc_regh,
			    PM2_RE_COLOUR, pixel);

		if (cnt > 190) {
			pm2fb_wait(sc, 200);
			cnt = 0;
		}		
#endif
		data8++;
	}
#if 0
	/* if we have pixels left in latch write them out */
	if ((i & 3) != 0) {
		latch = latch << ((4 - (i & 3)) << 3);	
		bus_space_write_stream_4(sc->sc_memt, sc->sc_regh,
				    PM2_RE_DATA, latch);
	}
#endif
	/* 
	 * XXX
	 * occasionally characters end up in the cache only partially drawn
	 * apparently the blitter might end up grabbing them before they're
	 * completely flushed out into video memory
	 * so we let the pipeline drain a little bit before continuing
	 */
	pm2fb_wait(sc, 20);

	if (rv == GC_ADD) {
		glyphcache_add(&sc->sc_gc, c, x, y);
	} else if (attr & 1)
		pm2fb_rectfill(sc, x, y + he - 2, wi, 1, fg);
}

static void
pm2fb_copycols(void *cookie, int row, int srccol, int dstcol, int ncols)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;
	struct pm2fb_softc *sc = scr->scr_cookie;
	int32_t xs, xd, y, width, height;
	
	if ((sc->sc_locked == 0) && (sc->sc_mode == WSDISPLAYIO_MODE_EMUL)) {
		xs = ri->ri_xorigin + ri->ri_font->fontwidth * srccol;
		xd = ri->ri_xorigin + ri->ri_font->fontwidth * dstcol;
		y = ri->ri_yorigin + ri->ri_font->fontheight * row;
		width = ri->ri_font->fontwidth * ncols;
		height = ri->ri_font->fontheight;
		pm2fb_bitblt(sc, xs, y, xd, y, width, height, 3);
	}
}

static void
pm2fb_erasecols(void *cookie, int row, int startcol, int ncols, long fillattr)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;
	struct pm2fb_softc *sc = scr->scr_cookie;
	int32_t x, y, width, height, fg, bg, ul;
	
	if ((sc->sc_locked == 0) && (sc->sc_mode == WSDISPLAYIO_MODE_EMUL)) {
		x = ri->ri_xorigin + ri->ri_font->fontwidth * startcol;
		y = ri->ri_yorigin + ri->ri_font->fontheight * row;
		width = ri->ri_font->fontwidth * ncols;
		height = ri->ri_font->fontheight;
		rasops_unpack_attr(fillattr, &fg, &bg, &ul);

		pm2fb_rectfill(sc, x, y, width, height, ri->ri_devcmap[bg]);
	}
}

static void
pm2fb_copyrows(void *cookie, int srcrow, int dstrow, int nrows)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;
	struct pm2fb_softc *sc = scr->scr_cookie;
	int32_t x, ys, yd, width, height;

	if ((sc->sc_locked == 0) && (sc->sc_mode == WSDISPLAYIO_MODE_EMUL)) {
		x = ri->ri_xorigin;
		ys = ri->ri_yorigin + ri->ri_font->fontheight * srcrow;
		yd = ri->ri_yorigin + ri->ri_font->fontheight * dstrow;
		width = ri->ri_emuwidth;
		height = ri->ri_font->fontheight*nrows;
		pm2fb_bitblt(sc, x, ys, x, yd, width, height, 3);
	}
}

static void
pm2fb_eraserows(void *cookie, int row, int nrows, long fillattr)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;
	struct pm2fb_softc *sc = scr->scr_cookie;
	int32_t x, y, width, height, fg, bg, ul;
	
	if ((sc->sc_locked == 0) && (sc->sc_mode == WSDISPLAYIO_MODE_EMUL)) {
		x = ri->ri_xorigin;
		y = ri->ri_yorigin + ri->ri_font->fontheight * row;
		width = ri->ri_emuwidth;
		height = ri->ri_font->fontheight * nrows;
		rasops_unpack_attr(fillattr, &fg, &bg, &ul);

		pm2fb_rectfill(sc, x, y, width, height, ri->ri_devcmap[bg]);
	}
}

/*
 * Permedia2 can't blit outside of 2048x2048, so reject anything higher
 * max. dot clock is probably too high
 */

#define MODE_IS_VALID(m) (((m)->hdisplay < 2048) && \
	((m)->dot_clock < 230000))

static void
pm2_setup_i2c(struct pm2fb_softc *sc)
{
	int i;
#ifdef PM2FB_DEBUG
	int j;
#endif

	/* Fill in the i2c tag */
	sc->sc_i2c.ic_cookie = sc;
	sc->sc_i2c.ic_acquire_bus = pm2fb_i2c_acquire_bus;
	sc->sc_i2c.ic_release_bus = pm2fb_i2c_release_bus;
	sc->sc_i2c.ic_send_start = pm2fb_i2c_send_start;
	sc->sc_i2c.ic_send_stop = pm2fb_i2c_send_stop;
	sc->sc_i2c.ic_initiate_xfer = pm2fb_i2c_initiate_xfer;
	sc->sc_i2c.ic_read_byte = pm2fb_i2c_read_byte;
	sc->sc_i2c.ic_write_byte = pm2fb_i2c_write_byte;
	sc->sc_i2c.ic_exec = NULL;

	DPRINTF("data: %08x\n", bus_space_read_4(sc->sc_memt, sc->sc_regh,
		PM2_DISPLAY_DATA));

	bus_space_write_4(sc->sc_memt, sc->sc_regh, PM2_DISPLAY_DATA, 0);

	/* zero out the EDID buffer */
	memset(sc->sc_edid_data, 0, 128);

	/* Some monitors don't respond first time */
	i = 0;
	while (sc->sc_edid_data[1] == 0 && i < 10) {
		ddc_read_edid(&sc->sc_i2c, sc->sc_edid_data, 128);
		i++;
	}
#ifdef PM2FB_DEBUG
	printf("i = %d\n", i);
	for (i = 0; i < 128; i += 16) {
		printf("%02x:", i);
		for (j = 0; j < 16; j++)
			printf(" %02x", sc->sc_edid_data[i + j]);
		printf("\n");
	}
#endif

	if (edid_parse(&sc->sc_edid_data[0], &sc->sc_ei) != -1) {
#ifdef PM2FB_DEBUG
		edid_print(&sc->sc_ei);
#endif

		/*
		 * Now pick a mode.
		 */
		if ((sc->sc_ei.edid_preferred_mode != NULL)) {
			struct videomode *m = sc->sc_ei.edid_preferred_mode;
			if (MODE_IS_VALID(m)) {
				sc->sc_videomode = m;
			} else {
				aprint_error_dev(sc->sc_dev,
				    "unable to use preferred mode\n");
			}
		}
		/*
		 * if we can't use the preferred mode go look for the
		 * best one we can support
		 */
		if (sc->sc_videomode == NULL) {
			struct videomode *m = sc->sc_ei.edid_modes;

			sort_modes(sc->sc_ei.edid_modes,
			    &sc->sc_ei.edid_preferred_mode,
			    sc->sc_ei.edid_nmodes);
			if (sc->sc_videomode == NULL)
				for (int n = 0; n < sc->sc_ei.edid_nmodes; n++)
					if (MODE_IS_VALID(&m[n])) {
						sc->sc_videomode = &m[n];
						break;
					}
		}
	}
	if (sc->sc_videomode == NULL) {
		/* no EDID data? */
		sc->sc_videomode = pick_mode_by_ref(sc->sc_width, 
		    sc->sc_height, 60);
	}
	if (sc->sc_videomode != NULL) {
		pm2fb_set_mode(sc, sc->sc_videomode);
	}
}

/* I2C bitbanging */
static void pm2fb_i2cbb_set_bits(void *cookie, uint32_t bits)
{
	struct pm2fb_softc *sc = cookie;
	uint32_t out;

	out = bits << 2;	/* bitmasks match the IN bits */

	bus_space_write_4(sc->sc_memt, sc->sc_regh, PM2_DISPLAY_DATA, out);
	delay(100);
}

static void pm2fb_i2cbb_set_dir(void *cookie, uint32_t dir)
{
	/* Nothing to do */
}

static uint32_t pm2fb_i2cbb_read(void *cookie)
{
	struct pm2fb_softc *sc = cookie;
	uint32_t bits;

	bits = bus_space_read_4(sc->sc_memt, sc->sc_regh, PM2_DISPLAY_DATA);
	return bits;
}

/* higher level I2C stuff */
static int
pm2fb_i2c_acquire_bus(void *cookie, int flags)
{
	/* private bus */
	return (0);
}

static void
pm2fb_i2c_release_bus(void *cookie, int flags)
{
	/* private bus */
}

static int
pm2fb_i2c_send_start(void *cookie, int flags)
{
	return (i2c_bitbang_send_start(cookie, flags, &pm2fb_i2cbb_ops));
}

static int
pm2fb_i2c_send_stop(void *cookie, int flags)
{

	return (i2c_bitbang_send_stop(cookie, flags, &pm2fb_i2cbb_ops));
}

static int
pm2fb_i2c_initiate_xfer(void *cookie, i2c_addr_t addr, int flags)
{

	return (i2c_bitbang_initiate_xfer(cookie, addr, flags, 
	    &pm2fb_i2cbb_ops));
}

static int
pm2fb_i2c_read_byte(void *cookie, uint8_t *valp, int flags)
{
	return (i2c_bitbang_read_byte(cookie, valp, flags, &pm2fb_i2cbb_ops));
}

static int
pm2fb_i2c_write_byte(void *cookie, uint8_t val, int flags)
{
	return (i2c_bitbang_write_byte(cookie, val, flags, &pm2fb_i2cbb_ops));
}

static int
pm2vfb_set_pll(struct pm2fb_softc *sc, int freq)
{
	int m, n, p, diff, out_freq, bm = 1, bn = 3, bp = 0,
	    bdiff = 1000000 /* , bfreq */;
	int fi;
	uint8_t temp;

	for (m = 1; m < 128; m++) {
		for (n = 2 * m + 1; n < 256; n++) {
			fi = PM2_EXT_CLOCK_FREQ * n / m;
			for (p = 0; p < 2; p++) {
				out_freq = fi >> (p + 1);
				diff = abs(out_freq - freq);
				if (diff < bdiff) {
					bdiff = diff;
					/* bfreq = out_freq; */
					bm = m;
					bn = n;
					bp = p;
				}
			}
		}
	}
#if 0
	/*
	 * XXX
	 * output between switching modes and attaching a wsdisplay will
	 * go through firmware calls on sparc64 and potentially mess up
	 * our drawing engine state
	 */
	DPRINTF("best: %d kHz ( %d off ), %d %d %d\n", bfreq, bdiff, bm, bn, bp);
#endif
	temp = pm2fb_read_dac(sc, PM2V_DAC_CLOCK_CONTROL) & 0xfc;
	pm2fb_write_dac(sc, PM2V_DAC_CONTROL, 0);
	pm2fb_write_dac(sc, PM2V_DAC_CLOCK_A_M, bm);
	pm2fb_write_dac(sc, PM2V_DAC_CLOCK_A_N, bn);
	pm2fb_write_dac(sc, PM2V_DAC_CLOCK_A_P, bp);
	pm2fb_write_dac(sc, PM2V_DAC_CLOCK_CONTROL, temp | 3);
	return 0;
}

static int
pm2fb_set_pll(struct pm2fb_softc *sc, int freq)
{
	uint8_t  reg, bm = 0, bn = 0, bp = 0;
	unsigned int  m, n, p, fi, diff, out_freq, bdiff = 1000000;

	for (n = 2; n < 15; n++) {
		for (m = 2 ; m < 256; m++) {
			fi = PM2_EXT_CLOCK_FREQ * m / n;
			if (fi >= PM2_PLL_FREQ_MIN && fi <= PM2_PLL_FREQ_MAX) {
				for (p = 0; p < 5; p++) {
					out_freq = fi >> p;
					diff = abs(out_freq - freq);
					if (diff < bdiff) {
						bm = m;
						bn = n;
						bp = p;
						bdiff = diff;
					}
				}
			}
		}
	}

	pm2fb_write_dac(sc, PM2_DAC_PIXELCLKA_M, bm);
	pm2fb_write_dac(sc, PM2_DAC_PIXELCLKA_N, bn);
	pm2fb_write_dac(sc, PM2_DAC_PIXELCLKA_P, (bp | 0x08));

	do {
		reg = bus_space_read_1(sc->sc_memt, sc->sc_regh,
			PM2_DAC_INDEX_DATA);
	} while (reg == PCLK_LOCKED);

	return 0;
}

/*
 * most of the following was adapted from the xf86-video-glint driver's
 * pm2_dac.c (8bpp only)
 */
static void 
pm2fb_set_dac(struct pm2fb_softc *sc, const struct videomode *mode)
{
	int t1, t2, t3, t4, stride;
	uint32_t vclk, tmp;
	uint8_t sync = 0;
	
	t1 = mode->hsync_start - mode->hdisplay;
	t2 = mode->vsync_start - mode->vdisplay;
	t3 = mode->hsync_end - mode->hsync_start;
	t4 = mode->vsync_end - mode->vsync_start;

	/* first round up to the next multiple of 32 */
	stride = (mode->hdisplay + 31) & ~31;
	/* then find the next bigger one that we have partial products for */
	while ((partprodPermedia[stride >> 5] == -1) && (stride < 2048)) {
		stride += 32;
	}

	bus_space_write_4(sc->sc_memt, sc->sc_regh, PM2_HTOTAL, 
	    ((mode->htotal) >> 2) - 1);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, PM2_HSYNC_END,
	    (t1 + t3) >> 2);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, PM2_HSYNC_START,
	    (t1 >> 2));
	bus_space_write_4(sc->sc_memt, sc->sc_regh, PM2_HBLANK_END,
	    (mode->htotal - mode->hdisplay) >> 2);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, PM2_HGATE_END,
	    (mode->htotal - mode->hdisplay) >> 2);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, PM2_SCREEN_STRIDE,  
	    stride >> 3);

	bus_space_write_4(sc->sc_memt, sc->sc_regh, PM2_VTOTAL, 
	    mode->vtotal - 2);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, PM2_VSYNC_END,
	    t2 + t4 - 1);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, PM2_VSYNC_START,
	    t2 - 1);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, PM2_VBLANK_END,
	    mode->vtotal - mode->vdisplay);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, PM2_VIDEO_CONTROL,
	    PM2_VC_VIDEO_ENABLE | 
	    PM2_VC_HSYNC_ACT_HIGH | PM2_VC_VSYNC_ACT_HIGH);

	vclk = bus_space_read_4(sc->sc_memt, sc->sc_regh, PM2_VCLKCTL);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, PM2_VCLKCTL,
	    vclk & 0xfffffffc);

	tmp = bus_space_read_4(sc->sc_memt, sc->sc_regh, PM2_CHIP_CONFIG);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, PM2_CHIP_CONFIG,
	    tmp & 0xffffffdd);

	pm2fb_write_dac(sc, PM2_DAC_MODE_CONTROL, MOC_BUFFERFRONT);
	pm2fb_set_pll(sc, mode->dot_clock);  	

	sync = MC_PALETTE_8BIT;

	if (!(mode->flags & VID_PHSYNC))
	    sync |= MC_HSYNC_INV;
	if (!(mode->flags & VID_PVSYNC))
	    sync |= MC_VSYNC_INV;

	pm2fb_write_dac(sc, PM2_DAC_MISC_CONTROL, sync);
	pm2fb_write_dac(sc, PM2_DAC_COLOR_MODE,
	    CM_PALETTE | CM_GUI_ENABLE | CM_RGB);

	sc->sc_width = mode->hdisplay;
	sc->sc_height = mode->vdisplay;
	sc->sc_depth = 8;
	sc->sc_stride = stride;
	aprint_normal_dev(sc->sc_dev, "pm2 using %d x %d in 8 bit, stride %d\n",
	    sc->sc_width, sc->sc_height, stride);
}

/*
 * most of the following was adapted from the xf86-video-glint driver's
 * pm2v_dac.c
 */				
static void
pm2vfb_set_dac(struct pm2fb_softc *sc, const struct videomode *mode)
{
	int t1, t2, t3, t4, stride;
	uint32_t vclk;
	uint8_t sync = 0;
	
	t1 = mode->hsync_start - mode->hdisplay;
	t2 = mode->vsync_start - mode->vdisplay;
	t3 = mode->hsync_end - mode->hsync_start;
	t4 = mode->vsync_end - mode->vsync_start;

	bus_space_write_4(sc->sc_memt, sc->sc_regh, PM2_HTOTAL,
	    ((mode->htotal) >> 3) - 1);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, PM2_HSYNC_END,
	    (t1 + t3) >> 3);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, PM2_HSYNC_START,
	    (t1 >> 3) - 1);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, PM2_HBLANK_END,
	    (mode->htotal - mode->hdisplay) >> 3);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, PM2_HGATE_END,
	    (mode->htotal - mode->hdisplay) >> 3);

	/* first round up to the next multiple of 32 */
	stride = (mode->hdisplay + 31) & ~31;
	/* then find the next bigger one that we have partial products for */
	while ((partprodPermedia[stride >> 5] == -1) && (stride < 2048)) {
		stride += 32;
	}
	bus_space_write_4(sc->sc_memt, sc->sc_regh, PM2_SCREEN_STRIDE,
	    stride >> 3);

	bus_space_write_4(sc->sc_memt, sc->sc_regh, PM2_VTOTAL,
	    mode->vtotal - 1);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, PM2_VSYNC_END,
	    t2 + t4);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, PM2_VSYNC_START,
	    t2);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, PM2_VBLANK_END,
	    mode->vtotal - mode->vdisplay);

	bus_space_write_4(sc->sc_memt, sc->sc_regh, PM2_VIDEO_CONTROL,
	    PM2_VC_VIDEO_ENABLE | PM2_VC_RAMDAC_64BIT |
	    PM2_VC_HSYNC_ACT_HIGH | PM2_VC_VSYNC_ACT_HIGH);

	vclk = bus_space_read_4(sc->sc_memt, sc->sc_regh, PM2_VCLKCTL);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, PM2_VCLKCTL,
	    vclk & 0xfffffffc);

	pm2vfb_set_pll(sc, mode->dot_clock / 2);
	pm2fb_write_dac(sc, PM2V_DAC_MISC_CONTROL, PM2V_DAC_8BIT);

	if (mode->flags & VID_PHSYNC)
		sync |= PM2V_DAC_HSYNC_INV;
	if (mode->flags & VID_PVSYNC)
		sync |= PM2V_DAC_VSYNC_INV;
	pm2fb_write_dac(sc, PM2V_DAC_SYNC_CONTROL, sync);
	
	pm2fb_write_dac(sc, PM2V_DAC_COLOR_FORMAT, PM2V_DAC_PALETTE);
	pm2fb_write_dac(sc, PM2V_DAC_PIXEL_SIZE, PM2V_PS_8BIT);
	sc->sc_width = mode->hdisplay;
	sc->sc_height = mode->vdisplay;
	sc->sc_depth = 8;
	sc->sc_stride = stride;
	aprint_normal_dev(sc->sc_dev, "pm2v using %d x %d in 8 bit, stride %d\n",
	    sc->sc_width, sc->sc_height, stride);
}

static void
pm2fb_set_mode(struct pm2fb_softc *sc, const struct videomode *mode)
{
	if (sc->sc_is_pm2) {
		pm2fb_set_dac(sc, mode);
	} else {
		pm2vfb_set_dac(sc, mode);
	}      
}
