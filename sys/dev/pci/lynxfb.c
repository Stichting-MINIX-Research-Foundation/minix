/*	$NetBSD: lynxfb.c,v 1.4 2012/03/14 13:41:07 nonaka Exp $	*/
/*	$OpenBSD: smfb.c,v 1.13 2011/07/21 20:36:12 miod Exp $	*/

/*
 * Copyright (c) 2012 NONAKA Kimihiro <nonaka@netbsd.org>
 * Copyright (c) 2009, 2010 Miodrag Vallat.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * SiliconMotion SM712 frame buffer driver.
 *
 * Assumes its video output is an LCD panel, in 5:6:5 mode, and fixed
 * 1024x600 resolution, depending on the system model.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: lynxfb.c,v 1.4 2012/03/14 13:41:07 nonaka Exp $");

#include "opt_wsemul.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/endian.h>
#include <sys/kauth.h>

#include <sys/bus.h>

#include <dev/ic/vgareg.h>
#include <dev/isa/isareg.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>
#include <dev/pci/pciio.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>
#include <dev/rasops/rasops.h>
#include <dev/wscons/wsdisplay_vconsvar.h>
#include <dev/pci/wsdisplay_pci.h>

#include <dev/pci/lynxfbreg.h>
#include <dev/pci/lynxfbvar.h>

#ifndef	LYNXFB_DEFAULT_WIDTH
#define	LYNXFB_DEFAULT_WIDTH	1024
#endif
#ifndef	LYNXFB_DEFAULT_HEIGHT
#define	LYNXFB_DEFAULT_HEIGHT	600
#endif
#ifndef	LYNXFB_DEFAULT_DEPTH
#define	LYNXFB_DEFAULT_DEPTH	16
#endif
#ifndef	LYNXFB_DEFAULT_STRIDE
#define	LYNXFB_DEFAULT_STRIDE	0
#endif
#ifndef	LYNXFB_DEFAULT_FLAGS
#ifdef __MIPSEL__
#define	LYNXFB_DEFAULT_FLAGS	LYNXFB_FLAG_SWAPBR
#else
#define	LYNXFB_DEFAULT_FLAGS	0
#endif
#endif

struct lynxfb_softc;

/* minimal frame buffer information, suitable for early console */
struct lynxfb {
	struct lynxfb_softc	*sc;
	void			*fbaddr;

	bus_space_tag_t		memt;
	bus_space_handle_t	memh;

	/* DPR registers */
	bus_space_tag_t		dprt;
	bus_space_handle_t	dprh;
	/* MMIO space */
	bus_space_tag_t		mmiot;
	bus_space_handle_t	mmioh;

	struct vcons_screen	vcs;
	struct wsscreen_descr	wsd;

	int			width, height, depth, stride;
	int			accel;
	int			blank;

	/* LYNXFB_FLAG_* */
	int			flags;
#define	LYNXFB_FLAG_SWAPBR	(1 << 0)
};

#define	DPR_READ(fb, reg) \
	bus_space_read_4((fb)->memt, (fb)->dprh, (reg))
#define	DPR_WRITE(fb, reg, val) \
	bus_space_write_4((fb)->memt, (fb)->dprh, (reg), (val))

struct lynxfb_softc {
	device_t		sc_dev;
	bus_space_tag_t		sc_memt;
	bus_space_handle_t	sc_memh;

	pci_chipset_tag_t	sc_pc;
	pcitag_t		sc_pcitag;

	struct lynxfb		*sc_fb;
	struct lynxfb		sc_fb_store;
	bus_addr_t		sc_fbaddr;
	bus_size_t		sc_fbsize;

	struct vcons_data	sc_vd;
	struct wsscreen_list	sc_screenlist;
	const struct wsscreen_descr *sc_screens[1];
	int			sc_mode;
};

static int	lynxfb_match_by_id(pcireg_t);
static int	lynxfb_match(device_t, cfdata_t, void *);
static void	lynxfb_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(lynxfb, sizeof(struct lynxfb_softc),
    lynxfb_match, lynxfb_attach, NULL, NULL);

static int	lynxfb_ioctl(void *, void *, u_long, void *, int, struct lwp *);
static paddr_t	lynxfb_mmap(void *, void *, off_t, int);

static struct wsdisplay_accessops lynxfb_accessops = {
	lynxfb_ioctl,
	lynxfb_mmap,
	NULL,	/* alloc_screen */
	NULL,	/* free_screen */
	NULL,	/* show_screen */
	NULL,	/* load_font */
	NULL,	/* pollc */
	NULL,	/* scroll */
};

static bool	lynxfb_is_console(struct lynxfb_softc *, pcitag_t);
static void	lynxfb_init_screen(void *, struct vcons_screen *, int, long *);
static int	lynxfb_setup(struct lynxfb *);

static int	lynxfb_wait(struct lynxfb *);
static void	lynxfb_copyrect(struct lynxfb *, int, int, int, int, int, int);
static void	lynxfb_fillrect(struct lynxfb *, int, int, int, int, int);
static void	lynxfb_copyrows(void *, int, int, int);
static void	lynxfb_copycols(void *, int, int, int, int);
static void	lynxfb_erasecols(void *, int, int, int, long);
static void	lynxfb_eraserows(void *, int, int, long);
static void	lynxfb_vcons_copyrows(void *, int, int, int);
static void	lynxfb_vcons_copycols(void *, int, int, int, int);
static void	lynxfb_vcons_erasecols(void *, int, int, int, long);
static void	lynxfb_vcons_eraserows(void *, int, int, long);
static void	lynxfb_blank(struct lynxfb *, int);

static struct {
	bool is_console;
	pcitag_t tag;
	bus_space_tag_t memt;
	bus_space_handle_t memh;
	struct lynxfb fb;
	bus_addr_t fbaddr;
	bus_size_t fbsize;
} lynxfb_console;

int lynxfb_default_width = LYNXFB_DEFAULT_WIDTH;
int lynxfb_default_height = LYNXFB_DEFAULT_HEIGHT;
int lynxfb_default_depth = LYNXFB_DEFAULT_DEPTH;
int lynxfb_default_stride = LYNXFB_DEFAULT_STRIDE;
int lynxfb_default_flags = LYNXFB_DEFAULT_FLAGS;

static int
lynxfb_match_by_id(pcireg_t id)
{

	if (PCI_VENDOR(id) != PCI_VENDOR_SILMOTION)
		return (0);
	if (PCI_PRODUCT(id) != PCI_PRODUCT_SILMOTION_SM712)
		return (0);
	return (1);
}

int
lynxfb_cnattach(bus_space_tag_t memt, bus_space_tag_t iot, pci_chipset_tag_t pc,
    pcitag_t tag, pcireg_t id)
{
	struct rasops_info *ri = &lynxfb_console.fb.vcs.scr_ri;
	struct lynxfb *fb;
	bus_space_handle_t memh;
	bus_addr_t base;
	bus_size_t size;
	long defattr;
	int mapflags;
	int error;

	if (!lynxfb_match_by_id(id))
		return (ENODEV);

	if (pci_mapreg_info(pc, tag, PCI_MAPREG_START, PCI_MAPREG_TYPE_MEM,
	    &base, &size, &mapflags))
		return (EINVAL);
	error = bus_space_map(memt, base, size, BUS_SPACE_MAP_LINEAR | mapflags,
	    &memh);
	if (error)
		return (error);

	fb = &lynxfb_console.fb;
	fb->memt = memt;
	fb->memh = memh;
	fb->width = lynxfb_default_width;
	fb->height = lynxfb_default_height;
	fb->depth = lynxfb_default_depth;
	fb->stride = lynxfb_default_stride;
	if (fb->stride == 0)
		fb->stride = fb->width * fb->depth / 8;
	fb->flags = lynxfb_default_flags;
	error = lynxfb_setup(fb);
	if (error) {
		bus_space_unmap(memt, memh, size);
		return (error);
	}

	lynxfb_console.is_console = true;
	lynxfb_console.tag = tag;
	lynxfb_console.fbaddr = base;
	lynxfb_console.fbsize = size;
	(*ri->ri_ops.allocattr)(ri, 0, 0, 0, &defattr);
	wsdisplay_preattach(&fb->wsd, ri, 0, 0, defattr);

	return (0);
}

static bool
lynxfb_is_console(struct lynxfb_softc *sc, pcitag_t tag)
{

	return (lynxfb_console.is_console &&
	    !memcmp(&lynxfb_console.tag, &tag, sizeof(tag)));
}

static int
lynxfb_match(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa = (struct pci_attach_args *)aux;

	if (PCI_CLASS(pa->pa_class) != PCI_CLASS_DISPLAY)
		return (0);
	if (!lynxfb_match_by_id(pa->pa_id))
		return (0);
	return (1);
}

static void
lynxfb_attach(device_t parent, device_t self, void *aux)
{
	struct lynxfb_softc *sc = device_private(self);
	struct pci_attach_args *pa = (struct pci_attach_args *)aux;
	struct wsemuldisplaydev_attach_args waa;
	struct lynxfb *fb;
	struct rasops_info *ri;
	prop_dictionary_t dict;
	long defattr;
	bool is_console;

	sc->sc_dev = self;
	sc->sc_pc = pa->pa_pc;
	sc->sc_pcitag = pa->pa_tag;

	pci_aprint_devinfo(pa, NULL);

	is_console = lynxfb_is_console(sc, sc->sc_pcitag);

	fb = is_console ? &lynxfb_console.fb : &sc->sc_fb_store;
	sc->sc_fb = fb;
	fb->sc = sc;

	if (is_console) {
		sc->sc_fbaddr = lynxfb_console.fbaddr;
		sc->sc_fbsize = lynxfb_console.fbsize;
	} else {
		if (pci_mapreg_map(pa, PCI_MAPREG_START, PCI_MAPREG_TYPE_MEM,
		    BUS_SPACE_MAP_LINEAR, &fb->memt, &fb->memh, &sc->sc_fbaddr,
		    &sc->sc_fbsize)) {
			aprint_error_dev(self, "can't map frame buffer\n");
			return;
		}

		dict = device_properties(self);
		if (!prop_dictionary_get_uint32(dict, "width", &fb->width))
			fb->width = lynxfb_default_width;
		if (!prop_dictionary_get_uint32(dict, "height", &fb->height))
			fb->height = lynxfb_default_height;
		if (!prop_dictionary_get_uint32(dict, "depth", &fb->depth))
			fb->depth = lynxfb_default_depth;
		if (!prop_dictionary_get_uint32(dict, "linebytes", &fb->stride)) {
			if (lynxfb_default_stride == 0)
				fb->stride = fb->width * fb->depth / 8;
			else
				fb->stride = lynxfb_default_stride;
		}
		if (!prop_dictionary_get_uint32(dict, "flags", &fb->flags))
			fb->flags = lynxfb_default_flags;

		if (lynxfb_setup(fb)) {
			aprint_error_dev(self, "can't setup frame buffer\n");
			bus_space_unmap(fb->memt, fb->memh, sc->sc_fbsize);
			return;
		}
	}
	sc->sc_memt = fb->memt;
	sc->sc_memh = fb->memh;

	aprint_normal_dev(self, "%d x %d, %d bpp, stride %d\n", fb->width,
	    fb->height, fb->depth, fb->stride);

	fb->wsd = (struct wsscreen_descr) {
		"default",
		0, 0,
		NULL,
		8, 16,
		WSSCREEN_WSCOLORS | WSSCREEN_HILIT,
		NULL,
	};
	sc->sc_screens[0] = &fb->wsd;
	sc->sc_screenlist = (struct wsscreen_list){ 1, sc->sc_screens };
	sc->sc_mode = WSDISPLAYIO_MODE_EMUL;

	vcons_init(&sc->sc_vd, sc, &fb->wsd, &lynxfb_accessops);
	sc->sc_vd.init_screen = lynxfb_init_screen;

	ri = &fb->vcs.scr_ri;
	if (is_console) {
		vcons_init_screen(&sc->sc_vd, &fb->vcs, 1, &defattr);
		fb->vcs.scr_flags |= VCONS_SCREEN_IS_STATIC;

		fb->wsd.textops = &ri->ri_ops;
		fb->wsd.capabilities = ri->ri_caps;
		fb->wsd.nrows = ri->ri_rows;
		fb->wsd.ncols = ri->ri_cols;
		wsdisplay_cnattach(&fb->wsd, ri, 0, 0, defattr);
		vcons_replay_msgbuf(&fb->vcs);
	} else {
		/*
		 * since we're not the console we can postpone the rest
		 * until someone actually allocates a screen for us
		 */
		(*ri->ri_ops.allocattr)(ri, 0, 0, 0, &defattr);
	}

	waa.console = is_console;
	waa.scrdata = &sc->sc_screenlist;
	waa.accessops = &lynxfb_accessops;
	waa.accesscookie = &sc->sc_vd;

	config_found(self, &waa, wsemuldisplaydevprint);
}

/*
 * vga sequencer access through MMIO space
 */
static __inline uint8_t
lynxfb_vgats_read(struct lynxfb *fb, uint regno)
{

	bus_space_write_1(fb->mmiot, fb->mmioh, IO_VGA + VGA_TS_INDEX, regno);
	return bus_space_read_1(fb->mmiot, fb->mmioh, IO_VGA + VGA_TS_DATA);
}

static __inline void
lynxfb_vgats_write(struct lynxfb *fb, uint regno, uint8_t value)
{

	bus_space_write_1(fb->mmiot, fb->mmioh, IO_VGA + VGA_TS_INDEX, regno);
	bus_space_write_1(fb->mmiot, fb->mmioh, IO_VGA + VGA_TS_DATA, value);
}

/*
 * wsdisplay accesops
 */
static int
lynxfb_ioctl(void *v, void *vs, u_long cmd, void *data, int flags,
    struct lwp *l)
{
	struct vcons_data *vd = v;
	struct lynxfb_softc *sc = vd->cookie;
	struct lynxfb *fb = sc->sc_fb;
	struct vcons_screen *ms = vd->active;
	struct wsdisplay_fbinfo *wdf;
	struct wsdisplay_param *param;

	switch (cmd) {
	case WSDISPLAYIO_GTYPE:
		*(uint *)data = WSDISPLAY_TYPE_PCIMISC;
		return (0);

	/* PCI config read/write passthrough. */
	case PCI_IOC_CFGREAD:
	case PCI_IOC_CFGWRITE:
		return pci_devioctl(sc->sc_pc, sc->sc_pcitag,
		    cmd, data, flags, l);

	case WSDISPLAYIO_GET_BUSID:
		return wsdisplayio_busid_pci(sc->sc_dev, sc->sc_pc,
		    sc->sc_pcitag, data);

	case WSDISPLAYIO_GINFO:
		if (ms == NULL)
			return (ENODEV);
		wdf = (struct wsdisplay_fbinfo *)data;
		wdf->width = ms->scr_ri.ri_width;
		wdf->height = ms->scr_ri.ri_height;
		wdf->depth = ms->scr_ri.ri_depth;
		wdf->cmsize = 0;
		return (0);

	case WSDISPLAYIO_LINEBYTES:
		*(uint *)data = fb->stride;
		return (0);

	case WSDISPLAYIO_GVIDEO:
		*(int *)data = fb->blank ? WSDISPLAYIO_VIDEO_OFF :
		    WSDISPLAYIO_VIDEO_ON;
		return (0);

	case WSDISPLAYIO_SVIDEO:
		lynxfb_blank(fb, *(int *)data);
		return (0);

	case WSDISPLAYIO_GETPARAM:
		param = (struct wsdisplay_param *)data;
		switch (param->param) {
		case WSDISPLAYIO_PARAM_BACKLIGHT:
			param->min = 0;
			param->max = 1;
			param->curval = fb->blank;
			return (0);
		}
		break;

	case WSDISPLAYIO_SETPARAM:
		param = (struct wsdisplay_param *)data;
		switch (param->param) {
		case WSDISPLAYIO_PARAM_BACKLIGHT:
			lynxfb_blank(fb, param->curval);
			return (0);
		}
		break;
	}
	return (EPASSTHROUGH);
}

static paddr_t
lynxfb_mmap(void *v, void *vs, off_t offset, int prot)
{
	struct vcons_data *vd = v;
	struct lynxfb_softc *sc = vd->cookie;
	struct rasops_info *ri = &sc->sc_fb->vcs.scr_ri;

	/* 'regular' framebuffer mmap()ing */
	if (offset < ri->ri_stride * ri->ri_height) {
		return bus_space_mmap(sc->sc_memt, sc->sc_fbaddr + offset, 0,
		    prot, BUS_SPACE_MAP_LINEAR | BUS_SPACE_MAP_PREFETCHABLE);
	}

	/*
	 * restrict all other mappings to processes with superuser privileges
	 * or the kernel itself
	 */
	if (kauth_authorize_machdep(kauth_cred_get(),
	    KAUTH_MACHDEP_UNMANAGEDMEM, NULL, NULL, NULL, NULL) != 0) {
		aprint_normal_dev(sc->sc_dev, "mmap() rejected.\n");
		return (-1);
	}

	/* framebuffer mmap()ing */
	if (offset >= sc->sc_fbaddr &&
	    offset < sc->sc_fbaddr + ri->ri_stride * ri->ri_height) {
		return bus_space_mmap(sc->sc_memt, offset, 0, prot,
		    BUS_SPACE_MAP_LINEAR | BUS_SPACE_MAP_PREFETCHABLE);
	}

	/* register mmap()ing */
	if (offset >= sc->sc_fbaddr + SM7XX_REG_BASE &&
	    offset < sc->sc_fbaddr + SM7XX_REG_BASE + SM7XX_REG_SIZE) {
		return bus_space_mmap(sc->sc_memt, offset, 0, prot, 0);
	}

	return (-1);
}

static void
lynxfb_init_screen(void *cookie, struct vcons_screen *scr,
    int existing, long *defattr)
{
	struct lynxfb_softc *sc = cookie;
	struct lynxfb *fb = sc->sc_fb;
	struct rasops_info *ri = &scr->scr_ri;

	ri->ri_width = fb->width;
	ri->ri_height = fb->height;
	ri->ri_depth = fb->depth;
	ri->ri_stride = fb->stride;
	ri->ri_flg = RI_CENTER;
	ri->ri_bits = fb->fbaddr;

#ifdef VCONS_DRAW_INTR
	scr->scr_flags |= VCONS_DONT_READ;
#endif

	if (existing)
		ri->ri_flg |= RI_CLEAR;

	rasops_init(ri, 0, 0);
	ri->ri_caps = WSSCREEN_WSCOLORS;
	rasops_reconfig(ri, fb->height / ri->ri_font->fontheight,
	    fb->width / ri->ri_font->fontwidth);

	ri->ri_hw = scr;

	if (fb->accel) {
		ri->ri_ops.copycols = lynxfb_vcons_copycols;
		ri->ri_ops.copyrows = lynxfb_vcons_copyrows;
		ri->ri_ops.erasecols = lynxfb_vcons_erasecols;
		ri->ri_ops.eraserows = lynxfb_vcons_eraserows;
	}
}

/*
 * Frame buffer initialization.
 */
static int
lynxfb_setup(struct lynxfb *fb)
{
	struct rasops_info *ri = &fb->vcs.scr_ri;
	int error;

	fb->dprt = fb->memt;
	error = bus_space_subregion(fb->memt, fb->memh, SM7XX_DPR_BASE,
	    SMXXX_DPR_SIZE, &fb->dprh);
	if (error != 0)
		return (error);

	fb->mmiot = fb->memt;
	error = bus_space_subregion(fb->mmiot, fb->memh, SM7XX_MMIO_BASE,
	    SM7XX_MMIO_SIZE, &fb->mmioh);
	if (error != 0)
		return (error);

	ri->ri_width = fb->width;
	ri->ri_height = fb->height;
	ri->ri_depth = fb->depth;
	ri->ri_stride = fb->stride;
	ri->ri_flg = RI_CENTER | RI_CLEAR | RI_NO_AUTO;
	fb->fbaddr = ri->ri_bits = (void *)bus_space_vaddr(fb->memt, fb->memh);
	ri->ri_hw = fb;

	if (fb->flags & LYNXFB_FLAG_SWAPBR) {
		switch (fb->depth) {
		case 16:
			ri->ri_rnum = 5;
			ri->ri_rpos = 11;
			ri->ri_gnum = 6;
			ri->ri_gpos = 5;
			ri->ri_bnum = 5;
			ri->ri_bpos = 0;
			break;
		}
	}

	rasops_init(ri, 0, 0);
	rasops_reconfig(ri, ri->ri_height / ri->ri_font->fontheight,
	    ri->ri_width / ri->ri_font->fontwidth);

	fb->wsd.name = "std";
	fb->wsd.ncols = ri->ri_cols;
	fb->wsd.nrows = ri->ri_rows;
	fb->wsd.textops = &ri->ri_ops;
	fb->wsd.fontwidth = ri->ri_font->fontwidth;
	fb->wsd.fontheight = ri->ri_font->fontheight;
	fb->wsd.capabilities = ri->ri_caps;

	/*
	 * Setup 2D acceleration whenever possible
	 */
	if (lynxfb_wait(fb) == 0) {
		fb->accel = 1;

		DPR_WRITE(fb, DPR_CROP_TOPLEFT_COORDS, DPR_COORDS(0, 0));
		/* use of width both times is intentional */
		DPR_WRITE(fb, DPR_PITCH,
		    DPR_COORDS(ri->ri_width, ri->ri_width));
		DPR_WRITE(fb, DPR_SRC_WINDOW,
		    DPR_COORDS(ri->ri_width, ri->ri_width));
		DPR_WRITE(fb, DPR_BYTE_BIT_MASK, 0xffffffff);
		DPR_WRITE(fb, DPR_COLOR_COMPARE_MASK, 0);
		DPR_WRITE(fb, DPR_COLOR_COMPARE, 0);
		DPR_WRITE(fb, DPR_SRC_BASE, 0);
		DPR_WRITE(fb, DPR_DST_BASE, 0);
		DPR_READ(fb, DPR_DST_BASE);

		ri->ri_ops.copycols = lynxfb_copycols;
		ri->ri_ops.copyrows = lynxfb_copyrows;
		ri->ri_ops.erasecols = lynxfb_erasecols;
		ri->ri_ops.eraserows = lynxfb_eraserows;
	}

	return (0);
}

static int
lynxfb_wait(struct lynxfb *fb)
{
	uint32_t reg;
	int i;

	for (i = 10000; i > 0; i--) {
		reg = lynxfb_vgats_read(fb, 0x16);
		if ((reg & 0x18) == 0x10)
			return (0);
		delay(1);
	}
	return (EBUSY);
}

static void
lynxfb_copyrect(struct lynxfb *fb, int sx, int sy, int dx, int dy, int w, int h)
{
	uint32_t dir;

	/* Compute rop direction */
	if (sy < dy || (sy == dy && sx <= dx)) {
		sx += w - 1;
		dx += w - 1;
		sy += h - 1;
		dy += h - 1;
		dir = DE_CTRL_RTOL;
	} else
		dir = 0;

	DPR_WRITE(fb, DPR_SRC_COORDS, DPR_COORDS(sx, sy));
	DPR_WRITE(fb, DPR_DST_COORDS, DPR_COORDS(dx, dy));
	DPR_WRITE(fb, DPR_SPAN_COORDS, DPR_COORDS(w, h));
	DPR_WRITE(fb, DPR_DE_CTRL, DE_CTRL_START | DE_CTRL_ROP_ENABLE | dir |
	    (DE_CTRL_COMMAND_BITBLT << DE_CTRL_COMMAND_SHIFT) |
	    (DE_CTRL_ROP_SRC << DE_CTRL_ROP_SHIFT));
	DPR_READ(fb, DPR_DE_CTRL);

	lynxfb_wait(fb);
}

static void
lynxfb_fillrect(struct lynxfb *fb, int x, int y, int w, int h, int bg)
{
	struct rasops_info *ri = &fb->vcs.scr_ri;

	DPR_WRITE(fb, DPR_FG_COLOR, ri->ri_devcmap[bg]);
	DPR_WRITE(fb, DPR_DST_COORDS, DPR_COORDS(x, y));
	DPR_WRITE(fb, DPR_SPAN_COORDS, DPR_COORDS(w, h));
	DPR_WRITE(fb, DPR_DE_CTRL, DE_CTRL_START | DE_CTRL_ROP_ENABLE |
	    (DE_CTRL_COMMAND_SOLIDFILL << DE_CTRL_COMMAND_SHIFT) |
	    (DE_CTRL_ROP_SRC << DE_CTRL_ROP_SHIFT));
	DPR_READ(fb, DPR_DE_CTRL);

	lynxfb_wait(fb);
}

static inline void
lynxfb_copyrows1(struct rasops_info *ri, int src, int dst, int num,
    struct lynxfb *fb)
{
	struct wsdisplay_font *f = ri->ri_font;

	num *= f->fontheight;
	src *= f->fontheight;
	dst *= f->fontheight;

	lynxfb_copyrect(fb, ri->ri_xorigin, ri->ri_yorigin + src,
	    ri->ri_xorigin, ri->ri_yorigin + dst, ri->ri_emuwidth, num);
}

static inline void
lynxfb_copycols1(struct rasops_info *ri, int row, int src, int dst, int num,
    struct lynxfb *fb)
{
	struct wsdisplay_font *f = ri->ri_font;

	num *= f->fontwidth;
	src *= f->fontwidth;
	dst *= f->fontwidth;
	row *= f->fontheight;

	lynxfb_copyrect(fb, ri->ri_xorigin + src, ri->ri_yorigin + row,
	    ri->ri_xorigin + dst, ri->ri_yorigin + row, num, f->fontheight);
}

static inline void
lynxfb_erasecols1(struct rasops_info *ri, int row, int col, int num, long attr,
    struct lynxfb *fb)
{
	struct wsdisplay_font *f = ri->ri_font;
	int32_t bg, fg, ul;

	row *= f->fontheight;
	col *= f->fontwidth;
	num *= f->fontwidth;
	rasops_unpack_attr(attr, &fg, &bg, &ul);

	lynxfb_fillrect(fb, ri->ri_xorigin + col, ri->ri_yorigin + row,
	    num, f->fontheight, bg);
}

static inline void
lynxfb_eraserows1(struct rasops_info *ri, int row, int num, long attr,
    struct lynxfb *fb)
{
	struct wsdisplay_font *f = ri->ri_font;
	int32_t bg, fg, ul;
	int x, y, w;

	if ((num == ri->ri_rows) && ISSET(ri->ri_flg, RI_FULLCLEAR)) {
		num = ri->ri_height;
		x = y = 0;
		w = ri->ri_width;
	} else {
		num *= f->fontheight;
		x = ri->ri_xorigin;
		y = ri->ri_yorigin + row * f->fontheight;
		w = ri->ri_emuwidth;
	}
	rasops_unpack_attr(attr, &fg, &bg, &ul);
	lynxfb_fillrect(fb, x, y, w, num, bg);
}

static void
lynxfb_copyrows(void *cookie, int src, int dst, int num)
{
	struct rasops_info *ri = cookie;
	struct lynxfb *fb = ri->ri_hw;

	lynxfb_copyrows1(ri, src, dst, num, fb);
}

static void
lynxfb_copycols(void *cookie, int row, int src, int dst, int num)
{
	struct rasops_info *ri = cookie;
	struct lynxfb *fb = ri->ri_hw;

	lynxfb_copycols1(ri, row, src, dst, num, fb);
}

static void
lynxfb_erasecols(void *cookie, int row, int col, int num, long attr)
{
	struct rasops_info *ri = cookie;
	struct lynxfb *fb = ri->ri_hw;

	lynxfb_erasecols1(ri, row, col, num, attr, fb);
}

static void
lynxfb_eraserows(void *cookie, int row, int num, long attr)
{
	struct rasops_info *ri = cookie;
	struct lynxfb *fb = ri->ri_hw;

	lynxfb_eraserows1(ri, row, num, attr, fb);
}

static void
lynxfb_vcons_copyrows(void *cookie, int src, int dst, int num)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;
	struct lynxfb_softc *sc = scr->scr_cookie;
	struct lynxfb *fb = sc->sc_fb;

	lynxfb_copyrows1(ri, src, dst, num, fb);
}

static void
lynxfb_vcons_copycols(void *cookie, int row, int src, int dst, int num)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;
	struct lynxfb_softc *sc = scr->scr_cookie;
	struct lynxfb *fb = sc->sc_fb;

	lynxfb_copycols1(ri, row, src, dst, num, fb);
}

static void
lynxfb_vcons_erasecols(void *cookie, int row, int col, int num, long attr)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;
	struct lynxfb_softc *sc = scr->scr_cookie;
	struct lynxfb *fb = sc->sc_fb;

	lynxfb_erasecols1(ri, row, col, num, attr, fb);
}

static void
lynxfb_vcons_eraserows(void *cookie, int row, int num, long attr)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;
	struct lynxfb_softc *sc = scr->scr_cookie;
	struct lynxfb *fb = sc->sc_fb;

	lynxfb_eraserows1(ri, row, num, attr, fb);
}

static void
lynxfb_blank(struct lynxfb *fb, int blank)
{

	fb->blank = blank;
	if (!blank) {
		lynxfb_vgats_write(fb, 0x31,
		    lynxfb_vgats_read(fb, 0x31) | 0x01);
	} else {
		lynxfb_vgats_write(fb, 0x21,
		    lynxfb_vgats_read(fb, 0x21) | 0x30);
		lynxfb_vgats_write(fb, 0x31,
		    lynxfb_vgats_read(fb, 0x31) & ~0x01);
	}
}
