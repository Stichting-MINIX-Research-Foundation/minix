/*	$NetBSD: gffb.c,v 1.10 2015/09/16 16:52:54 macallan Exp $	*/

/*
 * Copyright (c) 2013 Michael Lorenz
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
 * A console driver for nvidia geforce graphics controllers
 * tested on macppc only so far, should work on other hardware as long as
 * something sets up a usable graphics mode and sets the right device properties
 * This driver should work with all NV1x hardware but so far it's been tested
 * only on NV11 / GeForce2 MX. Needs testing with more hardware and if
 * successful, PCI IDs need to be added to gffb_match()
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: gffb.c,v 1.10 2015/09/16 16:52:54 macallan Exp $");

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
#include <dev/pci/gffbreg.h>

#include <dev/wscons/wsdisplayvar.h>
#include <dev/wscons/wsconsio.h>
#include <dev/wsfont/wsfont.h>
#include <dev/rasops/rasops.h>
#include <dev/wscons/wsdisplay_vconsvar.h>
#include <dev/pci/wsdisplay_pci.h>
#include <dev/wscons/wsdisplay_glyphcachevar.h>

#include <dev/i2c/i2cvar.h>

#include "opt_gffb.h"
#include "opt_vcons.h"

#ifdef GFFB_DEBUG
#define DPRINTF printf
#else
#define DPRINTF while(0) printf
#endif

struct gffb_softc {
	device_t sc_dev;

	pci_chipset_tag_t sc_pc;
	pcitag_t sc_pcitag;

	bus_space_tag_t sc_memt;
	bus_space_tag_t sc_iot;

	bus_space_handle_t sc_regh, sc_fbh;
	bus_addr_t sc_fb, sc_reg;
	bus_size_t sc_fbsize, sc_regsize;
	uint8_t *sc_fbaddr;
	size_t sc_vramsize;

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
	int sc_put, sc_current, sc_free;
	uint32_t sc_rop;
	void (*sc_putchar)(void *, int, int, u_int, long);
	kmutex_t sc_lock;
	glyphcache sc_gc;
};

static int	gffb_match(device_t, cfdata_t, void *);
static void	gffb_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(gffb, sizeof(struct gffb_softc),
    gffb_match, gffb_attach, NULL, NULL);

static int	gffb_ioctl(void *, void *, u_long, void *, int,
			     struct lwp *);
static paddr_t	gffb_mmap(void *, void *, off_t, int);
static void	gffb_init_screen(void *, struct vcons_screen *, int, long *);

static int	gffb_putcmap(struct gffb_softc *, struct wsdisplay_cmap *);
static int 	gffb_getcmap(struct gffb_softc *, struct wsdisplay_cmap *);
static void	gffb_restore_palette(struct gffb_softc *);
static int 	gffb_putpalreg(struct gffb_softc *, uint8_t, uint8_t,
			    uint8_t, uint8_t);

static void	gffb_init(struct gffb_softc *);

static void	gffb_make_room(struct gffb_softc *, int);
static void	gffb_sync(struct gffb_softc *);

static void	gffb_rectfill(struct gffb_softc *, int, int, int, int,
			    uint32_t);
static void	gffb_bitblt(void *, int, int, int, int, int,
			    int, int);
static void	gffb_rop(struct gffb_softc *, int);

static void	gffb_cursor(void *, int, int, int);
static void	gffb_putchar(void *, int, int, u_int, long);
static void	gffb_copycols(void *, int, int, int, int);
static void	gffb_erasecols(void *, int, int, int, long);
static void	gffb_copyrows(void *, int, int, int);
static void	gffb_eraserows(void *, int, int, long);

#define GFFB_READ_4(o) bus_space_read_4(sc->sc_memt, sc->sc_regh, (o))
#define GFFB_WRITE_4(o, v) bus_space_write_4(sc->sc_memt, sc->sc_regh, (o), (v))

struct wsdisplay_accessops gffb_accessops = {
	gffb_ioctl,
	gffb_mmap,
	NULL,	/* alloc_screen */
	NULL,	/* free_screen */
	NULL,	/* show_screen */
	NULL, 	/* load_font */
	NULL,	/* pollc */
	NULL	/* scroll */
};

static int
gffb_match(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa = (struct pci_attach_args *)aux;

	if (PCI_CLASS(pa->pa_class) != PCI_CLASS_DISPLAY)
		return 0;
	if (PCI_VENDOR(pa->pa_id) != PCI_VENDOR_NVIDIA)
		return 0;

	/* only card tested on so far - likely need a list */
	if (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_NVIDIA_GEFORCE2MX)
		return 100;
	return (0);
}

static void
gffb_attach(device_t parent, device_t self, void *aux)
{
	struct gffb_softc	*sc = device_private(self);
	struct pci_attach_args	*pa = aux;
	struct rasops_info	*ri;
	bus_space_tag_t		tag;
	struct wsemuldisplaydev_attach_args aa;
	prop_dictionary_t	dict;
	unsigned long		defattr;
	bool			is_console = FALSE;
	int			i, j, f;
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
	sc->sc_height -= 300;
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

	if (pci_mapreg_map(pa, 0x10, PCI_MAPREG_TYPE_MEM, 0,
	    &tag, &sc->sc_regh, &sc->sc_reg, &sc->sc_regsize)) {
		aprint_error("%s: failed to map registers.\n",
		    device_xname(sc->sc_dev));
	}
	sc->sc_vramsize = GFFB_READ_4(GFFB_VRAM) & 0xfff00000;

	/* don't map more VRAM than we actually have */
	if (pci_mapreg_info(sc->sc_pc, sc->sc_pcitag,
	    0x14, PCI_MAPREG_TYPE_MEM, &sc->sc_fb, &sc->sc_fbsize, &f)) {
		aprint_error("%s: can't find the framebuffer?!\n",
		    device_xname(sc->sc_dev));
	}

	if (bus_space_map(sc->sc_memt, sc->sc_fb, sc->sc_vramsize,
	    BUS_SPACE_MAP_PREFETCHABLE | BUS_SPACE_MAP_LINEAR,
	    &sc->sc_fbh)) {
		aprint_error("%s: failed to map the framebuffer.\n",
		    device_xname(sc->sc_dev));
	}
	sc->sc_fbaddr = bus_space_vaddr(tag, sc->sc_fbh);

	aprint_normal("%s: %d MB aperture at 0x%08x\n", device_xname(self),
	    (int)(sc->sc_fbsize >> 20), (uint32_t)sc->sc_fb);
	aprint_normal_dev(sc->sc_dev, "%d MB video memory\n",
	    (int)(sc->sc_vramsize >> 20));

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

#ifdef GFFB_DEBUG
	printf("put: %08x\n", GFFB_READ_4(GFFB_FIFO_PUT));
	printf("get: %08x\n", GFFB_READ_4(GFFB_FIFO_GET));
#endif

	/*
	 * we don't have hardware synchronization so we need a lock to serialize
	 * access to the DMA buffer between normal and kernel output
	 * actually it might be enough to use atomic ops on sc_current, sc_free
	 * etc. but for now we'll play it safe
	 * XXX we will probably deadlock if we take an interrupt while sc_lock
	 * is held and then try to printf()
	 */
	mutex_init(&sc->sc_lock, MUTEX_DEFAULT, IPL_NONE);

	/* init engine here */	
	gffb_init(sc);

	vcons_init(&sc->vd, sc, &sc->sc_defaultscreen_descr,
	    &gffb_accessops);
	sc->vd.init_screen = gffb_init_screen;


	ri = &sc->sc_console_screen.scr_ri;

	sc->sc_gc.gc_bitblt = gffb_bitblt;
	sc->sc_gc.gc_blitcookie = sc;
	sc->sc_gc.gc_rop = 0xcc;

	if (is_console) {
		vcons_init_screen(&sc->vd, &sc->sc_console_screen, 1,
		    &defattr);
		sc->sc_console_screen.scr_flags |= VCONS_SCREEN_IS_STATIC;

		gffb_rectfill(sc, 0, 0, sc->sc_width, sc->sc_height,
		    ri->ri_devcmap[(defattr >> 16) & 0xf]);
		
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
		gffb_putpalreg(sc, i, cmap[j], cmap[j + 1], cmap[j + 2]);
		j += 3;
	}

	/* no suspend/resume support yet */
	pmf_device_register(sc->sc_dev, NULL, NULL);

	aa.console = is_console;
	aa.scrdata = &sc->sc_screenlist;
	aa.accessops = &gffb_accessops;
	aa.accesscookie = &sc->vd;

	config_found(sc->sc_dev, &aa, wsemuldisplaydevprint);

#ifdef GFFB_DEBUG
	for (i = 0; i < 40; i++) {
		for (j = 0; j < 40; j++) {
			gffb_rectfill(sc, i * 20, j * 20, 20, 20,
			    (i + j ) & 1 ? 0xe0e0e0e0 : 0x03030303);
		}
	}
	
	gffb_rectfill(sc, 0, 800, 1280, 224, 0x92929292);
	gffb_bitblt(sc, 0, 10, 10, 810, 200, 20, 0xcc);
	gffb_bitblt(sc, 0, 10, 10, 840, 300, 20, 0xcc);
	gffb_bitblt(sc, 0, 10, 10, 870, 400, 20, 0xcc);
	gffb_bitblt(sc, 0, 10, 10, 900, 500, 20, 0xcc);
	gffb_bitblt(sc, 0, 10, 10, 930, 600, 20, 0xcc);
	gffb_bitblt(sc, 0, 10, 610, 810, 200, 20, 0xcc);
	gffb_bitblt(sc, 0, 10, 610, 840, 300, 20, 0xcc);
	gffb_bitblt(sc, 0, 10, 610, 870, 400, 20, 0xcc);
	gffb_bitblt(sc, 0, 10, 610, 900, 500, 20, 0xcc);
	gffb_bitblt(sc, 0, 10, 610, 930, 600, 20, 0xcc);
	gffb_sync(sc);
	printf("put %x current %x\n", sc->sc_put, sc->sc_current);
#endif
}

static int
gffb_ioctl(void *v, void *vs, u_long cmd, void *data, int flag,
	struct lwp *l)
{
	struct vcons_data *vd = v;
	struct gffb_softc *sc = vd->cookie;
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
		return gffb_getcmap(sc,
		    (struct wsdisplay_cmap *)data);

	case WSDISPLAYIO_PUTCMAP:
		return gffb_putcmap(sc,
		    (struct wsdisplay_cmap *)data);

	case WSDISPLAYIO_LINEBYTES:
		*(u_int *)data = sc->sc_stride;
		return 0;

	case WSDISPLAYIO_SMODE: {
		int new_mode = *(int*)data;
		if (new_mode != sc->sc_mode) {
			sc->sc_mode = new_mode;
			if(new_mode == WSDISPLAYIO_MODE_EMUL) {
				gffb_init(sc);
				gffb_restore_palette(sc);
				glyphcache_wipe(&sc->sc_gc);
				gffb_rectfill(sc, 0, 0, sc->sc_width,
				    sc->sc_height, ms->scr_ri.ri_devcmap[
				    (ms->scr_defattr >> 16) & 0xf]);
				vcons_redraw_screen(ms);
			}
		}
		}
		return 0;
	
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
gffb_mmap(void *v, void *vs, off_t offset, int prot)
{
	struct vcons_data *vd = v;
	struct gffb_softc *sc = vd->cookie;
	paddr_t pa;

	/* 'regular' framebuffer mmap()ing */
	if (offset < sc->sc_vramsize) {
		pa = bus_space_mmap(sc->sc_memt, sc->sc_fb + offset + 0x2000,
		    0, prot, BUS_SPACE_MAP_LINEAR);
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
gffb_init_screen(void *cookie, struct vcons_screen *scr,
    int existing, long *defattr)
{
	struct gffb_softc *sc = cookie;
	struct rasops_info *ri = &scr->scr_ri;

	ri->ri_depth = sc->sc_depth;
	ri->ri_width = sc->sc_width;
	ri->ri_height = sc->sc_height;
	ri->ri_stride = sc->sc_stride;
	ri->ri_bits = sc->sc_fbaddr + 0x2000;
	ri->ri_flg = RI_CENTER;
	if (sc->sc_depth == 8)
		ri->ri_flg |= RI_8BIT_IS_RGB | RI_ENABLE_ALPHA;

	rasops_init(ri, 0, 0);
	ri->ri_caps = WSSCREEN_WSCOLORS;

	rasops_reconfig(ri, sc->sc_height / ri->ri_font->fontheight,
		    sc->sc_width / ri->ri_font->fontwidth);

	ri->ri_hw = scr;

	sc->sc_putchar = ri->ri_ops.putchar;
	ri->ri_ops.copyrows = gffb_copyrows;
	ri->ri_ops.copycols = gffb_copycols;
	ri->ri_ops.eraserows = gffb_eraserows;
	ri->ri_ops.erasecols = gffb_erasecols;
	ri->ri_ops.cursor = gffb_cursor;
	ri->ri_ops.putchar = gffb_putchar;
}

static int
gffb_putcmap(struct gffb_softc *sc, struct wsdisplay_cmap *cm)
{
	u_char *r, *g, *b;
	u_int index = cm->index;
	u_int count = cm->count;
	int i, error;
	u_char rbuf[256], gbuf[256], bbuf[256];

#ifdef GFFB_DEBUG
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
		gffb_putpalreg(sc, index, *r, *g, *b);
		index++;
		r++, g++, b++;
	}
	return 0;
}

static int
gffb_getcmap(struct gffb_softc *sc, struct wsdisplay_cmap *cm)
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
gffb_restore_palette(struct gffb_softc *sc)
{
	int i;

	for (i = 0; i < (1 << sc->sc_depth); i++) {
		gffb_putpalreg(sc, i, sc->sc_cmap_red[i],
		    sc->sc_cmap_green[i], sc->sc_cmap_blue[i]);
	}
}

static int
gffb_putpalreg(struct gffb_softc *sc, uint8_t idx, uint8_t r, uint8_t g,
    uint8_t b)
{
	/* port 0 */
	bus_space_write_1(sc->sc_memt, sc->sc_regh,
	    GFFB_PDIO0 + GFFB_PEL_IW, idx);
	bus_space_write_1(sc->sc_memt, sc->sc_regh,
	    GFFB_PDIO0 + GFFB_PEL_D, r);
	bus_space_write_1(sc->sc_memt, sc->sc_regh,
	    GFFB_PDIO0 + GFFB_PEL_D, g);
	bus_space_write_1(sc->sc_memt, sc->sc_regh,
	    GFFB_PDIO0 + GFFB_PEL_D, b);

	/* port 1 */
	bus_space_write_1(sc->sc_memt, sc->sc_regh,
	    GFFB_PDIO1 + GFFB_PEL_IW, idx);
	bus_space_write_1(sc->sc_memt, sc->sc_regh,
	    GFFB_PDIO1 + GFFB_PEL_D, r);
	bus_space_write_1(sc->sc_memt, sc->sc_regh,
	    GFFB_PDIO1 + GFFB_PEL_D, g);
	bus_space_write_1(sc->sc_memt, sc->sc_regh,
	    GFFB_PDIO1 + GFFB_PEL_D, b);
	
	return 0;
}


static void
gffb_dma_kickoff(struct gffb_softc *sc)
{

	if (sc->sc_current != sc->sc_put) {
		sc->sc_put = sc->sc_current;
		membar_sync();
		(void)*sc->sc_fbaddr;
		GFFB_WRITE_4(GFFB_FIFO_PUT, sc->sc_put);
		membar_sync();
	}
}

static void
gffb_dmanext(struct gffb_softc *sc, uint32_t data)
{
	bus_space_write_stream_4(sc->sc_memt, sc->sc_fbh, sc->sc_current, data);
	sc->sc_current += 4;
}

static void
gffb_dmastart(struct gffb_softc *sc, uint32_t tag, int size)
{
	if(sc->sc_free <= (size << 2))
		gffb_make_room(sc, size);
	gffb_dmanext(sc, ((size) << 18) | (tag));
	sc->sc_free -= ((size + 1) << 2);
}

/*
 * from xf86_video_nv/nv_xaa.c:
 * There is a HW race condition with videoram command buffers.
 * You can't jump to the location of your put offset.  We write put
 * at the jump offset + SKIPS dwords with noop padding in between
 * to solve this problem
 */

#define SKIPS  8

static void 
gffb_make_room(struct gffb_softc *sc, int size)
{
	uint32_t get;

	size = (size + 1) << 2;	/* slots -> offset */

	while (sc->sc_free < size) {
		get = GFFB_READ_4(GFFB_FIFO_GET);

		if (sc->sc_put >= get) {
			sc->sc_free = 0x2000 - sc->sc_current;
			if (sc->sc_free < size) {
				gffb_dmanext(sc, 0x20000000);
				if(get <= (SKIPS << 2)) {
					if (sc->sc_put <= (SKIPS << 2)) {
						/* corner case - will be idle */
						GFFB_WRITE_4(GFFB_FIFO_PUT,
						    (SKIPS + 1) << 2);
					}
					do {
						get =GFFB_READ_4(GFFB_FIFO_GET);
					} while (get <= (SKIPS << 2));
				}
				GFFB_WRITE_4(GFFB_FIFO_PUT, SKIPS << 2);
				sc->sc_current = sc->sc_put = (SKIPS << 2);
				sc->sc_free = get - ((SKIPS + 1) << 2);
			}
		} else
			sc->sc_free = get - sc->sc_current - 4;
	}
}

static void
gffb_sync(struct gffb_softc *sc)
{
	int bail;
	int i;

	/*
	 * if there are commands in the buffer make sure the chip is actually
	 * trying to run them
	 */
	gffb_dma_kickoff(sc);

	/* now wait for the command buffer to drain... */
	bail = 100000000;
	while ((GFFB_READ_4(GFFB_FIFO_GET) != sc->sc_put) && (bail > 0)) {
		bail--;
	}
	if (bail == 0) goto crap;

	/* ... and for the engine to go idle */
	bail = 100000000;
	while((GFFB_READ_4(GFFB_BUSY) != 0) && (bail > 0)) {
		bail--;
	}
	if (bail == 0) goto crap;
	return;
crap:
	/* if we time out fill the buffer with NOPs and cross fingers */
	sc->sc_put = 0;
	sc->sc_current = 0;
	for (i = 0; i < 0x2000; i += 4)
		bus_space_write_stream_4(sc->sc_memt, sc->sc_fbh, i, 0);
	aprint_error_dev(sc->sc_dev, "DMA lockup\n");
}

static void
gffb_init(struct gffb_softc *sc)
{
	int i;
	uint32_t foo;

	/* init display start */
	GFFB_WRITE_4(GFFB_CRTC0 + GFFB_DISPLAYSTART, 0x2000);
	GFFB_WRITE_4(GFFB_CRTC1 + GFFB_DISPLAYSTART, 0x2000);
	GFFB_WRITE_4(GFFB_PDIO0 + GFFB_PEL_MASK, 0xff);
	GFFB_WRITE_4(GFFB_PDIO1 + GFFB_PEL_MASK, 0xff);
	
	/* DMA stuff. A whole lot of magic number voodoo from xf86-video-nv */
	GFFB_WRITE_4(GFFB_PMC + 0x140, 0);
	GFFB_WRITE_4(GFFB_PMC + 0x200, 0xffff00ff);
	GFFB_WRITE_4(GFFB_PMC + 0x200, 0xffffffff);
	GFFB_WRITE_4(GFFB_PTIMER + 0x800, 8);
	GFFB_WRITE_4(GFFB_PTIMER + 0x840, 3);
	GFFB_WRITE_4(GFFB_PTIMER + 0x500, 0);
	GFFB_WRITE_4(GFFB_PTIMER + 0x400, 0xffffffff);
	for (i = 0; i < 8; i++) {
		GFFB_WRITE_4(GFFB_PMC + 0x240 + (i * 0x10), 0);
		GFFB_WRITE_4(GFFB_PMC + 0x244 + (i * 0x10),
		    sc->sc_vramsize - 1);
	}

	for (i = 0; i < 8; i++) {
		GFFB_WRITE_4(GFFB_PFB + 0x0240 + (i * 0x10), 0);
		GFFB_WRITE_4(GFFB_PFB + 0x0244 + (i * 0x10),
		    sc->sc_vramsize - 1);
	}

	GFFB_WRITE_4(GFFB_PRAMIN, 0x80000010);
	GFFB_WRITE_4(GFFB_PRAMIN + 0x04, 0x80011201);
	GFFB_WRITE_4(GFFB_PRAMIN + 0x08, 0x80000011);
	GFFB_WRITE_4(GFFB_PRAMIN + 0x0c, 0x80011202);
	GFFB_WRITE_4(GFFB_PRAMIN + 0x10, 0x80000012);
	GFFB_WRITE_4(GFFB_PRAMIN + 0x14, 0x80011203);
	GFFB_WRITE_4(GFFB_PRAMIN + 0x18, 0x80000013);
	GFFB_WRITE_4(GFFB_PRAMIN + 0x1c, 0x80011204);
	GFFB_WRITE_4(GFFB_PRAMIN + 0x20, 0x80000014);
	GFFB_WRITE_4(GFFB_PRAMIN + 0x24, 0x80011205);
	GFFB_WRITE_4(GFFB_PRAMIN + 0x28, 0x80000015);
	GFFB_WRITE_4(GFFB_PRAMIN + 0x2c, 0x80011206);
	GFFB_WRITE_4(GFFB_PRAMIN + 0x30, 0x80000016);
	GFFB_WRITE_4(GFFB_PRAMIN + 0x34, 0x80011207);
	GFFB_WRITE_4(GFFB_PRAMIN + 0x38, 0x80000017);
	GFFB_WRITE_4(GFFB_PRAMIN + 0x3c, 0x80011208);
	GFFB_WRITE_4(GFFB_PRAMIN + 0x2000, 0x00003000);
	GFFB_WRITE_4(GFFB_PRAMIN + 0x2004, sc->sc_vramsize - 1);
	GFFB_WRITE_4(GFFB_PRAMIN + 0x2008, 0x00000002);
	GFFB_WRITE_4(GFFB_PRAMIN + 0x200c, 0x00000002);
	GFFB_WRITE_4(GFFB_PRAMIN + 0x2010, 0x01008042);	/* different for nv40 */
	GFFB_WRITE_4(GFFB_PRAMIN + 0x2014, 0);
	GFFB_WRITE_4(GFFB_PRAMIN + 0x2018, 0x12001200);
	GFFB_WRITE_4(GFFB_PRAMIN + 0x201c, 0);
	GFFB_WRITE_4(GFFB_PRAMIN + 0x2020, 0x01008043);
	GFFB_WRITE_4(GFFB_PRAMIN + 0x2024, 0);
	GFFB_WRITE_4(GFFB_PRAMIN + 0x2028, 0);
	GFFB_WRITE_4(GFFB_PRAMIN + 0x202c, 0);
	GFFB_WRITE_4(GFFB_PRAMIN + 0x2030, 0x01008044);
	GFFB_WRITE_4(GFFB_PRAMIN + 0x2034, 0x00000002);
	GFFB_WRITE_4(GFFB_PRAMIN + 0x2038, 0);
	GFFB_WRITE_4(GFFB_PRAMIN + 0x203c, 0);
	GFFB_WRITE_4(GFFB_PRAMIN + 0x2040, 0x01008019);
	GFFB_WRITE_4(GFFB_PRAMIN + 0x2044, 0);
	GFFB_WRITE_4(GFFB_PRAMIN + 0x2048, 0);
	GFFB_WRITE_4(GFFB_PRAMIN + 0x204c, 0);
	GFFB_WRITE_4(GFFB_PRAMIN + 0x2050, 0x0100a05c);
	GFFB_WRITE_4(GFFB_PRAMIN + 0x2054, 0);
	GFFB_WRITE_4(GFFB_PRAMIN + 0x2058, 0);
	GFFB_WRITE_4(GFFB_PRAMIN + 0x205c, 0);
	/* XXX 0x0100805f if !WaitVSynvPossible */
	GFFB_WRITE_4(GFFB_PRAMIN + 0x2060, 0x0100809f);
	GFFB_WRITE_4(GFFB_PRAMIN + 0x2064, 0);
	GFFB_WRITE_4(GFFB_PRAMIN + 0x2068, 0x12001200);
	GFFB_WRITE_4(GFFB_PRAMIN + 0x206c, 0);
	GFFB_WRITE_4(GFFB_PRAMIN + 0x2070, 0x0100804a);
	GFFB_WRITE_4(GFFB_PRAMIN + 0x2074, 0x00000002);
	GFFB_WRITE_4(GFFB_PRAMIN + 0x2078, 0);
	GFFB_WRITE_4(GFFB_PRAMIN + 0x207c, 0);
	GFFB_WRITE_4(GFFB_PRAMIN + 0x2080, 0x01018077);
	GFFB_WRITE_4(GFFB_PRAMIN + 0x2084, 0);
	GFFB_WRITE_4(GFFB_PRAMIN + 0x2088, 0x12001200);
	GFFB_WRITE_4(GFFB_PRAMIN + 0x208c, 0);
	GFFB_WRITE_4(GFFB_PRAMIN + 0x2090, 0x00003002);
	GFFB_WRITE_4(GFFB_PRAMIN + 0x2094, 0x00007fff);
	/* command buffer start with some flag in the lower bits */
	GFFB_WRITE_4(GFFB_PRAMIN + 0x2098, 0x00000002);
	GFFB_WRITE_4(GFFB_PRAMIN + 0x209c, 0x00000002);
#if BYTE_ORDER == BIG_ENDIAN
	GFFB_WRITE_4(GFFB_PRAMIN + 0x2010, 0x01088042);
	GFFB_WRITE_4(GFFB_PRAMIN + 0x2020, 0x01088043);
	GFFB_WRITE_4(GFFB_PRAMIN + 0x2030, 0x01088044);
	GFFB_WRITE_4(GFFB_PRAMIN + 0x2040, 0x01088019);
	GFFB_WRITE_4(GFFB_PRAMIN + 0x2050, 0x0108a05c);
	GFFB_WRITE_4(GFFB_PRAMIN + 0x2060, 0x0108809f);
	GFFB_WRITE_4(GFFB_PRAMIN + 0x2070, 0x0108804a);
	GFFB_WRITE_4(GFFB_PRAMIN + 0x2080, 0x01098077);
	GFFB_WRITE_4(GFFB_PRAMIN + 0x2034, 0x00000001);
	GFFB_WRITE_4(GFFB_PRAMIN + 0x2074, 0x00000001);
#endif

	/* PGRAPH setup */
	GFFB_WRITE_4(GFFB_PFIFO + 0x0500, 0);
	GFFB_WRITE_4(GFFB_PFIFO + 0x0504, 0x00000001);
	GFFB_WRITE_4(GFFB_PFIFO + 0x1200, 0);
	GFFB_WRITE_4(GFFB_PFIFO + 0x1250, 0);
	GFFB_WRITE_4(GFFB_PFIFO + 0x1204, 0x00000100);	/* different on nv40 */
	GFFB_WRITE_4(GFFB_PFIFO + 0x1240, 0);
	GFFB_WRITE_4(GFFB_PFIFO + 0x1244, 0);
	GFFB_WRITE_4(GFFB_PFIFO + 0x122c, 0x00001209);	/* different on nv40 */
	GFFB_WRITE_4(GFFB_PFIFO + 0x1000, 0);
	GFFB_WRITE_4(GFFB_PFIFO + 0x1050, 0);
	GFFB_WRITE_4(GFFB_PFIFO + 0x0210, 0x03000100);
	GFFB_WRITE_4(GFFB_PFIFO + 0x0214, 0x00000110);
	GFFB_WRITE_4(GFFB_PFIFO + 0x0218, 0x00000112);
	GFFB_WRITE_4(GFFB_PFIFO + 0x050c, 0x0000ffff);
	GFFB_WRITE_4(GFFB_PFIFO + 0x1258, 0x0000ffff);
	GFFB_WRITE_4(GFFB_PFIFO + 0x0140, 0);
	GFFB_WRITE_4(GFFB_PFIFO + 0x0100, 0xffffffff);
	GFFB_WRITE_4(GFFB_PFIFO + 0x1054, 0x00000001);
	GFFB_WRITE_4(GFFB_PFIFO + 0x1230, 0);
	GFFB_WRITE_4(GFFB_PFIFO + 0x1280, 0);
#if BYTE_ORDER == BIG_ENDIAN
	GFFB_WRITE_4(GFFB_PFIFO + 0x1224, 0x800f0078);
#else
	GFFB_WRITE_4(GFFB_PFIFO + 0x1224, 0x000f0078);
#endif
	GFFB_WRITE_4(GFFB_PFIFO + 0x1220, 0x00000001);
	GFFB_WRITE_4(GFFB_PFIFO + 0x1200, 0x00000001);
	GFFB_WRITE_4(GFFB_PFIFO + 0x1250, 0x00000001);
	GFFB_WRITE_4(GFFB_PFIFO + 0x1254, 0x00000001);
	GFFB_WRITE_4(GFFB_PFIFO + 0x0500, 0x00000001);

	GFFB_WRITE_4(GFFB_PGRAPH + 0x0080, 0xFFFFFFFF);
	GFFB_WRITE_4(GFFB_PGRAPH + 0x0080, 0x00000000);

	GFFB_WRITE_4(GFFB_PGRAPH + 0x0140, 0x00000000);
	GFFB_WRITE_4(GFFB_PGRAPH + 0x0100, 0xFFFFFFFF);
	GFFB_WRITE_4(GFFB_PGRAPH + 0x0144, 0x10010100);
	GFFB_WRITE_4(GFFB_PGRAPH + 0x0714, 0xFFFFFFFF);
	GFFB_WRITE_4(GFFB_PGRAPH + 0x0720, 0x00000001);
	/*
	 * xf86_video_nv does this in two writes,
	 * not sure if they can be combined
	 */
	foo = GFFB_READ_4(GFFB_PGRAPH + 0x0710);
	GFFB_WRITE_4(GFFB_PGRAPH + 0x0710, foo & 0x0007ff00);
	foo = GFFB_READ_4(GFFB_PGRAPH + 0x0710);
	GFFB_WRITE_4(GFFB_PGRAPH + 0x0710, foo | 0x00020100);

	/* NV_ARCH_10 */
	GFFB_WRITE_4(GFFB_PGRAPH + 0x0084, 0x00118700);
	GFFB_WRITE_4(GFFB_PGRAPH + 0x0088, 0x24E00810);
	GFFB_WRITE_4(GFFB_PGRAPH + 0x008C, 0x55DE0030);

	for(i = 0; i < 128; i += 4) {
		GFFB_WRITE_4(GFFB_PGRAPH + 0x0B00 + i,
		    GFFB_READ_4(GFFB_PFB + 0x0240 + i));
	}

	GFFB_WRITE_4(GFFB_PGRAPH + 0x640, 0);
	GFFB_WRITE_4(GFFB_PGRAPH + 0x644, 0);
	GFFB_WRITE_4(GFFB_PGRAPH + 0x684, sc->sc_vramsize - 1);
	GFFB_WRITE_4(GFFB_PGRAPH + 0x688, sc->sc_vramsize - 1);

	GFFB_WRITE_4(GFFB_PGRAPH + 0x0810, 0x00000000);
	GFFB_WRITE_4(GFFB_PGRAPH + 0x0608, 0xFFFFFFFF);

	GFFB_WRITE_4(GFFB_PGRAPH + 0x053C, 0);
	GFFB_WRITE_4(GFFB_PGRAPH + 0x0540, 0);
	GFFB_WRITE_4(GFFB_PGRAPH + 0x0544, 0x00007FFF);
	GFFB_WRITE_4(GFFB_PGRAPH + 0x0548, 0x00007FFF);

	GFFB_WRITE_4(GFFB_CMDSTART, 0x00000002);
	GFFB_WRITE_4(GFFB_FIFO_GET, 0);
	sc->sc_put = 0;
	sc->sc_current = 0;
	sc->sc_free = 0x2000;

	for(i = 0; i < SKIPS; i++)
		gffb_dmanext(sc, 0);
	
	gffb_dmanext(sc, 0x00040000);
	gffb_dmanext(sc, 0x80000010);
	gffb_dmanext(sc, 0x00042000);
	gffb_dmanext(sc, 0x80000011);
	gffb_dmanext(sc, 0x00044000);
	gffb_dmanext(sc, 0x80000012);
	gffb_dmanext(sc, 0x00046000);
	gffb_dmanext(sc, 0x80000013);
	gffb_dmanext(sc, 0x00048000);
	gffb_dmanext(sc, 0x80000014);
	gffb_dmanext(sc, 0x0004A000);
	gffb_dmanext(sc, 0x80000015);
	gffb_dmanext(sc, 0x0004C000);
	gffb_dmanext(sc, 0x80000016);
	gffb_dmanext(sc, 0x0004E000);
	gffb_dmanext(sc, 0x80000017);
	sc->sc_free = 0x2000 - sc->sc_current;

	gffb_dmastart(sc, SURFACE_FORMAT, 4);
	gffb_dmanext(sc, SURFACE_FORMAT_DEPTH8);
	gffb_dmanext(sc, sc->sc_stride | (sc->sc_stride << 16));
	gffb_dmanext(sc, 0x2000);	/* src offset */
	gffb_dmanext(sc, 0x2000);	/* dst offset */

	gffb_dmastart(sc, RECT_FORMAT, 1);
	gffb_dmanext(sc, RECT_FORMAT_DEPTH8);

	gffb_dmastart(sc, PATTERN_FORMAT, 1);
	gffb_dmanext(sc, PATTERN_FORMAT_DEPTH8);

	gffb_dmastart(sc, PATTERN_COLOR_0, 4);
	gffb_dmanext(sc, 0xffffffff);
	gffb_dmanext(sc, 0xffffffff);
	gffb_dmanext(sc, 0xffffffff);
	gffb_dmanext(sc, 0xffffffff);
	
	gffb_dmastart(sc, ROP_SET, 1);
	gffb_dmanext(sc, 0xcc);
	sc->sc_rop = 0xcc;

	gffb_dma_kickoff(sc);
	gffb_sync(sc);
	DPRINTF("put %x current %x\n", sc->sc_put, sc->sc_current);
	
}

static void
gffb_rop(struct gffb_softc *sc, int rop)
{
	if (rop == sc->sc_rop)
		return;
	sc->sc_rop = rop;
	gffb_dmastart(sc, ROP_SET, 1);
	gffb_dmanext(sc, rop);
}	

static void
gffb_rectfill(struct gffb_softc *sc, int x, int y, int wi, int he,
     uint32_t colour)
{
	mutex_enter(&sc->sc_lock);
	gffb_rop(sc, 0xcc);

	gffb_dmastart(sc, RECT_SOLID_COLOR, 1);
	gffb_dmanext(sc, colour);

	gffb_dmastart(sc, RECT_SOLID_RECTS(0), 2);
	gffb_dmanext(sc, (x << 16) | y);
	gffb_dmanext(sc, (wi << 16) | he);
	gffb_dma_kickoff(sc);
	mutex_exit(&sc->sc_lock);
}

static void
gffb_bitblt(void *cookie, int xs, int ys, int xd, int yd,
    int wi, int he, int rop)
{
	struct gffb_softc *sc = cookie;

	mutex_enter(&sc->sc_lock);

	gffb_rop(sc, rop);

	gffb_dmastart(sc, BLIT_POINT_SRC, 3);
	gffb_dmanext(sc, (ys << 16) | xs);
	gffb_dmanext(sc, (yd << 16) | xd);
	gffb_dmanext(sc, (he << 16) | wi);
	gffb_dma_kickoff(sc);
	mutex_exit(&sc->sc_lock);
}

static void
gffb_cursor(void *cookie, int on, int row, int col)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;
	struct gffb_softc *sc = scr->scr_cookie;
	int x, y, wi, he;
	
	wi = ri->ri_font->fontwidth;
	he = ri->ri_font->fontheight;
	
	if (sc->sc_mode == WSDISPLAYIO_MODE_EMUL) {
		x = ri->ri_ccol * wi + ri->ri_xorigin;
		y = ri->ri_crow * he + ri->ri_yorigin;
		if (ri->ri_flg & RI_CURSOR) {
			gffb_bitblt(sc, x, y, x, y, wi, he, 0x33);
			ri->ri_flg &= ~RI_CURSOR;
		}
		ri->ri_crow = row;
		ri->ri_ccol = col;
		if (on) {
			x = ri->ri_ccol * wi + ri->ri_xorigin;
			y = ri->ri_crow * he + ri->ri_yorigin;
			gffb_bitblt(sc, x, y, x, y, wi, he, 0x33);
			ri->ri_flg |= RI_CURSOR;
		}
	} else {
		scr->scr_ri.ri_crow = row;
		scr->scr_ri.ri_ccol = col;
		scr->scr_ri.ri_flg &= ~RI_CURSOR;
	}

}

static void
gffb_putchar(void *cookie, int row, int col, u_int c, long attr)
{
	struct rasops_info *ri = cookie;
	struct wsdisplay_font *font = PICK_FONT(ri, c);
	struct vcons_screen *scr = ri->ri_hw;
	struct gffb_softc *sc = scr->scr_cookie;
	int x, y, wi, he, rv = GC_NOPE;
	uint32_t bg;

	if (sc->sc_mode != WSDISPLAYIO_MODE_EMUL) 
		return;

	if (!CHAR_IN_FONT(c, font))
		return;

	wi = font->fontwidth;
	he = font->fontheight;
	
	x = ri->ri_xorigin + col * wi;
	y = ri->ri_yorigin + row * he;
	bg = ri->ri_devcmap[(attr >> 16) & 0xf];
	
	if (c == 0x20) {
		gffb_rectfill(sc, x, y, wi, he, bg);
		return;
	}
	rv = glyphcache_try(&sc->sc_gc, c, x, y, attr);
	if (rv == GC_OK)
		return;

	mutex_enter(&sc->sc_lock);
	gffb_sync(sc);
	sc->sc_putchar(cookie, row, col, c, attr);
	membar_sync();
	mutex_exit(&sc->sc_lock);

	if (rv == GC_ADD) {
		glyphcache_add(&sc->sc_gc, c, x, y);
	}
}

static void
gffb_copycols(void *cookie, int row, int srccol, int dstcol, int ncols)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;
	struct gffb_softc *sc = scr->scr_cookie;
	int32_t xs, xd, y, width, height;
	
	if ((sc->sc_locked == 0) && (sc->sc_mode == WSDISPLAYIO_MODE_EMUL)) {
		xs = ri->ri_xorigin + ri->ri_font->fontwidth * srccol;
		xd = ri->ri_xorigin + ri->ri_font->fontwidth * dstcol;
		y = ri->ri_yorigin + ri->ri_font->fontheight * row;
		width = ri->ri_font->fontwidth * ncols;
		height = ri->ri_font->fontheight;
		gffb_bitblt(sc, xs, y, xd, y, width, height, 0xcc);
	}
}

static void
gffb_erasecols(void *cookie, int row, int startcol, int ncols, long fillattr)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;
	struct gffb_softc *sc = scr->scr_cookie;
	int32_t x, y, width, height, fg, bg, ul;
	
	if ((sc->sc_locked == 0) && (sc->sc_mode == WSDISPLAYIO_MODE_EMUL)) {
		x = ri->ri_xorigin + ri->ri_font->fontwidth * startcol;
		y = ri->ri_yorigin + ri->ri_font->fontheight * row;
		width = ri->ri_font->fontwidth * ncols;
		height = ri->ri_font->fontheight;
		rasops_unpack_attr(fillattr, &fg, &bg, &ul);

		gffb_rectfill(sc, x, y, width, height, ri->ri_devcmap[bg]);
	}
}

static void
gffb_copyrows(void *cookie, int srcrow, int dstrow, int nrows)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;
	struct gffb_softc *sc = scr->scr_cookie;
	int32_t x, ys, yd, width, height;

	if ((sc->sc_locked == 0) && (sc->sc_mode == WSDISPLAYIO_MODE_EMUL)) {
		x = ri->ri_xorigin;
		ys = ri->ri_yorigin + ri->ri_font->fontheight * srcrow;
		yd = ri->ri_yorigin + ri->ri_font->fontheight * dstrow;
		width = ri->ri_emuwidth;
		height = ri->ri_font->fontheight * nrows;
		gffb_bitblt(sc, x, ys, x, yd, width, height, 0xcc);
	}
}

static void
gffb_eraserows(void *cookie, int row, int nrows, long fillattr)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;
	struct gffb_softc *sc = scr->scr_cookie;
	int32_t x, y, width, height, fg, bg, ul;
	
	if ((sc->sc_locked == 0) && (sc->sc_mode == WSDISPLAYIO_MODE_EMUL)) {
		x = ri->ri_xorigin;
		y = ri->ri_yorigin + ri->ri_font->fontheight * row;
		width = ri->ri_emuwidth;
		height = ri->ri_font->fontheight * nrows;
		rasops_unpack_attr(fillattr, &fg, &bg, &ul);

		gffb_rectfill(sc, x, y, width, height, ri->ri_devcmap[bg]);
	}
}
