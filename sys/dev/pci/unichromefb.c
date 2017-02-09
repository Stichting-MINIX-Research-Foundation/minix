/* $NetBSD: unichromefb.c,v 1.18 2011/01/22 15:14:28 cegger Exp $ */

/*-
 * Copyright (c) 2006, 2008 Jared D. McNeill <jmcneill@invisible.ca>
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

/*
 * Copyright 1998-2006 VIA Technologies, Inc. All Rights Reserved.
 * Copyright 2001-2006 S3 Graphics, Inc. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHOR(S) OR COPYRIGHT HOLDER(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: unichromefb.c,v 1.18 2011/01/22 15:14:28 cegger Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <sys/bus.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>
#include <dev/pci/pciio.h>

#include <dev/wscons/wsdisplayvar.h>
#include <dev/wscons/wsconsio.h>
#include <dev/wsfont/wsfont.h>
#include <dev/rasops/rasops.h>
#include <dev/wscons/wsdisplay_vconsvar.h>
#include <dev/pci/wsdisplay_pci.h>

#include <dev/pci/unichromereg.h>
#include <dev/pci/unichromemode.h>
#include <dev/pci/unichromehw.h>
#include <dev/pci/unichromeconfig.h>
#include <dev/pci/unichromeaccel.h>

#include "vga.h"

#if NVGA > 0
#include <dev/ic/mc6845reg.h>
#include <dev/ic/pcdisplayvar.h>
#include <dev/ic/vgareg.h>
#include <dev/ic/vgavar.h>
#endif

/* XXX */
#define UNICHROMEFB_DEPTH	16
#define UNICHROMEFB_MODE	VIA_RES_1280X1024
#define UNICHROMEFB_WIDTH	1280
#define UNICHROMEFB_HEIGHT	1024

struct unichromefb_softc {
	device_t		sc_dev;
	struct vcons_data	sc_vd;
	void *			sc_fbbase;
	unsigned int		sc_fbaddr;
	unsigned int		sc_fbsize;
	bus_addr_t		sc_mmiobase;
	bus_size_t		sc_mmiosize;

	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;

	bus_space_tag_t		sc_memt;
	bus_space_handle_t	sc_memh;
	bus_space_tag_t		sc_apmemt;
	bus_space_handle_t	sc_apmemh;

	struct pci_attach_args	sc_pa;

	int			sc_width;
	int			sc_height;
	int			sc_depth;
	int			sc_stride;

	int			sc_wsmode;

	int			sc_accel;
};

static int unichromefb_match(device_t, cfdata_t, void *);
static void unichromefb_attach(device_t, device_t, void *);

static int unichromefb_drm_print(void *, const char *);
static int unichromefb_drm_unmap(struct unichromefb_softc *);
static int unichromefb_drm_map(struct unichromefb_softc *);

struct wsscreen_descr unichromefb_stdscreen = {
	"fb",
	0, 0,
	NULL,
	8, 16,
	WSSCREEN_WSCOLORS, NULL,
};

static int	unichromefb_ioctl(void *, void *, u_long, void *, int,
				  struct lwp *);
static paddr_t	unichromefb_mmap(void *, void *, off_t, int);

static void	unichromefb_init_screen(void *, struct vcons_screen *,
					int, long *);

/* hardware access */
static uint8_t	uni_rd(struct unichromefb_softc *, int, uint8_t);
static void	uni_wr(struct unichromefb_softc *, int, uint8_t, uint8_t);
static void	uni_wr_mask(struct unichromefb_softc *, int, uint8_t,
			    uint8_t, uint8_t);
static void	uni_wr_x(struct unichromefb_softc *, struct io_reg *, int);
static void	uni_wr_dac(struct unichromefb_softc *, uint8_t, uint8_t,
			   uint8_t, uint8_t);

/* helpers */
static struct VideoModeTable *	uni_getmode(int);
static void	uni_setmode(struct unichromefb_softc *, int, int);
static void	uni_crt_lock(struct unichromefb_softc *);
static void	uni_crt_unlock(struct unichromefb_softc *);
static void	uni_crt_enable(struct unichromefb_softc *);
static void	uni_crt_disable(struct unichromefb_softc *);
static void	uni_screen_enable(struct unichromefb_softc *);
static void	uni_screen_disable(struct unichromefb_softc *);
static void	uni_set_start(struct unichromefb_softc *);
static void	uni_set_crtc(struct unichromefb_softc *,
			     struct crt_mode_table *, int, int, int);
static void	uni_load_crtc(struct unichromefb_softc *, struct display_timing,
			      int);
static void	uni_load_reg(struct unichromefb_softc *, int, int,
			     struct io_register *, int);
static void	uni_fix_crtc(struct unichromefb_softc *);
static void	uni_load_offset(struct unichromefb_softc *, int, int, int);
static void	uni_load_fetchcnt(struct unichromefb_softc *, int, int, int);
static void	uni_load_fifo(struct unichromefb_softc *, int, int, int);
static void	uni_set_depth(struct unichromefb_softc *, int, int);
static uint32_t	uni_get_clkval(struct unichromefb_softc *, int);
static void	uni_set_vclk(struct unichromefb_softc *, uint32_t, int);
static void	uni_init_dac(struct unichromefb_softc *, int);
static void	uni_init_accel(struct unichromefb_softc *);
static void	uni_set_accel_depth(struct unichromefb_softc *);

/* graphics ops */
static void	uni_wait_idle(struct unichromefb_softc *);
static void	uni_fillrect(struct unichromefb_softc *,
			     int, int, int, int, int);
static void	uni_rectinvert(struct unichromefb_softc *,
			       int, int, int, int);
static void	uni_bitblit(struct unichromefb_softc *, int, int, int, int,
			    int, int);
static void	uni_setup_mono(struct unichromefb_softc *, int, int, int,
			       int, uint32_t, uint32_t);
#if notyet
static void	uni_cursor_show(struct unichromefb_softc *);
static void	uni_cursor_hide(struct unichromefb_softc *);
#endif

/* rasops glue */
static void	uni_copycols(void *, int, int, int, int);
static void	uni_copyrows(void *, int, int, int);
static void	uni_erasecols(void *, int, int, int, long);
static void	uni_eraserows(void *, int, int, long);
static void	uni_cursor(void *, int, int, int);
static void	uni_putchar(void *, int, int, u_int, long);

struct wsdisplay_accessops unichromefb_accessops = {
	unichromefb_ioctl,
	unichromefb_mmap,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
};

static struct vcons_screen unichromefb_console_screen;

const struct wsscreen_descr *_unichromefb_scrlist[] = {
	&unichromefb_stdscreen,
};

struct wsscreen_list unichromefb_screenlist = {
	sizeof(_unichromefb_scrlist) / sizeof(struct wsscreen_descr *),
	_unichromefb_scrlist
};

CFATTACH_DECL_NEW(unichromefb, sizeof(struct unichromefb_softc),
    unichromefb_match, unichromefb_attach, NULL, NULL);

static int
unichromefb_match(device_t parent, cfdata_t match, void *opaque)
{
	struct pci_attach_args *pa;

	pa = (struct pci_attach_args *)opaque;

	if (PCI_CLASS(pa->pa_class) != PCI_CLASS_DISPLAY ||
	    PCI_SUBCLASS(pa->pa_class) != PCI_SUBCLASS_DISPLAY_VGA)
		return 0;

	if (PCI_VENDOR(pa->pa_id) != PCI_VENDOR_VIATECH)
		return 0;

	switch (PCI_PRODUCT(pa->pa_id)) {
	case PCI_PRODUCT_VIATECH_VT3314_IG:
		return 10;	/* beat vga(4) */
	}

	return 0;
}

static void
unichromefb_attach(device_t parent, device_t self, void *opaque)
{
	struct unichromefb_softc *sc = device_private(self);
	struct pci_attach_args *pa;
	struct rasops_info *ri;
	struct wsemuldisplaydev_attach_args aa;
	uint8_t val;
	long defattr;

	pa = (struct pci_attach_args *)opaque;

	sc->sc_dev = self;
	sc->sc_width = UNICHROMEFB_WIDTH;
	sc->sc_height = UNICHROMEFB_HEIGHT;
	sc->sc_depth = UNICHROMEFB_DEPTH;
	sc->sc_stride = sc->sc_width * (sc->sc_depth / 8);

	sc->sc_wsmode = WSDISPLAYIO_MODE_EMUL;

	sc->sc_iot = pa->pa_iot;
	sc->sc_pa = *pa;

#if NVGA > 0
	/* XXX vga_cnattach claims the I/O registers that we need;
	 *     we need to nuke it here so we can take over.
	 */
	vga_cndetach();
#endif

	if (bus_space_map(sc->sc_iot, VIA_REGBASE, 0x20, 0, &sc->sc_ioh)) {
		aprint_error(": failed to map I/O registers\n");
		return;
	}

	sc->sc_apmemt = pa->pa_memt;
	val = uni_rd(sc, VIASR, SR30);
	sc->sc_fbaddr = val << 24;
	val = uni_rd(sc, VIASR, SR39);
	sc->sc_fbsize = val * (4*1024*1024);
	if (sc->sc_fbsize < 16*1024*1024 || sc->sc_fbsize > 64*1024*1024)
		sc->sc_fbsize = 16*1024*1024;
	if (bus_space_map(sc->sc_apmemt, sc->sc_fbaddr, sc->sc_fbsize,
	    BUS_SPACE_MAP_LINEAR, &sc->sc_apmemh)) {
		aprint_error(": failed to map aperture at 0x%08x/0x%x\n",
		    sc->sc_fbaddr, sc->sc_fbsize);
		return;
	}
	sc->sc_fbbase = (void *)bus_space_vaddr(sc->sc_apmemt, sc->sc_apmemh);

	if (pci_mapreg_map(pa, 0x14, PCI_MAPREG_TYPE_MEM, 0,
	    &sc->sc_memt, &sc->sc_memh, &sc->sc_mmiobase,
	    &sc->sc_mmiosize)) {
		sc->sc_accel = 0;
		aprint_error(": failed to map MMIO registers\n");
	} else {
		sc->sc_accel = 1;
	}

	aprint_naive("\n");
	aprint_normal(": VIA UniChrome frame buffer\n");

	if (sc->sc_accel)
		aprint_normal_dev(self, "MMIO @0x%08x/0x%x\n",
		    (uint32_t)sc->sc_mmiobase,
		    (uint32_t)sc->sc_mmiosize);

	ri = &unichromefb_console_screen.scr_ri;
	memset(ri, 0, sizeof(struct rasops_info));

	vcons_init(&sc->sc_vd, sc, &unichromefb_stdscreen,
	    &unichromefb_accessops);
	sc->sc_vd.init_screen = unichromefb_init_screen;

	uni_setmode(sc, UNICHROMEFB_MODE, sc->sc_depth);

	uni_init_dac(sc, IGA1);
	if (sc->sc_accel) {
		uni_init_accel(sc);
		uni_fillrect(sc, 0, 0, sc->sc_width, sc->sc_height, 0);
	}

	aprint_normal_dev(self, "FB @0x%08x (%dx%dx%d)\n",
	       sc->sc_fbaddr, sc->sc_width, sc->sc_height, sc->sc_depth);

	unichromefb_console_screen.scr_flags |= VCONS_SCREEN_IS_STATIC;
	vcons_init_screen(&sc->sc_vd, &unichromefb_console_screen, 1, &defattr);

	unichromefb_stdscreen.ncols = ri->ri_cols;
	unichromefb_stdscreen.nrows = ri->ri_rows;
	unichromefb_stdscreen.textops = &ri->ri_ops;
	unichromefb_stdscreen.capabilities = ri->ri_caps;
	unichromefb_stdscreen.modecookie = NULL;

	wsdisplay_cnattach(&unichromefb_stdscreen, ri, 0, 0, defattr);

	aa.console = 1; /* XXX */
	aa.scrdata = &unichromefb_screenlist;
	aa.accessops = &unichromefb_accessops;
	aa.accesscookie = &sc->sc_vd;

	config_found(self, &aa, wsemuldisplaydevprint);

	config_found_ia(self, "drm", opaque, unichromefb_drm_print);

	return;
}

static int
unichromefb_drm_print(void *opaque, const char *pnp)
{
	if (pnp)
		aprint_normal("drm at %s", pnp);

	return UNCONF;
}

static int
unichromefb_drm_unmap(struct unichromefb_softc *sc)
{
	aprint_debug_dev(sc->sc_dev, "releasing bus resources\n");

	bus_space_unmap(sc->sc_apmemt, sc->sc_apmemh, sc->sc_fbsize);
	bus_space_unmap(sc->sc_memt, sc->sc_memh, sc->sc_mmiosize);
	bus_space_unmap(sc->sc_iot, sc->sc_ioh, 0x20);

	return 0;
}

static int
unichromefb_drm_map(struct unichromefb_softc *sc)
{
	int rv;

	rv = bus_space_map(sc->sc_iot, VIA_REGBASE, 0x20, 0,
	    &sc->sc_ioh);
	if (rv) {
		aprint_error_dev(sc->sc_dev, "failed to map I/O registers\n");
		return rv;
	}
	rv = bus_space_map(sc->sc_apmemt, sc->sc_fbaddr, sc->sc_fbsize,
	    BUS_SPACE_MAP_LINEAR, &sc->sc_apmemh);
	if (rv) {
		aprint_error_dev(sc->sc_dev,
		    "failed to map aperture at 0x%08x/0x%x\n",
		    sc->sc_fbaddr, sc->sc_fbsize);
		return rv;
	}
	sc->sc_fbbase = (void *)bus_space_vaddr(sc->sc_apmemt, sc->sc_apmemh);
	rv = pci_mapreg_map(&sc->sc_pa, 0x14, PCI_MAPREG_TYPE_MEM, 0,
	    &sc->sc_memt, &sc->sc_memh, &sc->sc_mmiobase,
	    &sc->sc_mmiosize);
	if (rv) {
		aprint_error_dev(sc->sc_dev, "failed to map MMIO registers\n");
		sc->sc_accel = 0;
	}

	uni_setmode(sc, UNICHROMEFB_MODE, sc->sc_depth);
	uni_init_dac(sc, IGA1);
	if (sc->sc_accel)
		uni_init_accel(sc);

	aprint_debug_dev(sc->sc_dev, "re-acquired bus resources\n");

	return 0;
}

static int
unichromefb_ioctl(void *v, void *vs, u_long cmd, void *data, int flag,
		  struct lwp *l)
{
	struct vcons_data *vd;
	struct unichromefb_softc *sc;
	struct wsdisplay_fbinfo *fb;

	vd = (struct vcons_data *)v;
	sc = (struct unichromefb_softc *)vd->cookie;

	switch (cmd) {
	case WSDISPLAYIO_GTYPE:
		*(u_int *)data = WSDISPLAY_TYPE_PCIMISC;
		return 0;
	case WSDISPLAYIO_GINFO:
		if (vd->active != NULL) {
			fb = (struct wsdisplay_fbinfo *)data;
			fb->width = sc->sc_width;
			fb->height = sc->sc_height;
			fb->depth = sc->sc_depth;
			fb->cmsize = 256;
			return 0;
		} else
			return ENODEV;
	case WSDISPLAYIO_GVIDEO:
			return ENODEV;
	case WSDISPLAYIO_SVIDEO:
			return ENODEV;
	case WSDISPLAYIO_GETCMAP:
			return EINVAL;
	case WSDISPLAYIO_PUTCMAP:
			return EINVAL;
	case WSDISPLAYIO_LINEBYTES:
		*(u_int *)data = sc->sc_stride;
		return 0;
	case WSDISPLAYIO_SMODE: {
		int new_mode = *(int *)data;
		if (new_mode != sc->sc_wsmode) {
			sc->sc_wsmode = new_mode;
			switch (new_mode) {
			case WSDISPLAYIO_MODE_EMUL:
				unichromefb_drm_map(sc);
				vcons_redraw_screen(vd->active);
				break;
			default:
				unichromefb_drm_unmap(sc);
				break;
			}
		}
		}
		return 0;
	case WSDISPLAYIO_SSPLASH:
		return ENODEV;
	case WSDISPLAYIO_SPROGRESS:
		return ENODEV;

	/* PCI config read/write passthrough. */
	case PCI_IOC_CFGREAD:
	case PCI_IOC_CFGWRITE:
		return (pci_devioctl(sc->sc_pa.pa_pc, sc->sc_pa.pa_tag,
		    cmd, data, flag, l));

	case WSDISPLAYIO_GET_BUSID:
		return wsdisplayio_busid_pci(sc->sc_dev,
		    sc->sc_pa.pa_pc, sc->sc_pa.pa_tag, data);

	}

	return EPASSTHROUGH;
}

static paddr_t
unichromefb_mmap(void *v, void *vs, off_t offset, int prot)
{
	return -1;
}

static void
unichromefb_init_screen(void *c, struct vcons_screen *scr, int existing,
			long *defattr)
{
	struct unichromefb_softc *sc;
	struct rasops_info *ri;

	sc = (struct unichromefb_softc *)c;
	ri = &scr->scr_ri;
	ri->ri_flg = RI_CENTER;
	ri->ri_depth = sc->sc_depth;
	ri->ri_width = sc->sc_width;
	ri->ri_height = sc->sc_height;
	ri->ri_stride = sc->sc_stride;
	ri->ri_bits = sc->sc_fbbase;
	if (existing)
		ri->ri_flg |= RI_CLEAR;

	switch (ri->ri_depth) {
	case 32:
		ri->ri_rnum = ri->ri_gnum = ri->ri_bnum = 8;
		ri->ri_rpos = 16;
		ri->ri_gpos = 8;
		ri->ri_bpos = 0;
		break;
	case 16:
		ri->ri_rnum = 5;
		ri->ri_gnum = 6;
		ri->ri_bnum = 5;
		ri->ri_rpos = 11;
		ri->ri_gpos = 5;
		ri->ri_bpos = 0;
		break;
	}

	rasops_init(ri, sc->sc_height / 16, sc->sc_width / 8);
	ri->ri_caps = WSSCREEN_WSCOLORS;
	rasops_reconfig(ri, sc->sc_height / ri->ri_font->fontheight,
	    sc->sc_width / ri->ri_font->fontwidth);

	ri->ri_hw = scr;
	if (sc->sc_accel) {
		ri->ri_ops.copyrows = uni_copyrows;
		ri->ri_ops.copycols = uni_copycols;
		ri->ri_ops.eraserows = uni_eraserows;
		ri->ri_ops.erasecols = uni_erasecols;
		ri->ri_ops.cursor = uni_cursor;
		ri->ri_ops.putchar = uni_putchar;
	}

	return;
}

/*
 * hardware access
 */
static uint8_t
uni_rd(struct unichromefb_softc *sc, int off, uint8_t idx)
{
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, off, idx);
	return bus_space_read_1(sc->sc_iot, sc->sc_ioh, off + 1);
}

static void
uni_wr(struct unichromefb_softc *sc, int off, uint8_t idx, uint8_t val)
{
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, off, idx);
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, off + 1, val);
}

static void
uni_wr_mask(struct unichromefb_softc *sc, int off, uint8_t idx,
    uint8_t val, uint8_t mask)
{
	uint8_t tmp;

	bus_space_write_1(sc->sc_iot, sc->sc_ioh, off, idx);
	tmp = bus_space_read_1(sc->sc_iot, sc->sc_ioh, off + 1);
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, off + 1,
	    ((val & mask) | (tmp & ~mask)));
}

static void
uni_wr_dac(struct unichromefb_softc *sc, uint8_t idx,
    uint8_t r, uint8_t g, uint8_t b)
{
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, LUT_INDEX_WRITE, idx);
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, LUT_DATA, r);
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, LUT_DATA, g);
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, LUT_DATA, b);
}

static void
uni_wr_x(struct unichromefb_softc *sc, struct io_reg *tbl, int num)
{
	int i;
	uint8_t tmp;

	for (i = 0; i < num; i++) {
		bus_space_write_1(sc->sc_iot, sc->sc_ioh, tbl[i].port,
		    tbl[i].index);
		tmp = bus_space_read_1(sc->sc_iot, sc->sc_ioh,
		    tbl[i].port + 1);
		tmp = (tmp & (~tbl[i].mask)) | tbl[i].value;
		bus_space_write_1(sc->sc_iot, sc->sc_ioh, tbl[i].index + 1,
		    tmp);
	}
}

/*
 * helpers
 */
static struct VideoModeTable *
uni_getmode(int mode)
{
	int i;

	for (i = 0; i < NUM_TOTAL_MODETABLE; i++)
		if (CLE266Modes[i].ModeIndex == mode)
			return &CLE266Modes[i];

	return NULL;
}

static void
uni_setmode(struct unichromefb_softc *sc, int idx, int bpp)
{
	struct VideoModeTable *vtbl;
	struct crt_mode_table *crt;
	int i;

	/* XXX */
	vtbl = uni_getmode(idx);
	if (vtbl == NULL)
		panic("%s: unsupported mode: %d\n",
		    device_xname(sc->sc_dev), idx);

	crt = vtbl->crtc;

	uni_screen_disable(sc);

	(void)bus_space_read_1(sc->sc_iot, sc->sc_ioh, VIAStatus);
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, VIAAR, 0);

	/* XXX assume CN900 for now */
	uni_wr_x(sc, CN900_ModeXregs, NUM_TOTAL_CN900_ModeXregs);

	uni_crt_disable(sc);

	/* Fill VPIT params */
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, VIAWMisc, VPIT.Misc);

	/* Write sequencer */
	for (i = 1; i <= StdSR; i++) {
		bus_space_write_1(sc->sc_iot, sc->sc_ioh, VIASR, i);
		bus_space_write_1(sc->sc_iot, sc->sc_ioh, VIASR + 1,
		    VPIT.SR[i - 1]);
	}

	uni_set_start(sc);

	uni_set_crtc(sc, crt, idx, bpp / 8, IGA1);

	for (i = 0; i < StdGR; i++) {
		bus_space_write_1(sc->sc_iot, sc->sc_ioh, VIAGR, i);
		bus_space_write_1(sc->sc_iot, sc->sc_ioh, VIAGR + 1,
		    VPIT.GR[i]);
	}

	for (i = 0; i < StdAR; i++) {
		(void)bus_space_read_1(sc->sc_iot, sc->sc_ioh, VIAStatus);
		bus_space_write_1(sc->sc_iot, sc->sc_ioh, VIAAR, i);
		bus_space_write_1(sc->sc_iot, sc->sc_ioh, VIAAR,
		    VPIT.AR[i]);
	}

	(void)bus_space_read_1(sc->sc_iot, sc->sc_ioh, VIAStatus);
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, VIAAR, 0x20);

	uni_set_crtc(sc, crt, idx, bpp / 8, IGA1);
	/* set crt output path */
	uni_wr_mask(sc, VIASR, SR16, 0x00, BIT6);

	uni_crt_enable(sc);
	uni_screen_enable(sc);

	return;
}

static void
uni_crt_lock(struct unichromefb_softc *sc)
{
	uni_wr_mask(sc, VIACR, CR11, BIT7, BIT7);
}

static void
uni_crt_unlock(struct unichromefb_softc *sc)
{
	uni_wr_mask(sc, VIACR, CR11, 0, BIT7);
	uni_wr_mask(sc, VIACR, CR47, 0, BIT0);
}

static void
uni_crt_enable(struct unichromefb_softc *sc)
{
	uni_wr_mask(sc, VIACR, CR36, 0, BIT5+BIT4);
}

static void
uni_crt_disable(struct unichromefb_softc *sc)
{
	uni_wr_mask(sc, VIACR, CR36, BIT5+BIT4, BIT5+BIT4);
}

static void
uni_screen_enable(struct unichromefb_softc *sc)
{
	uni_wr_mask(sc, VIASR, SR01, 0, BIT5);
}

static void
uni_screen_disable(struct unichromefb_softc *sc)
{
	uni_wr_mask(sc, VIASR, SR01, 0x20, BIT5);
}

static void
uni_set_start(struct unichromefb_softc *sc)
{
	uni_crt_unlock(sc);

	uni_wr(sc, VIACR, CR0C, 0x00);
	uni_wr(sc, VIACR, CR0D, 0x00);
	uni_wr(sc, VIACR, CR34, 0x00);
	uni_wr_mask(sc, VIACR, CR48, 0x00, BIT0 + BIT1);

	uni_wr(sc, VIACR, CR62, 0x00);
	uni_wr(sc, VIACR, CR63, 0x00);
	uni_wr(sc, VIACR, CR64, 0x00);
	uni_wr(sc, VIACR, CRA3, 0x00);

	uni_crt_lock(sc);
}

static void
uni_set_crtc(struct unichromefb_softc *sc, struct crt_mode_table *ctbl,
    int mode, int bpp_byte, int iga)
{
	struct VideoModeTable *vtbl;
	struct display_timing crtreg;
	int i;
	int index;
	int haddr, vaddr;
	uint8_t val;
	uint32_t pll_d_n;

	index = 0;

	vtbl = uni_getmode(mode);
	for (i = 0; i < vtbl->mode_array; i++) {
		index = i;
		if (ctbl[i].refresh_rate == 60)
			break;
	}

	crtreg = ctbl[index].crtc;

	haddr = crtreg.hor_addr;
	vaddr = crtreg.ver_addr;

	val = bus_space_read_1(sc->sc_iot, sc->sc_ioh, VIARMisc);
	if (ctbl[index].h_sync_polarity == NEGATIVE) {
		if (ctbl[index].v_sync_polarity == NEGATIVE)
			bus_space_write_1(sc->sc_iot, sc->sc_ioh, VIAWMisc,
			    (val & (~(BIT6+BIT7))) | (BIT6+BIT7));
		else
			bus_space_write_1(sc->sc_iot, sc->sc_ioh, VIAWMisc,
			    (val & (~(BIT6+BIT7))) | (BIT6));
	} else {
		if (ctbl[index].v_sync_polarity == NEGATIVE)
			bus_space_write_1(sc->sc_iot, sc->sc_ioh, VIAWMisc,
			    (val & (~(BIT6+BIT7))) | (BIT7));
		else
			bus_space_write_1(sc->sc_iot, sc->sc_ioh, VIAWMisc,
			    (val & (~(BIT6+BIT7))));
	}

	if (iga == IGA1) {
		uni_crt_unlock(sc);
		uni_wr(sc, VIACR, CR09, 0x00);
		uni_wr_mask(sc, VIACR, CR11, 0x00, BIT4+BIT5+BIT6);
		uni_wr_mask(sc, VIACR, CR17, 0x00, BIT7);
	}

	uni_load_crtc(sc, crtreg, iga);
	uni_fix_crtc(sc);
	uni_crt_lock(sc);
	uni_wr_mask(sc, VIACR, CR17, 0x80, BIT7);

	uni_load_offset(sc, haddr, bpp_byte, iga);
	uni_load_fetchcnt(sc, haddr, bpp_byte, iga);
	uni_load_fifo(sc, iga, haddr, vaddr);

	uni_set_depth(sc, bpp_byte, iga);
	pll_d_n = uni_get_clkval(sc, ctbl[index].clk);
	uni_set_vclk(sc, pll_d_n, iga);
}

static void
uni_load_crtc(struct unichromefb_softc *sc,
    struct display_timing device_timing, int iga)
{
	int regnum, val;
	struct io_register *reg;
	int i;

	regnum = val = 0;
	reg = NULL;

	uni_crt_unlock(sc);

	for (i = 0; i < 12; i++) {
		switch (iga) {
		case IGA1:
			switch (i) {
			case H_TOTAL_INDEX:
				val = IGA1_HOR_TOTAL_FORMULA(
				    device_timing.hor_total);
				regnum = iga1_crtc_reg.hor_total.reg_num;
				reg = iga1_crtc_reg.hor_total.reg;
				break;
			case H_ADDR_INDEX:
				val = IGA1_HOR_ADDR_FORMULA(
				    device_timing.hor_addr);
				regnum = iga1_crtc_reg.hor_addr.reg_num;
				reg = iga1_crtc_reg.hor_addr.reg;
				break;
			case H_BLANK_START_INDEX:
				val = IGA1_HOR_BLANK_START_FORMULA(
				    device_timing.hor_blank_start);
				regnum = iga1_crtc_reg.hor_blank_start.reg_num;
				reg = iga1_crtc_reg.hor_blank_start.reg;
				break;
			case H_BLANK_END_INDEX:
				val = IGA1_HOR_BLANK_END_FORMULA(
				    device_timing.hor_blank_start,
				    device_timing.hor_blank_end);
				regnum = iga1_crtc_reg.hor_blank_end.reg_num;
				reg = iga1_crtc_reg.hor_blank_end.reg;
				break;
			case H_SYNC_START_INDEX:
				val = IGA1_HOR_SYNC_START_FORMULA(
				    device_timing.hor_sync_start);
				regnum = iga1_crtc_reg.hor_sync_start.reg_num;
				reg = iga1_crtc_reg.hor_sync_start.reg;
				break;
			case H_SYNC_END_INDEX:
				val = IGA1_HOR_SYNC_END_FORMULA(
				    device_timing.hor_sync_start,
				    device_timing.hor_sync_end);
				regnum = iga1_crtc_reg.hor_sync_end.reg_num;
				reg = iga1_crtc_reg.hor_sync_end.reg;
				break;
			case V_TOTAL_INDEX:
				val = IGA1_VER_TOTAL_FORMULA(
				    device_timing.ver_total);
				regnum = iga1_crtc_reg.ver_total.reg_num;
				reg = iga1_crtc_reg.ver_total.reg;
				break;
			case V_ADDR_INDEX:
				val = IGA1_VER_ADDR_FORMULA(
				    device_timing.ver_addr);
				regnum = iga1_crtc_reg.ver_addr.reg_num;
				reg = iga1_crtc_reg.ver_addr.reg;
				break;
			case V_BLANK_START_INDEX:
				val = IGA1_VER_BLANK_START_FORMULA(
				    device_timing.ver_blank_start);
				regnum = iga1_crtc_reg.ver_blank_start.reg_num;
				reg = iga1_crtc_reg.ver_blank_start.reg;
				break;
			case V_BLANK_END_INDEX:
				val = IGA1_VER_BLANK_END_FORMULA(
				    device_timing.ver_blank_start,
				    device_timing.ver_blank_end);
				regnum = iga1_crtc_reg.ver_blank_end.reg_num;
				reg = iga1_crtc_reg.ver_blank_end.reg;
				break;
			case V_SYNC_START_INDEX:
				val = IGA1_VER_SYNC_START_FORMULA(
				    device_timing.ver_sync_start);
				regnum = iga1_crtc_reg.ver_sync_start.reg_num;
				reg = iga1_crtc_reg.ver_sync_start.reg;
				break;
			case V_SYNC_END_INDEX:
				val = IGA1_VER_SYNC_END_FORMULA(
				    device_timing.ver_sync_start,
				    device_timing.ver_sync_end);
				regnum = iga1_crtc_reg.ver_sync_end.reg_num;
				reg = iga1_crtc_reg.ver_sync_end.reg;
				break;
			default:
				aprint_error_dev(sc->sc_dev,
				    "unknown index %d while setting up CRTC\n",
				    i);
				break;
			}
			break;
		case IGA2:
			aprint_error_dev(sc->sc_dev, "%s: IGA2 not supported\n",
			    __func__);
			break;
		}

		uni_load_reg(sc, val, regnum, reg, VIACR);
	}

	uni_crt_lock(sc);
}

static void
uni_load_reg(struct unichromefb_softc *sc, int timing, int regnum,
    struct io_register *reg, int type)
{
	int regmask, bitnum, data;
	int i, j;
	int shift_next_reg;
	int startidx, endidx, cridx;
	uint16_t getbit;

	bitnum = 0;

	for (i = 0; i < regnum; i++) {
		regmask = data = 0;
		startidx = reg[i].start_bit;
		endidx = reg[i].end_bit;
		cridx = reg[i].io_addr;

		shift_next_reg = bitnum;

		for (j = startidx; j <= endidx; j++) {
			regmask = regmask | (BIT0 << j);
			getbit = (timing & (BIT0 << bitnum));
			data = data | ((getbit >> shift_next_reg) << startidx);
			++bitnum;
		}

		if (type == VIACR)
			uni_wr_mask(sc, VIACR, cridx, data, regmask);
		else
			uni_wr_mask(sc, VIASR, cridx, data, regmask);
	}

	return;
}

static void
uni_fix_crtc(struct unichromefb_softc *sc)
{
	uni_wr_mask(sc, VIACR, CR03, 0x80, BIT7);
	uni_wr(sc, VIACR, CR18, 0xff);
	uni_wr_mask(sc, VIACR, CR07, 0x10, BIT4);
	uni_wr_mask(sc, VIACR, CR09, 0x40, BIT6);
	uni_wr_mask(sc, VIACR, CR35, 0x10, BIT4);
	uni_wr_mask(sc, VIACR, CR33, 0x06, BIT0+BIT1+BIT2);
	uni_wr(sc, VIACR, CR17, 0xe3);
	uni_wr(sc, VIACR, CR08, 0x00);
	uni_wr(sc, VIACR, CR14, 0x00);

	return;
}

static void
uni_load_offset(struct unichromefb_softc *sc, int haddr, int bpp, int iga)
{

	switch (iga) {
	case IGA1:
		uni_load_reg(sc,
		    IGA1_OFFSET_FORMULA(haddr, bpp),
		    offset_reg.iga1_offset_reg.reg_num,
		    offset_reg.iga1_offset_reg.reg,
		    VIACR);
		break;
	default:
		aprint_error_dev(sc->sc_dev, "%s: only IGA1 is supported\n",
		    __func__);
		break;
	}

	return;
}

static void
uni_load_fetchcnt(struct unichromefb_softc *sc, int haddr, int bpp, int iga)
{

	switch (iga) {
	case IGA1:
		uni_load_reg(sc,
		    IGA1_FETCH_COUNT_FORMULA(haddr, bpp),
		    fetch_count_reg.iga1_fetch_count_reg.reg_num,
		    fetch_count_reg.iga1_fetch_count_reg.reg,
		    VIASR);
		break;
	default:
		aprint_error_dev(sc->sc_dev, "%s: only IGA1 is supported\n",
		    __func__);
		break;
	}

	return;
}

static void
uni_load_fifo(struct unichromefb_softc *sc, int iga, int horact, int veract)
{
	int val, regnum;
	struct io_register *reg;
	int iga1_fifo_max_depth, iga1_fifo_threshold;
	int iga1_fifo_high_threshold, iga1_display_queue_expire_num;

	reg = NULL;
	iga1_fifo_max_depth = iga1_fifo_threshold = 0;
	iga1_fifo_high_threshold = iga1_display_queue_expire_num = 0;

	switch (iga) {
	case IGA1:
		/* XXX if (type == CN900) { */
		iga1_fifo_max_depth = CN900_IGA1_FIFO_MAX_DEPTH;
		iga1_fifo_threshold = CN900_IGA1_FIFO_THRESHOLD;
		iga1_fifo_high_threshold = CN900_IGA1_FIFO_HIGH_THRESHOLD;
		if (horact > 1280 && veract > 1024)
			iga1_display_queue_expire_num = 16;
		else
			iga1_display_queue_expire_num =
			    CN900_IGA1_DISPLAY_QUEUE_EXPIRE_NUM;
		/* XXX } */

		/* set display FIFO depth select */
		val = IGA1_FIFO_DEPTH_SELECT_FORMULA(iga1_fifo_max_depth);
		regnum =
		    display_fifo_depth_reg.iga1_fifo_depth_select_reg.reg_num;
		reg = display_fifo_depth_reg.iga1_fifo_depth_select_reg.reg;
		uni_load_reg(sc, val, regnum, reg, VIASR);

		/* set display FIFO threshold select */
		val = IGA1_FIFO_THRESHOLD_FORMULA(iga1_fifo_threshold);
		regnum = fifo_threshold_select_reg.iga1_fifo_threshold_select_reg.reg_num;
		reg = fifo_threshold_select_reg.iga1_fifo_threshold_select_reg.reg;
		uni_load_reg(sc, val, regnum, reg, VIASR);

		/* set display FIFO high threshold select */
		val = IGA1_FIFO_HIGH_THRESHOLD_FORMULA(iga1_fifo_high_threshold);
		regnum = fifo_high_threshold_select_reg.iga1_fifo_high_threshold_select_reg.reg_num;
		reg = fifo_high_threshold_select_reg.iga1_fifo_high_threshold_select_reg.reg;
		uni_load_reg(sc, val, regnum, reg, VIASR);

		/* set display queue expire num */
		val = IGA1_DISPLAY_QUEUE_EXPIRE_NUM_FORMULA(iga1_display_queue_expire_num);
		regnum = display_queue_expire_num_reg.iga1_display_queue_expire_num_reg.reg_num;
		reg = display_queue_expire_num_reg.iga1_display_queue_expire_num_reg.reg;
		uni_load_reg(sc, val, regnum, reg, VIASR);

		break;
	default:
		aprint_error_dev(sc->sc_dev, "%s: only IGA1 is supported\n",
		    __func__);
		break;
	}

	return;
}

static void
uni_set_depth(struct unichromefb_softc *sc, int bpp, int iga)
{
	switch (iga) {
	case IGA1:
		switch (bpp) {
		case MODE_32BPP:
			uni_wr_mask(sc, VIASR, SR15, 0xae, 0xfe);
			break;
		case MODE_16BPP:
			uni_wr_mask(sc, VIASR, SR15, 0xb6, 0xfe);
			break;
		case MODE_8BPP:
			uni_wr_mask(sc, VIASR, SR15, 0x22, 0xfe);
			break;
		default:
			aprint_error_dev(sc->sc_dev,
			    "%s: mode (%d) unsupported\n", __func__, bpp);
		}
		break;
	default:
		aprint_error_dev(sc->sc_dev, "%s: only IGA1 is supported\n",
		    __func__);
		break;
	}
}

static uint32_t
uni_get_clkval(struct unichromefb_softc *sc, int clk)
{
	int i;

	for (i = 0; i < NUM_TOTAL_PLL_TABLE; i++) {
		if (clk == pll_value[i].clk) {
			/* XXX only CN900 supported for now */
			return pll_value[i].k800_pll;
		}
	}

	aprint_error_dev(sc->sc_dev, "can't find matching PLL value\n");

	return 0;
}

static void
uni_set_vclk(struct unichromefb_softc *sc, uint32_t clk, int iga)
{
	uint8_t val;

	/* hardware reset on */
	uni_wr_mask(sc, VIACR, CR17, 0x00, BIT7);

	switch (iga) {
	case IGA1:
		/* XXX only CN900 is supported */
		uni_wr(sc, VIASR, SR44, clk / 0x10000);
		uni_wr(sc, VIASR, SR45, (clk & 0xffff) / 0x100);
		uni_wr(sc, VIASR, SR46, clk % 0x100);
		break;
	default:
		aprint_error_dev(sc->sc_dev, "%s: only IGA1 is supported\n",
		    __func__);
		break;
	}

	/* hardware reset off */
	uni_wr_mask(sc, VIACR, CR17, 0x80, BIT7);

	/* reset pll */
	switch (iga) {
	case IGA1:
		uni_wr_mask(sc, VIASR, SR40, 0x02, BIT1);
		uni_wr_mask(sc, VIASR, SR40, 0x00, BIT1);
		break;
	}

	/* good to go */
	val = bus_space_read_1(sc->sc_iot, sc->sc_ioh, VIARMisc);
	val |= (BIT2+BIT3);
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, VIAWMisc, val);

	return;
}

static void
uni_init_dac(struct unichromefb_softc *sc, int iga)
{
	int i;

	/* XXX only IGA1 for now */
	uni_wr_mask(sc, VIASR, SR1A, 0x00, BIT0);
	uni_wr_mask(sc, VIASR, SR18, 0x00, BIT7+BIT6);
	for (i = 0; i < 256; i++)
		uni_wr_dac(sc, i,
		    palLUT_table[i].red, palLUT_table[i].green, palLUT_table[i].blue);

	uni_wr_mask(sc, VIASR, SR18, 0xc0, BIT7+BIT6);

	return;
}

static void
uni_init_accel(struct unichromefb_softc *sc)
{

	/* init 2D engine regs to reset 2D engine */
	MMIO_OUT32(VIA_REG_GEMODE, 0);
	MMIO_OUT32(VIA_REG_SRCPOS, 0);
	MMIO_OUT32(VIA_REG_DSTPOS, 0);
	MMIO_OUT32(VIA_REG_DIMENSION, 0);
	MMIO_OUT32(VIA_REG_PATADDR, 0);
	MMIO_OUT32(VIA_REG_FGCOLOR, 0);
	MMIO_OUT32(VIA_REG_BGCOLOR, 0);
	MMIO_OUT32(VIA_REG_CLIPTL, 0);
	MMIO_OUT32(VIA_REG_CLIPBR, 0);
	MMIO_OUT32(VIA_REG_OFFSET, 0);
	MMIO_OUT32(VIA_REG_KEYCONTROL, 0);
	MMIO_OUT32(VIA_REG_SRCBASE, 0);
	MMIO_OUT32(VIA_REG_DSTBASE, 0);
	MMIO_OUT32(VIA_REG_PITCH, 0);
	MMIO_OUT32(VIA_REG_MONOPAT1, 0);

	/* init AGP and VQ registers */
	MMIO_OUT32(VIA_REG_TRANSET, 0x00100000);
	MMIO_OUT32(VIA_REG_TRANSPACE, 0x00000000);
	MMIO_OUT32(VIA_REG_TRANSPACE, 0x00333004);
	MMIO_OUT32(VIA_REG_TRANSPACE, 0x60000000);
	MMIO_OUT32(VIA_REG_TRANSPACE, 0x61000000);
	MMIO_OUT32(VIA_REG_TRANSPACE, 0x62000000);
	MMIO_OUT32(VIA_REG_TRANSPACE, 0x63000000);
	MMIO_OUT32(VIA_REG_TRANSPACE, 0x64000000);
	MMIO_OUT32(VIA_REG_TRANSPACE, 0x7d000000);

	MMIO_OUT32(VIA_REG_TRANSET, 0xfe020000);
	MMIO_OUT32(VIA_REG_TRANSPACE, 0x00000000);

	/* disable VQ */
	MMIO_OUT32(VIA_REG_TRANSET, 0x00fe0000);
	MMIO_OUT32(VIA_REG_TRANSPACE, 0x00000004);
	MMIO_OUT32(VIA_REG_TRANSPACE, 0x40008c0f);
	MMIO_OUT32(VIA_REG_TRANSPACE, 0x44000000);
	MMIO_OUT32(VIA_REG_TRANSPACE, 0x45080c04);
	MMIO_OUT32(VIA_REG_TRANSPACE, 0x46800408);

	uni_set_accel_depth(sc);

	MMIO_OUT32(VIA_REG_SRCBASE, 0);
	MMIO_OUT32(VIA_REG_DSTBASE, 0);

	MMIO_OUT32(VIA_REG_PITCH, VIA_PITCH_ENABLE |
	    (((sc->sc_width * sc->sc_depth >> 3) >> 3) |
	    (((sc->sc_width * sc->sc_depth >> 3) >> 3) << 16)));

	return;
}

static void
uni_set_accel_depth(struct unichromefb_softc *sc)
{
	uint32_t gemode;

	gemode = MMIO_IN32(0x04) & 0xfffffcff;

	switch (sc->sc_depth) {
	case 32:
		gemode |= VIA_GEM_32bpp;
		break;
	case 16:
		gemode |= VIA_GEM_16bpp;
		break;
	default:
		gemode |= VIA_GEM_8bpp;
		break;
	}

	/* set colour depth and pitch */
	MMIO_OUT32(VIA_REG_GEMODE, gemode);

	return;
}

static void
uni_wait_idle(struct unichromefb_softc *sc)
{
	int loop = 0;

	while (!(MMIO_IN32(VIA_REG_STATUS) & VIA_VR_QUEUE_BUSY) &&
	    (loop++ < MAXLOOP))
		;

	while ((MMIO_IN32(VIA_REG_STATUS) &
	    (VIA_CMD_RGTR_BUSY | VIA_2D_ENG_BUSY | VIA_3D_ENG_BUSY)) &&
	    (loop++ < MAXLOOP))
		;

	if (loop >= MAXLOOP)
		aprint_error_dev(sc->sc_dev, "engine stall\n");

	return;
}

static void
uni_fillrect(struct unichromefb_softc *sc, int x, int y, int width,
    int height, int colour)
{

	uni_wait_idle(sc);

	MMIO_OUT32(VIA_REG_SRCPOS, 0);
	MMIO_OUT32(VIA_REG_SRCBASE, 0);
	MMIO_OUT32(VIA_REG_DSTBASE, 0);
	MMIO_OUT32(VIA_REG_PITCH, VIA_PITCH_ENABLE |
	    (((sc->sc_width * sc->sc_depth >> 3) >> 3) |
	    (((sc->sc_width * sc->sc_depth >> 3) >> 3) << 16)));
	MMIO_OUT32(VIA_REG_DSTPOS, ((y << 16) | x));
	MMIO_OUT32(VIA_REG_DIMENSION,
	    (((height - 1) << 16) | (width - 1)));
	MMIO_OUT32(VIA_REG_FGCOLOR, colour);
	MMIO_OUT32(VIA_REG_GECMD, (0x01 | 0x2000 | 0xf0 << 24));

	return;
}

static void
uni_rectinvert(struct unichromefb_softc *sc, int x, int y, int width,
    int height)
{

	uni_wait_idle(sc);

	MMIO_OUT32(VIA_REG_SRCPOS, 0);
	MMIO_OUT32(VIA_REG_SRCBASE, 0);
	MMIO_OUT32(VIA_REG_DSTBASE, 0);
	MMIO_OUT32(VIA_REG_PITCH, VIA_PITCH_ENABLE |
	    (((sc->sc_width * sc->sc_depth >> 3) >> 3) |
	    (((sc->sc_width * sc->sc_depth >> 3) >> 3) << 16)));
	MMIO_OUT32(VIA_REG_DSTPOS, ((y << 16) | x));
	MMIO_OUT32(VIA_REG_DIMENSION,
	    (((height - 1) << 16) | (width - 1)));
	MMIO_OUT32(VIA_REG_GECMD, (0x01 | 0x2000 | 0x55 << 24));

	return;
}

static void
uni_bitblit(struct unichromefb_softc *sc, int xs, int ys, int xd, int yd, int width, int height)
{
	uint32_t dir;

	dir = 0;

	if (ys < yd) {
		yd += height - 1;
		ys += height - 1;
		dir |= 0x4000;
	}

	if (xs < xd) {
		xd += width - 1;
		xs += width - 1;
		dir |= 0x8000;
	}

	uni_wait_idle(sc);

	MMIO_OUT32(VIA_REG_SRCBASE, 0);
	MMIO_OUT32(VIA_REG_DSTBASE, 0);
	MMIO_OUT32(VIA_REG_PITCH, VIA_PITCH_ENABLE |
	    (((sc->sc_width * sc->sc_depth >> 3) >> 3) |
	    (((sc->sc_width * sc->sc_depth >> 3) >> 3) << 16)));
	MMIO_OUT32(VIA_REG_SRCPOS, ys << 16 | xs);
	MMIO_OUT32(VIA_REG_DSTPOS, yd << 16 | xd);
	MMIO_OUT32(VIA_REG_DIMENSION, ((height - 1) << 16) | (width - 1));
	MMIO_OUT32(VIA_REG_GECMD, (0x01 | dir | (0xcc << 24)));

	return;
}

static void
uni_setup_mono(struct unichromefb_softc *sc, int xd, int yd, int width, int height,
    uint32_t fg, uint32_t bg)
{

	uni_wait_idle(sc);

	MMIO_OUT32(VIA_REG_SRCBASE, 0);
	MMIO_OUT32(VIA_REG_DSTBASE, 0);
	MMIO_OUT32(VIA_REG_PITCH, VIA_PITCH_ENABLE |
	    (((sc->sc_width * sc->sc_depth >> 3) >> 3) |
	    (((sc->sc_width * sc->sc_depth >> 3) >> 3) << 16)));
	MMIO_OUT32(VIA_REG_SRCPOS, 0);
	MMIO_OUT32(VIA_REG_DSTPOS, (yd << 16) | xd);
	MMIO_OUT32(VIA_REG_DIMENSION, ((height - 1) << 16) | (width - 1));
	MMIO_OUT32(VIA_REG_FGCOLOR, fg);
	MMIO_OUT32(VIA_REG_BGCOLOR, bg);
	MMIO_OUT32(VIA_REG_GECMD, 0xcc020142);

	return;
}

#if notyet
static void
uni_cursor_show(struct unichromefb_softc *sc)
{
	uint32_t val;

	val = MMIO_IN32(VIA_REG_CURSOR_MODE);
	val |= 1;
	MMIO_OUT32(VIA_REG_CURSOR_MODE, val);

	return;
}

static void
uni_cursor_hide(struct unichromefb_softc *sc)
{
	uint32_t val;

	val = MMIO_IN32(VIA_REG_CURSOR_MODE);
	val &= 0xfffffffe;
	MMIO_OUT32(VIA_REG_CURSOR_MODE, val);

	return;
}
#endif

/*
 * rasops glue
 */
static void
uni_copycols(void *opaque, int row, int srccol, int dstcol, int ncols)
{
	struct rasops_info *ri;
	struct vcons_screen *scr;
	struct unichromefb_softc *sc;
	int xs, xd, y, width, height;

	ri = (struct rasops_info *)opaque;
	scr = (struct vcons_screen *)ri->ri_hw;
	sc = (struct unichromefb_softc *)scr->scr_cookie;

	if (sc->sc_wsmode == WSDISPLAYIO_MODE_EMUL) {
		xs = ri->ri_xorigin + ri->ri_font->fontwidth * srccol;
		xd = ri->ri_xorigin + ri->ri_font->fontwidth * dstcol;
		y = ri->ri_yorigin + ri->ri_font->fontheight * row;
		width = ri->ri_font->fontwidth * ncols;
		height = ri->ri_font->fontheight;
		uni_bitblit(sc, xs, y, xd, y, width, height);
	}

	return;
}

static void
uni_copyrows(void *opaque, int srcrow, int dstrow, int nrows)
{
	struct rasops_info *ri;
	struct vcons_screen *scr;
	struct unichromefb_softc *sc;
	int x, ys, yd, width, height;

	ri = (struct rasops_info *)opaque;
	scr = (struct vcons_screen *)ri->ri_hw;
	sc = (struct unichromefb_softc *)scr->scr_cookie;

	if (sc->sc_wsmode == WSDISPLAYIO_MODE_EMUL) {
		x = ri->ri_xorigin;
		ys = ri->ri_yorigin + ri->ri_font->fontheight * srcrow;
		yd = ri->ri_yorigin + ri->ri_font->fontheight * dstrow;
		width = ri->ri_emuwidth;
		height = ri->ri_font->fontheight * nrows;
		uni_bitblit(sc, x, ys, x, yd, width, height);
	}

	return;
}

static void
uni_erasecols(void *opaque, int row, int startcol, int ncols, long fillattr)
{
	struct rasops_info *ri;
	struct vcons_screen *scr;
	struct unichromefb_softc *sc;
	int x, y, width, height, fg, bg, ul;

	ri = (struct rasops_info *)opaque;
	scr = (struct vcons_screen *)ri->ri_hw;
	sc = (struct unichromefb_softc *)scr->scr_cookie;

	if (sc->sc_wsmode == WSDISPLAYIO_MODE_EMUL) {
		x = ri->ri_xorigin + ri->ri_font->fontwidth * startcol;
		y = ri->ri_yorigin + ri->ri_font->fontheight * row;
		width = ri->ri_font->fontwidth * ncols;
		height = ri->ri_font->fontheight;
		rasops_unpack_attr(fillattr, &fg, &bg, &ul);
		uni_fillrect(sc, x, y, width, height, ri->ri_devcmap[bg]);
	}

	return;
}

static void
uni_eraserows(void *opaque, int row, int nrows, long fillattr)
{
	struct rasops_info *ri;
	struct vcons_screen *scr;
	struct unichromefb_softc *sc;
	int x, y, width, height, fg, bg, ul;

	ri = (struct rasops_info *)opaque;
	scr = (struct vcons_screen *)ri->ri_hw;
	sc = (struct unichromefb_softc *)scr->scr_cookie;

	if (sc->sc_wsmode == WSDISPLAYIO_MODE_EMUL) {
		rasops_unpack_attr(fillattr, &fg, &bg, &ul);
		if ((row == 0) && (nrows == ri->ri_rows)) {
			/* clear the whole screen */
			uni_fillrect(sc, 0, 0, ri->ri_width,
			    ri->ri_height, ri->ri_devcmap[bg]);
		} else {
			x = ri->ri_xorigin;
			y = ri->ri_yorigin + ri->ri_font->fontheight * row;
			width = ri->ri_emuwidth;
			height = ri->ri_font->fontheight * nrows;
			uni_fillrect(sc, x, y, width, height,
			    ri->ri_devcmap[bg]);
		}
	}

	return;
}

static void
uni_cursor(void *opaque, int on, int row, int col)
{
	struct rasops_info *ri;
	struct vcons_screen *scr;
	struct unichromefb_softc *sc;
	int x, y, wi, he;

	ri = (struct rasops_info *)opaque;
	scr = (struct vcons_screen *)ri->ri_hw;
	sc = (struct unichromefb_softc *)scr->scr_cookie;

	uni_wait_idle(sc);

	wi = ri->ri_font->fontwidth;
	he = ri->ri_font->fontheight;

	if (sc->sc_wsmode == WSDISPLAYIO_MODE_EMUL) {
		x = ri->ri_ccol * wi + ri->ri_xorigin;
		y = ri->ri_crow * he + ri->ri_yorigin;
		if (ri->ri_flg & RI_CURSOR) {
			uni_rectinvert(sc, x, y, wi, he);
			ri->ri_flg &= ~RI_CURSOR;
		}
		ri->ri_crow = row;
		ri->ri_ccol = col;
		if (on) {
			x = ri->ri_ccol * wi + ri->ri_xorigin;
			y = ri->ri_crow * he + ri->ri_yorigin;
			uni_rectinvert(sc, x, y, wi, he);
			ri->ri_flg |= RI_CURSOR;
		}
	} else {
		ri->ri_flg &= ~RI_CURSOR;
		ri->ri_crow = row;
		ri->ri_ccol = col;
	}

	return;
}

static void
uni_putchar(void *opaque, int row, int col, u_int c, long attr)
{
	struct rasops_info *ri;
	struct vcons_screen *scr;
	struct unichromefb_softc *sc;

	ri = (struct rasops_info *)opaque;
	scr = (struct vcons_screen *)ri->ri_hw;
	sc = (struct unichromefb_softc *)scr->scr_cookie;

	if (sc->sc_wsmode == WSDISPLAYIO_MODE_EMUL) {
		uint32_t *data;
		int fg, bg, ul, uc, i;
		int x, y, wi, he;

		wi = ri->ri_font->fontwidth;
		he = ri->ri_font->fontheight;

		if (!CHAR_IN_FONT(c, ri->ri_font))
			return;

		rasops_unpack_attr(attr, &fg, &bg, &ul);
		x = ri->ri_xorigin + col * wi;
		y = ri->ri_yorigin + row * he;
		if (c == 0x20)
			uni_fillrect(sc, x, y, wi, he, ri->ri_devcmap[bg]);
		else {
			uc = c - ri->ri_font->firstchar;
			data = (uint32_t *)((uint8_t *)ri->ri_font->data +
			    uc * ri->ri_fontscale);
			uni_setup_mono(sc, x, y, wi, he,
			    ri->ri_devcmap[fg], ri->ri_devcmap[bg]);
			for (i = 0; i < (wi * he) / 4; i++) {
				MMIO_OUT32(VIA_MMIO_BLTBASE, *data);
				data++;
			}
		}
	}

	return;
}
