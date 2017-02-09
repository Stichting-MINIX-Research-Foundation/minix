/*	$OpenBSD: sisfb.c,v 1.2 2010/12/26 15:40:59 miod Exp $	*/

/*
 * Copyright (c) 2010 Miodrag Vallat.
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
 * Minimalistic driver for the SIS315 Pro frame buffer found on the
 * Lemote Fuloong 2F systems.
 * Does not support accelaration, mode change, secondary output, or
 * anything fancy.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: sisfb.c,v 1.5 2014/01/26 21:22:49 bouyer Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/bus.h>
#include <sys/kauth.h>

#include <uvm/uvm_extern.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>
#include <dev/pci/pciio.h>

#include <dev/videomode/videomode.h>

#include <dev/wscons/wsdisplayvar.h>
#include <dev/wscons/wsconsio.h>
#include <dev/wsfont/wsfont.h>
#include <dev/rasops/rasops.h>
#include <dev/wscons/wsdisplay_vconsvar.h>
#include <dev/pci/wsdisplay_pci.h>

#include <dev/i2c/i2cvar.h>

#include <dev/pci/sisfb.h>

struct sisfb_softc;

/* minimal frame buffer information, suitable for early console */

struct sisfb {
	struct sisfb_softc	*sc;
	uint8_t			 cmap[256 * 3];

	bus_space_tag_t		 fbt;
	bus_space_handle_t	 fbh;
	bus_addr_t	 	 fbbase;
	bus_size_t	 	 fbsize;

	bus_space_tag_t		 mmiot;
	bus_space_handle_t	 mmioh;
	bus_addr_t	 	 mmiobase;
	bus_size_t	 	 mmiosize;

	bus_space_tag_t		 iot;
	bus_space_handle_t	 ioh;
	bus_addr_t	 	 iobase;
	bus_size_t	 	 iosize;

	struct vcons_screen	 vcs;
	struct wsscreen_descr	 wsd;

	int			 fb_depth;
	int			 fb_width;
	int			 fb_height;
	int			 fb_stride;
	void 			 *fb_addr;
};

struct sisfb_softc {
	device_t		 sc_dev;
	struct sisfb		*sc_fb;
	struct sisfb		 sc_fb_store;

	struct vcons_data	vd;
	struct wsscreen_list	sc_wsl;
	const struct wsscreen_descr	*sc_scrlist[1];
	int			sc_nscr;
	int			sc_mode;

	pci_chipset_tag_t	sc_pc;
	pcitag_t		sc_pt;
};

int	sisfb_match(device_t, cfdata_t, void *);
void	sisfb_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(sisfb, sizeof(struct sisfb_softc),
    sisfb_match, sisfb_attach, NULL, NULL);


int	sisfb_alloc_screen(void *, const struct wsscreen_descr *, void **, int *,
	    int *, long *);
void	sisfb_free_screen(void *, void *);
int	sisfb_ioctl(void *, void *, u_long, void *, int, struct lwp *);
int	sisfb_show_screen(void *, void *, int, void (*)(void *, int, int),
	    void *);
paddr_t	sisfb_mmap(void *, void *, off_t, int);
void	sisfb_init_screen(void *, struct vcons_screen *, int, long *);


struct wsdisplay_accessops sisfb_accessops = {
	sisfb_ioctl,
	sisfb_mmap,
	sisfb_alloc_screen,
	sisfb_free_screen,
	sisfb_show_screen,
	NULL,	/* load_font */
	NULL,	/* poolc */
	NULL	/* scroll */
};

int	sisfb_getcmap(uint8_t *, struct wsdisplay_cmap *);
void	sisfb_loadcmap(struct sisfb *, int, int);
int	sisfb_putcmap(uint8_t *, struct wsdisplay_cmap *);
int	sisfb_setup(struct sisfb *);

static struct sisfb sisfbcn;

/*
 * Control Register access
 *
 * These are 8 bit registers; the choice of larger width types is intentional.
 */

#define	SIS_VGA_PORT_OFFSET	0x380

#define	SEQ_ADDR		(0x3c4 - SIS_VGA_PORT_OFFSET)
#define	SEQ_DATA		(0x3c5 - SIS_VGA_PORT_OFFSET)
#define	DAC_ADDR		(0x3c8 - SIS_VGA_PORT_OFFSET)
#define	DAC_DATA		(0x3c9 - SIS_VGA_PORT_OFFSET)
#undef	CRTC_ADDR
#define	CRTC_ADDR		(0x3d4 - SIS_VGA_PORT_OFFSET)
#define	CRTC_DATA		(0x3d5 - SIS_VGA_PORT_OFFSET)

#define	CRTC_HDISPLE	0x01	/* horizontal display end */
#define	CRTC_OVERFLL	0x07	/* overflow low */
#define	CRTC_STARTADRH	0x0C	/* linear start	address mid */
#define	CRTC_STARTADRL	0x0D	/* linear start	address low */
#define	CRTC_VDE	0x12	/* vertical display end */


static inline uint sisfb_crtc_read(struct sisfb *, uint);
static inline void sisfb_crtc_write(struct sisfb *, uint, uint);
static inline uint sisfb_seq_read(struct sisfb *, uint);
static inline void sisfb_seq_write(struct sisfb *, uint, uint);

static inline uint
sisfb_crtc_read(struct sisfb *fb, uint idx)
{
	uint val;
	bus_space_write_1(fb->iot, fb->ioh, CRTC_ADDR, idx);
	val = bus_space_read_1(fb->iot, fb->ioh, CRTC_DATA);
#ifdef SIS_DEBUG
	printf("CRTC %04x -> %02x\n", idx, val);
#endif
	return val;
}

static inline void
sisfb_crtc_write(struct sisfb *fb, uint idx, uint val)
{
#ifdef SIS_DEBUG
	printf("CRTC %04x <- %02x\n", idx, val);
#endif
	bus_space_write_1(fb->iot, fb->ioh, CRTC_ADDR, idx);
	bus_space_write_1(fb->iot, fb->ioh, CRTC_DATA, val);
}

static inline uint
sisfb_seq_read(struct sisfb *fb, uint idx)
{
	uint val;
	bus_space_write_1(fb->iot, fb->ioh, SEQ_ADDR, idx);
	val = bus_space_read_1(fb->iot, fb->ioh, SEQ_DATA);
#ifdef SIS_DEBUG
	printf("SEQ %04x -> %02x\n", idx, val);
#endif
	return val;
}

static inline void
sisfb_seq_write(struct sisfb *fb, uint idx, uint val)
{
#ifdef SIS_DEBUG
	printf("SEQ %04x <- %02x\n", idx, val);
#endif
	bus_space_write_1(fb->iot, fb->ioh, SEQ_ADDR, idx);
	bus_space_write_1(fb->iot, fb->ioh, SEQ_DATA, val);
}

int
sisfb_match(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa = (struct pci_attach_args *)aux;

	if (PCI_CLASS(pa->pa_class) != PCI_CLASS_DISPLAY)
		return 0;

	if (PCI_VENDOR(pa->pa_id) != PCI_VENDOR_SIS)
		return 0;

	if (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_SIS_315PRO_VGA)
		return 100;
	return (0);
}

void
sisfb_attach(device_t parent, device_t self, void *aux)
{
	struct sisfb_softc *sc = device_private(self);
	struct pci_attach_args *pa = (struct pci_attach_args *)aux;
	struct rasops_info *ri;
	struct wsemuldisplaydev_attach_args waa;
	struct sisfb *fb;
	int console;
	unsigned long defattr;

	sc->sc_dev = self;
	console = sisfbcn.vcs.scr_ri.ri_hw != NULL;

	if (console)
		fb = &sisfbcn;
	else {
		fb = &sc->sc_fb_store;
	}

	sc->sc_fb = fb;
	fb->sc = sc;

	pci_aprint_devinfo(pa, NULL);

	sc->sc_pt = pa->pa_tag;
	sc->sc_pc = pa->pa_pc;

	if (!console) {
		fb->fbt = pa->pa_memt;
		fb->mmiot = pa->pa_memt;
		fb->iot = pa->pa_iot;
		if (pci_mapreg_map(pa, PCI_MAPREG_START, PCI_MAPREG_TYPE_MEM,
		    BUS_SPACE_MAP_LINEAR, &fb->fbt, &fb->fbh,
		    &fb->fbbase, &fb->fbsize) != 0) {
			aprint_error_dev(self, ": can't map frame buffer\n");
			return;
		}

		if (pci_mapreg_map(pa, PCI_MAPREG_START + 4,
		    PCI_MAPREG_TYPE_MEM, 0,
		    &fb->mmiot, &fb->mmioh, &fb->mmiobase, &fb->mmiosize) != 0) {
			aprint_error_dev(self, ": can't map mmio area\n");
			goto fail1;
		}

		if (pci_mapreg_map(pa, PCI_MAPREG_START + 8, PCI_MAPREG_TYPE_IO,
		    0, &fb->iot, &fb->ioh, &fb->iobase, &fb->iosize) != 0) {
			aprint_error_dev(self, ": can't map registers\n");
			goto fail2;
		}


		if (sisfb_setup(sc->sc_fb) != 0) {
			aprint_error_dev(self, ": can't setup frame buffer\n");
			goto fail3;
		}
	}

	aprint_normal_dev(self, ": %dx%dx%d frame buffer\n",
	    fb->vcs.scr_ri.ri_width, fb->vcs.scr_ri.ri_height,
	    fb->vcs.scr_ri.ri_depth);

	aprint_debug_dev(self, ": fb 0x%" PRIxBUSSIZE "@0x%" PRIxBUSADDR
	    ", mmio 0x%" PRIxBUSSIZE "@0x%" PRIxBUSADDR
	    ", io 0x%" PRIxBUSSIZE "@0x%" PRIxBUSADDR "\n",
	    fb->fbsize, fb->fbbase,
	    fb->mmiosize, fb->mmiobase,
	    fb->iosize, fb->iobase);

	fb->wsd = (struct wsscreen_descr){
		"default",
		0, 0,
		NULL,
		8, 16,
		WSSCREEN_WSCOLORS | WSSCREEN_HILIT,
		NULL
	};
	sc->sc_scrlist[0] = &fb->wsd;
	sc->sc_wsl = (struct wsscreen_list){1, sc->sc_scrlist};
	sc->sc_mode = WSDISPLAYIO_MODE_EMUL;


	vcons_init(&sc->vd, sc, &fb->wsd, &sisfb_accessops);
	sc->vd.init_screen = sisfb_init_screen;

	ri = &fb->vcs.scr_ri;
	if (console) {
		vcons_init_screen(&sc->vd, &fb->vcs, 1, &defattr);
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

	waa.console = console;
	waa.scrdata = &sc->sc_wsl;
	waa.accessops = &sisfb_accessops;
	waa.accesscookie = &sc->vd;

	config_found(sc->sc_dev, &waa, wsemuldisplaydevprint);
	return;

fail3:
	bus_space_unmap(fb->iot, fb->ioh, fb->iosize);
fail2:
	bus_space_unmap(fb->mmiot, fb->mmioh, fb->mmiosize);
fail1:
	bus_space_unmap(fb->fbt, fb->fbh, fb->fbsize);
}

/*
 * wsdisplay accesops
 */

int
sisfb_alloc_screen(void *v, const struct wsscreen_descr *type, void **cookiep,
    int *curxp, int *curyp, long *attrp)
{
	struct sisfb_softc *sc = (struct sisfb_softc *)v;
	struct rasops_info *ri = &sc->sc_fb->vcs.scr_ri;

	if (sc->sc_nscr > 0)
		return ENOMEM;

	*cookiep = ri;
	*curxp = *curyp = 0;
	ri->ri_ops.allocattr(ri, 0, 0, 0, attrp);
	sc->sc_nscr++;

	return 0;
}

void
sisfb_free_screen(void *v, void *cookie)
{
	struct sisfb_softc *sc = (struct sisfb_softc *)v;

	sc->sc_nscr--;
}

int
sisfb_ioctl(void *v, void *vs, u_long cmd, void *data, int flags,
    struct lwp *l)
{
	struct vcons_data *vd = v;
	struct sisfb_softc *sc = vd->cookie;
	struct sisfb *fb = sc->sc_fb;
	struct rasops_info *ri = &fb->vcs.scr_ri;
	struct wsdisplay_cmap *cm;
	struct wsdisplay_fbinfo *wdf;
	int rc;

	switch (cmd) {
	case WSDISPLAYIO_GTYPE:
		*(uint *)data = WSDISPLAY_TYPE_PCIMISC;
		return 0;
	case WSDISPLAYIO_GINFO:
		if (vd->active != NULL) {
			wdf = (struct wsdisplay_fbinfo *)data;
			wdf->width = ri->ri_width;
			wdf->height = ri->ri_height;
			wdf->depth = ri->ri_depth;
			wdf->cmsize = 256;
			return 0;
		} else
			return ENODEV;
	case WSDISPLAYIO_LINEBYTES:
		*(uint *)data = ri->ri_stride;
		return 0;
	case WSDISPLAYIO_GETCMAP:
		cm = (struct wsdisplay_cmap *)data;
		rc = sisfb_getcmap(fb->cmap, cm);
		return rc;
	case WSDISPLAYIO_PUTCMAP:
		cm = (struct wsdisplay_cmap *)data;
		rc = sisfb_putcmap(fb->cmap, cm);
		if (rc != 0)
			return rc;
		if (ri->ri_depth == 8)
			sisfb_loadcmap(fb, cm->index, cm->count);
		return 0;
	case WSDISPLAYIO_SMODE:
		sc->sc_mode = *(uint *)data;
		aprint_debug_dev(sc->sc_dev, ": switching to ");
		switch(sc->sc_mode) {
		case WSDISPLAYIO_MODE_EMUL:
			aprint_debug("WSDISPLAYIO_MODE_EMUL\n");
			break;
		case WSDISPLAYIO_MODE_MAPPED:
			aprint_debug("WSDISPLAYIO_MODE_MAPPED\n");
			break;
		case WSDISPLAYIO_MODE_DUMBFB:
			aprint_debug("WSDISPLAYIO_MODE_DUMBFB\n");
			break;
		default:
			aprint_debug("unknown mode %d\n", sc->sc_mode);
			return EINVAL;
		}
		return 0;
	case WSDISPLAYIO_GMODE:
		*(uint *)data = sc->sc_mode;
		return 0;
	case PCI_IOC_CFGREAD:
	case PCI_IOC_CFGWRITE:
		return pci_devioctl(sc->sc_pc, sc->sc_pt, cmd, data, flags, l);
	case WSDISPLAYIO_GET_BUSID:
		return wsdisplayio_busid_pci(sc->sc_dev, sc->sc_pc,
		    sc->sc_pt, data);
	}
	return EPASSTHROUGH;
}

int
sisfb_show_screen(void *v, void *cookie, int waitok,
    void (*cb)(void *, int, int), void *cbarg)
{
	return 0;
}

paddr_t
sisfb_mmap(void *v, void *vs, off_t offset, int prot)
{
	struct vcons_data *vd = v;
	struct sisfb_softc *sc = vd->cookie;
	struct rasops_info *ri = &sc->sc_fb->vcs.scr_ri;
	struct sisfb *fb = sc->sc_fb;
	const uintptr_t fb_offset =
	  (uintptr_t)bus_space_vaddr(fb->fbt, fb->fbh) - (uintptr_t)fb->fb_addr;
	paddr_t pa;

	if (sc->sc_mode != WSDISPLAYIO_MODE_MAPPED) {
		if (offset >= 0 && offset < ri->ri_stride * ri->ri_height) {
			pa = bus_space_mmap(fb->fbt,
			    fb->fbbase, fb_offset + offset,
			    prot, BUS_SPACE_MAP_LINEAR);
			return pa;
		}
		return -1;
	}
	if (kauth_authorize_generic(kauth_cred_get(), KAUTH_GENERIC_ISSUSER,
	    NULL) != 0) {
		return -1;
	}
	if (offset >= (fb->fbbase & ~PAGE_MASK) &&
	    offset <= ((fb->fbbase + fb->fbsize + PAGE_SIZE - 1) & ~PAGE_MASK)) {
		pa = bus_space_mmap(fb->fbt, fb->fbbase, offset - fb->fbbase,
		    prot, BUS_SPACE_MAP_LINEAR|BUS_SPACE_MAP_PREFETCHABLE);
		return pa;
	}
	if (offset >= (fb->mmiobase & ~PAGE_MASK) &&
	    offset <= ((fb->mmiobase + fb->mmiosize + PAGE_SIZE - 1) & ~PAGE_MASK)) {
		pa = bus_space_mmap(fb->mmiot, fb->mmiobase, offset - fb->mmiobase,
		    prot, BUS_SPACE_MAP_LINEAR);
		return pa;
	}
	if (offset >= (fb->iobase & ~PAGE_MASK) &&
	    offset <= ((fb->iobase + fb->iosize + PAGE_SIZE - 1) & ~PAGE_MASK)) {
		pa = bus_space_mmap(fb->iot, fb->iobase, offset - fb->iobase,
		    prot, BUS_SPACE_MAP_LINEAR);
		return pa;
	}
	return -1;
}

void
sisfb_init_screen(void *cookie, struct vcons_screen *scr,
    int existing, long *defattr)
{
	struct sisfb_softc *sc = cookie;
	struct sisfb *fb = sc->sc_fb;
	struct rasops_info *ri = &scr->scr_ri;

	ri->ri_depth = fb->fb_depth;
	ri->ri_width = fb->fb_width;
	ri->ri_height = fb->fb_height;
	ri->ri_stride = fb->fb_stride;
	ri->ri_flg = RI_CENTER | RI_FULLCLEAR;

	ri->ri_bits = fb->fb_addr;

	if (existing) {
		ri->ri_flg |= RI_CLEAR;
	}

	rasops_init(ri, fb->fb_height / 8, fb->fb_width / 8);
	ri->ri_caps = WSSCREEN_WSCOLORS;
	rasops_reconfig(ri, fb->fb_height / ri->ri_font->fontheight,
		    fb->fb_width / ri->ri_font->fontwidth);

	ri->ri_hw = scr;
}


/*
 * Frame buffer initialization.
 */

int
sisfb_setup(struct sisfb *fb)
{
	struct rasops_info *ri = &fb->vcs.scr_ri;
	uint width, height, bpp;
	bus_size_t fbaddr;
	uint tmp;

	/*
	 * Unlock access to extended registers.
	 */

	sisfb_seq_write(fb, 0x05, 0x86);

	/*
	 * Try and figure out display settings.
	 */

	height = sisfb_crtc_read(fb, CRTC_VDE);
	tmp = sisfb_crtc_read(fb, CRTC_OVERFLL);
	if (ISSET(tmp, 1 << 1))
		height |= 1 << 8;
	if (ISSET(tmp, 1 << 6))
		height |= 1 << 9;
	tmp = sisfb_seq_read(fb, 0x0a);
	if (ISSET(tmp, 1 << 1))
		height |= 1 << 10;
	height++;

	width = sisfb_crtc_read(fb, CRTC_HDISPLE);
	tmp = sisfb_seq_read(fb, 0x0b);
	if (ISSET(tmp, 1 << 2))
		width |= 1 << 8;
	if (ISSET(tmp, 1 << 3))
		width |= 1 << 9;
	width++;
	width <<= 3;

#ifdef SIS_DEBUG
	printf("height %d width %d\n", height, width);
#endif

	fbaddr = sisfb_crtc_read(fb, CRTC_STARTADRL) |
	    (sisfb_crtc_read(fb, CRTC_STARTADRH) << 8) |
	    (sisfb_seq_read(fb, 0x0d) << 16) |
	    ((sisfb_seq_read(fb, 0x37) & 0x03) << 24);
	fbaddr <<= 2;
#ifdef SIS_DEBUG
	printf("FBADDR %lx\n", fbaddr);
#endif

	tmp = sisfb_seq_read(fb, 0x06);
	switch (tmp & 0x1c) {
	case 0x00:
		bpp = 8;
		break;
	case 0x04:
		bpp = 15;
		break;
	case 0x08:
		bpp = 16;
		break;
	case 0x10:
		bpp = 32;
		break;
	default:
		aprint_error("unknown bpp for 0x%x\n", tmp);
		return EINVAL;
	}
#ifdef SIS_DEBUG
	printf("BPP %d\n", bpp);
#endif

	fb->fb_width = ri->ri_width = width;
	fb->fb_height = ri->ri_height = height;
	fb->fb_depth = ri->ri_depth = bpp;
	fb->fb_stride = ri->ri_stride = (ri->ri_width * ri->ri_depth) / 8;
	ri->ri_flg = /* RI_CENTER | RI_CLEAR | RI_FULLCLEAR */ RI_CENTER | RI_NO_AUTO;
	fb->fb_addr = ri->ri_bits =
	    (void *)((char *)bus_space_vaddr(fb->fbt, fb->fbh) + fbaddr);
	ri->ri_hw = fb;

#ifdef SIS_DEBUG
	printf("ri_bits %p\n", ri->ri_bits);
#endif

#ifdef __MIPSEL__
	/* swap B and R */
	switch (bpp) {
	case 15:
		ri->ri_rnum = 5;
		ri->ri_rpos = 10;
		ri->ri_gnum = 5;
		ri->ri_gpos = 5;
		ri->ri_bnum = 5;
		ri->ri_bpos = 0;
		break;
	case 16:
		ri->ri_rnum = 5;
		ri->ri_rpos = 11;
		ri->ri_gnum = 6;
		ri->ri_gpos = 5;
		ri->ri_bnum = 5;
		ri->ri_bpos = 0;
		break;
	}
#endif

	bcopy(rasops_cmap, fb->cmap, sizeof(fb->cmap));
	if (bpp == 8) {
		sisfb_loadcmap(fb, 0, 256);
	}

	rasops_init(ri, 25, 80);
	rasops_reconfig(ri, ri->ri_height / ri->ri_font->fontheight,
	    ri->ri_width / ri->ri_font->fontwidth);

	fb->wsd.name = "std";
	fb->wsd.ncols = ri->ri_cols;
	fb->wsd.nrows = ri->ri_rows;
	fb->wsd.textops = &ri->ri_ops;
	fb->wsd.fontwidth = ri->ri_font->fontwidth;
	fb->wsd.fontheight = ri->ri_font->fontheight;
	fb->wsd.capabilities = ri->ri_caps;

	return 0;
}

/*
 * Colormap handling routines.
 */

void
sisfb_loadcmap(struct sisfb *fb, int baseidx, int count)
{
	uint8_t *cmap = fb->cmap + baseidx * 3;

	bus_space_write_1(fb->iot, fb->ioh, DAC_ADDR, baseidx);
	while (count-- != 0) {
		bus_space_write_1(fb->iot, fb->ioh, DAC_DATA, *cmap++ >> 2);
		bus_space_write_1(fb->iot, fb->ioh, DAC_DATA, *cmap++ >> 2);
		bus_space_write_1(fb->iot, fb->ioh, DAC_DATA, *cmap++ >> 2);
	}
}

int
sisfb_getcmap(uint8_t *cmap, struct wsdisplay_cmap *cm)
{
	uint index = cm->index, count = cm->count, i;
	uint8_t ramp[256], *dst, *src;
	int rc;

	if (index >= 256 || count > 256 - index)
		return EINVAL;

	index *= 3;

	src = cmap + index;
	dst = ramp;
	for (i = 0; i < count; i++)
		*dst++ = *src, src += 3;
	rc = copyout(ramp, cm->red, count);
	if (rc != 0)
		return rc;

	src = cmap + index + 1;
	dst = ramp;
	for (i = 0; i < count; i++)
		*dst++ = *src, src += 3;
	rc = copyout(ramp, cm->green, count);
	if (rc != 0)
		return rc;

	src = cmap + index + 2;
	dst = ramp;
	for (i = 0; i < count; i++)
		*dst++ = *src, src += 3;
	rc = copyout(ramp, cm->blue, count);
	if (rc != 0)
		return rc;

	return 0;
}

int
sisfb_putcmap(uint8_t *cmap, struct wsdisplay_cmap *cm)
{
	uint index = cm->index, count = cm->count, i;
	uint8_t ramp[256], *dst, *src;
	int rc;

	if (index >= 256 || count > 256 - index)
		return EINVAL;

	index *= 3;

	rc = copyin(cm->red, ramp, count);
	if (rc != 0)
		return rc;
	dst = cmap + index;
	src = ramp;
	for (i = 0; i < count; i++)
		*dst = *src++, dst += 3;

	rc = copyin(cm->green, ramp, count);
	if (rc != 0)
		return rc;
	dst = cmap + index + 1;
	src = ramp;
	for (i = 0; i < count; i++)
		*dst = *src++, dst += 3;

	rc = copyin(cm->blue, ramp, count);
	if (rc != 0)
		return rc;
	dst = cmap + index + 2;
	src = ramp;
	for (i = 0; i < count; i++)
		*dst = *src++, dst += 3;

	return 0;
}

/*
 * Early console code
 */

int
sisfb_cnattach(bus_space_tag_t memt, bus_space_tag_t iot,
    pci_chipset_tag_t pc, pcitag_t tag, pcireg_t id)
{
	long defattr;
	struct rasops_info * const ri = &sisfbcn.vcs.scr_ri;
	int flags;
	int rc;

	/* filter out unrecognized devices */
	switch (id) {
	default:
		return ENODEV;
	case PCI_ID_CODE(PCI_VENDOR_SIS, PCI_PRODUCT_SIS_315PRO_VGA):
		break;
	}

	if (pci_mapreg_info(pc, tag, PCI_MAPREG_START,
	    PCI_MAPREG_TYPE_MEM,
	    &sisfbcn.fbbase, &sisfbcn.fbsize, &flags)) {
		printf("sisfb can't map frame buffer\n");
		return ENODEV;
	}

	sisfbcn.fbt = memt;
	rc = bus_space_map(memt, sisfbcn.fbbase, sisfbcn.fbsize,
	    BUS_SPACE_MAP_LINEAR, &sisfbcn.fbh);
#ifdef SIS_DEBUG
	printf("sisfb_cnattach(memt, 0x%x rc %d\n", PCI_MAPREG_MEM_ADDR(bar), rc);
#endif
	if (rc != 0)
		return rc;

	if (pci_mapreg_info(pc, tag, PCI_MAPREG_START + 4,
	    PCI_MAPREG_TYPE_MEM,
	    &sisfbcn.mmiobase, &sisfbcn.mmiosize, &flags)) {
		printf("sisfb can't map mem space\n");
		return ENODEV;
	}
	sisfbcn.mmiot = memt;
	rc = bus_space_map(memt, sisfbcn.mmiobase, sisfbcn.mmiosize,
	    BUS_SPACE_MAP_LINEAR, &sisfbcn.mmioh);
#ifdef SIS_DEBUG
	printf("sisfb_cnattach(memt2, 0x%x rc %d\n", PCI_MAPREG_MEM_ADDR(bar), rc);
#endif
	if (rc != 0)
		return rc;

	if (pci_mapreg_info(pc, tag, PCI_MAPREG_START + 8,
	    PCI_MAPREG_TYPE_IO,
	    &sisfbcn.iobase, &sisfbcn.iosize, &flags)) {
		printf("sisfb can't map mem space\n");
		return ENODEV;
	}
	sisfbcn.iot = iot;
	rc = bus_space_map(iot, sisfbcn.iobase, sisfbcn.iosize,
	    0, &sisfbcn.ioh);
#ifdef SIS_DEBUG
	printf("sisfb_cnattach(iot, 0x%x rc %d\n", PCI_MAPREG_MEM_ADDR(bar), rc);
#endif
	if (rc != 0)
		return rc;

	rc = sisfb_setup(&sisfbcn);
#ifdef SIS_DEBUG
	printf("sisfb_setup %d %p\n", rc, sisfbcn.vcs.scr_ri.ri_hw);
#endif
	if (rc != 0)
		return rc;

	ri->ri_ops.allocattr(ri, 0,  ri->ri_rows - 1, 0, &defattr);
	wsdisplay_preattach(&sisfbcn.wsd, ri, 0, 0, defattr);

	return 0;
}
