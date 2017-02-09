/*	$NetBSD: cgtwelve.c,v 1.5 2012/01/11 16:08:57 macallan Exp $ */

/*-
 * Copyright (c) 2010 Michael Lorenz
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

/* a console driver for the Sun CG12 / Matrox SG3 graphics board */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: cgtwelve.c,v 1.5 2012/01/11 16:08:57 macallan Exp $");

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

#include <dev/sbus/cgtwelvereg.h>
#include <dev/ic/bt462reg.h>

#include "opt_wsemul.h"
#include "opt_cgtwelve.h"


struct cgtwelve_softc {
	device_t	sc_dev;
	bus_space_tag_t sc_tag;
	bus_space_handle_t sc_regh;
	bus_addr_t	sc_paddr;
	void		*sc_fbaddr;
	void		*sc_shadow;
	uint8_t		*sc_wids;
	void		*sc_int;
	int		sc_width;
	int		sc_height;
	int		sc_stride;
	int		sc_fbsize;
	int		sc_mode;
	struct vcons_data vd;
};

static int	cgtwelve_match(device_t, cfdata_t, void *);
static void	cgtwelve_attach(device_t, device_t, void *);
static int	cgtwelve_ioctl(void *, void *, u_long, void *, int,
				 struct lwp*);
static paddr_t	cgtwelve_mmap(void *, void *, off_t, int);
static void	cgtwelve_init_screen(void *, struct vcons_screen *, int,
				 long *);
static void	cgtwelve_write_wid(struct cgtwelve_softc *, int, uint8_t);
static void	cgtwelve_select_ovl(struct cgtwelve_softc *, int);
#define CG12_SEL_OVL	0
#define CG12_SEL_ENABLE	1
#define CG12_SEL_8BIT	2
#define CG12_SEL_24BIT	3
#define CG12_SEL_WID	4
static void	cgtwelve_write_dac(struct cgtwelve_softc *, int, int, int, int);
static void	cgtwelve_setup(struct cgtwelve_softc *, int);

CFATTACH_DECL_NEW(cgtwelve, sizeof(struct cgtwelve_softc),
    cgtwelve_match, cgtwelve_attach, NULL, NULL);

struct wsscreen_descr cgtwelve_defscreendesc = {
	"default",
	0, 0,
	NULL,
	8, 16,
	0,
};

static struct vcons_screen cgtwelve_console_screen;

const struct wsscreen_descr *_cgtwelve_scrlist[] = {
	&cgtwelve_defscreendesc,
	/* XXX other formats, graphics screen? */
};

struct wsscreen_list cgtwelve_screenlist = {
	sizeof(_cgtwelve_scrlist) / sizeof(struct wsscreen_descr *),
	_cgtwelve_scrlist
};

struct wsdisplay_accessops cgtwelve_accessops = {
	cgtwelve_ioctl,
	cgtwelve_mmap,
	NULL,	/* vcons_alloc_screen */
	NULL,	/* vcons_free_screen */
	NULL,	/* vcons_show_screen */
	NULL,	/* load_font */
	NULL,	/* polls */
	NULL,	/* scroll */
};

extern const u_char rasops_cmap[768];

static int
cgtwelve_match(device_t parent, cfdata_t cf, void *aux)
{
	struct sbus_attach_args *sa = aux;

	if (strcmp("cgtwelve", sa->sa_name) == 0)
		return 100;
	return 0;
}

/*
 * Attach a display.  We need to notice if it is the console, too.
 */
static void
cgtwelve_attach(device_t parent, device_t self, void *args)
{
	struct cgtwelve_softc *sc = device_private(self);
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

	sc->sc_paddr = sbus_bus_addr(sa->sa_bustag, sa->sa_slot, sa->sa_offset);

	/* read geometry information from the device tree */
	sc->sc_width = prom_getpropint(sa->sa_node, "width", 1152);
	sc->sc_height = prom_getpropint(sa->sa_node, "height", 900);
#ifdef CG12_COLOR
	sc->sc_stride = sc->sc_width;
#else
	sc->sc_stride = (sc->sc_width + 7) >> 3;
#endif
	sc->sc_fbsize = sc->sc_height * sc->sc_stride;

	sc->sc_fbaddr = (void *)prom_getpropint(sa->sa_node, "address", 0);
	if (sc->sc_fbaddr == NULL) {
		if (sbus_bus_map(sa->sa_bustag,
			 sa->sa_slot,
			 sa->sa_offset + CG12_FB_MONO,
			 sc->sc_fbsize,
			 BUS_SPACE_MAP_LINEAR, &bh) != 0) {
			aprint_error_dev(self, "cannot map framebuffer\n");
			return;
		}
		sc->sc_fbaddr = bus_space_vaddr(sa->sa_bustag, bh);
	}
		
	aprint_normal_dev(self, "%d x %d\n", sc->sc_width, sc->sc_height);

	if (sbus_bus_map(sa->sa_bustag,
			 sa->sa_slot,
			 sa->sa_offset + CG12_OFF_REGISTERS,
			 0xc0000, 0, &sc->sc_regh) != 0) {
		aprint_error("%s: couldn't map registers\n", 
		    device_xname(sc->sc_dev));
		return;
	}

	if (sbus_bus_map(sa->sa_bustag,
			 sa->sa_slot,
			 sa->sa_offset + CG12_OFF_WID, 0x100000, 
			 BUS_SPACE_MAP_LINEAR | BUS_SPACE_MAP_LARGE,
			 &bh) != 0) {
		aprint_error("%s: couldn't map WID\n", 
		    device_xname(sc->sc_dev));
		return;
	}
	sc->sc_wids = bus_space_vaddr(sa->sa_bustag, bh);

	if (sbus_bus_map(sa->sa_bustag,
			 sa->sa_slot,
			 sa->sa_offset + CG12_OFF_INTEN, 0x400000, 
			 BUS_SPACE_MAP_LINEAR | BUS_SPACE_MAP_LARGE,
			 &bh) != 0) {
		aprint_error("%s: couldn't map colour fb\n", 
		    device_xname(sc->sc_dev));
		return;
	}
	sc->sc_int = bus_space_vaddr(sa->sa_bustag, bh);

#ifdef CG12_COLOR
	cgtwelve_setup(sc, 8);
#else
	cgtwelve_setup(sc, 1);
#endif
#ifdef CG12_SHADOW
	sc->sc_shadow = kmem_alloc(sc->sc_fbsize, KM_SLEEP);
#else
	sc->sc_shadow = NULL;
#endif

	isconsole = fb_is_console(node);

	sc->sc_mode = WSDISPLAYIO_MODE_EMUL;
	wsfont_init();

	vcons_init(&sc->vd, sc, &cgtwelve_defscreendesc, &cgtwelve_accessops);
	sc->vd.init_screen = cgtwelve_init_screen;

	vcons_init_screen(&sc->vd, &cgtwelve_console_screen, 1, &defattr);
	cgtwelve_console_screen.scr_flags |= VCONS_SCREEN_IS_STATIC;

	ri = &cgtwelve_console_screen.scr_ri;

	cgtwelve_defscreendesc.nrows = ri->ri_rows;
	cgtwelve_defscreendesc.ncols = ri->ri_cols;
	cgtwelve_defscreendesc.textops = &ri->ri_ops;
	cgtwelve_defscreendesc.capabilities = ri->ri_caps;

	if(isconsole) {
		wsdisplay_cnattach(&cgtwelve_defscreendesc, ri, 0, 0, defattr);
		vcons_replay_msgbuf(&cgtwelve_console_screen);
	}

	aa.console = isconsole;
	aa.scrdata = &cgtwelve_screenlist;
	aa.accessops = &cgtwelve_accessops;
	aa.accesscookie = &sc->vd;

	config_found(self, &aa, wsemuldisplaydevprint);
#ifdef CG12_DEBUG
	{
		int i;
		for (i = 0; i < 0x10; i++) {
			bus_space_write_4(sc->sc_tag, sc->sc_regh, 
			    CG12DAC_ADDR0, (i << 16) | (i << 8) | i);
			bus_space_write_4(sc->sc_tag, sc->sc_regh, 
			    CG12DAC_ADDR1, 0x010101);
			printf("%02x: %08x\n", i, bus_space_read_4(sc->sc_tag, 
			    sc->sc_regh, CG12DAC_CTRL));
		}
	}
#endif
}

/* 0 - overlay plane, 1 - enable plane, 2 - 8bit fb, 3 - 24bit fb, 4 - WIDs */
static void
cgtwelve_select_ovl(struct cgtwelve_softc *sc, int which)
{
	switch(which) {
		case 0:
			bus_space_write_4(sc->sc_tag, sc->sc_regh,
			    CG12DPU_PLN_RDMSK_HOST, CG12_PLN_RD_OVERLAY);
			bus_space_write_4(sc->sc_tag, sc->sc_regh,
			    CG12DPU_PLN_WRMSK_HOST, CG12_PLN_WR_OVERLAY);
			bus_space_write_4(sc->sc_tag, sc->sc_regh,
			    CG12DPU_PLN_SL_HOST, CG12_PLN_SL_OVERLAY);
			bus_space_write_4(sc->sc_tag, sc->sc_regh,
			    CG12APU_HPAGE, CG12_HPAGE_OVERLAY);
			bus_space_write_4(sc->sc_tag, sc->sc_regh,
			    CG12APU_HACCESS, CG12_HACCESS_OVERLAY);
			break;
		case 1:
			bus_space_write_4(sc->sc_tag, sc->sc_regh,
			    CG12DPU_PLN_RDMSK_HOST, CG12_PLN_RD_ENABLE);
			bus_space_write_4(sc->sc_tag, sc->sc_regh,
			    CG12DPU_PLN_WRMSK_HOST, CG12_PLN_WR_ENABLE);
			bus_space_write_4(sc->sc_tag, sc->sc_regh,
			    CG12DPU_PLN_SL_HOST, CG12_PLN_SL_ENABLE);
			bus_space_write_4(sc->sc_tag, sc->sc_regh,
			    CG12APU_HPAGE, CG12_HPAGE_ENABLE);
			bus_space_write_4(sc->sc_tag, sc->sc_regh,
			    CG12APU_HACCESS, CG12_HACCESS_ENABLE);
			break;
		case 2:
			bus_space_write_4(sc->sc_tag, sc->sc_regh,
			    CG12DPU_PLN_RDMSK_HOST, CG12_PLN_RD_8BIT);
			bus_space_write_4(sc->sc_tag, sc->sc_regh,
			    CG12DPU_PLN_WRMSK_HOST, CG12_PLN_WR_8BIT);
			bus_space_write_4(sc->sc_tag, sc->sc_regh,
			    CG12DPU_PLN_SL_HOST, CG12_PLN_SL_8BIT);
			bus_space_write_4(sc->sc_tag, sc->sc_regh,
			    CG12APU_HPAGE, CG12_HPAGE_8BIT);
			bus_space_write_4(sc->sc_tag, sc->sc_regh,
			    CG12APU_HACCESS, CG12_HACCESS_8BIT);
			break;
		case 3:
			bus_space_write_4(sc->sc_tag, sc->sc_regh,
			    CG12DPU_PLN_RDMSK_HOST, CG12_PLN_RD_24BIT);
			bus_space_write_4(sc->sc_tag, sc->sc_regh,
			    CG12DPU_PLN_WRMSK_HOST, CG12_PLN_WR_24BIT);
			bus_space_write_4(sc->sc_tag, sc->sc_regh,
			    CG12DPU_PLN_SL_HOST, CG12_PLN_SL_24BIT);
			bus_space_write_4(sc->sc_tag, sc->sc_regh,
			    CG12APU_HPAGE, CG12_HPAGE_24BIT);
			bus_space_write_4(sc->sc_tag, sc->sc_regh,
			    CG12APU_HACCESS, CG12_HACCESS_24BIT);
			break;
		case 4:
			bus_space_write_4(sc->sc_tag, sc->sc_regh,
			    CG12DPU_PLN_RDMSK_HOST, CG12_PLN_RD_WID);
			bus_space_write_4(sc->sc_tag, sc->sc_regh,
			    CG12DPU_PLN_WRMSK_HOST, CG12_PLN_WR_WID);
			bus_space_write_4(sc->sc_tag, sc->sc_regh,
			    CG12DPU_PLN_SL_HOST, CG12_PLN_SL_WID);
			bus_space_write_4(sc->sc_tag, sc->sc_regh,
			    CG12APU_HPAGE, CG12_HPAGE_WID);
			bus_space_write_4(sc->sc_tag, sc->sc_regh,
			    CG12APU_HACCESS, CG12_HACCESS_WID);
			break;
	}
}

static void
cgtwelve_write_wid(struct cgtwelve_softc *sc, int idx, uint8_t wid)
{
	bus_space_write_4(sc->sc_tag, sc->sc_regh, CG12_WSC_ADDR, idx << 16);
	bus_space_write_4(sc->sc_tag, sc->sc_regh, CG12_WSC_DATA, 
	    ((uint32_t)wid) << 16);
}

static void
cgtwelve_write_dac(struct cgtwelve_softc *sc, int idx, int r, int g, int b)
{
	uint32_t lo = (idx & 0xff);
	uint32_t hi = (idx >> 8) & 0xff;

	lo |= lo << 8 | lo << 16;
	hi |= hi << 8 | hi << 16;
	bus_space_write_4(sc->sc_tag, sc->sc_regh, CG12DAC_ADDR0, lo);
	bus_space_write_4(sc->sc_tag, sc->sc_regh, CG12DAC_ADDR1, hi);
	bus_space_write_4(sc->sc_tag, sc->sc_regh, CG12DAC_DATA,
	    b << 16 | g << 8 | r);
}

static void
cgtwelve_setup(struct cgtwelve_softc *sc, int depth)
{
	int i, j;

	/* first let's put some stuff into the WID table */
	cgtwelve_write_wid(sc, 0, CG12_WID_8_BIT);
	cgtwelve_write_wid(sc, 1, CG12_WID_24_BIT);
	
	/* a linear ramp for the gamma table */
	for (i = 0; i < 256; i++)
		cgtwelve_write_dac(sc, i + 0x100, i, i, i);

	j = 0;
	/* rasops' ANSI colour map */
	for (i = 0; i < 256; i++) {
		cgtwelve_write_dac(sc, i,
		    rasops_cmap[j],
		    rasops_cmap[j + 1],
		    rasops_cmap[j + 2]);
		j += 3;
	}

	switch(depth) {
	case 1:
		/* setup the console */

		/* first, make the overlay all opaque */
		cgtwelve_select_ovl(sc, CG12_SEL_ENABLE);
		memset(sc->sc_fbaddr, 0xff, 0x20000);

		/* now write the right thing into the WID plane */
		cgtwelve_select_ovl(sc, CG12_SEL_WID);
		memset(sc->sc_wids, 0, 0x100000);

		/* now clean the plane */
		cgtwelve_select_ovl(sc, CG12_SEL_OVL);
		memset(sc->sc_fbaddr, 0, 0x20000);
		break;
	case 8:
		/* setup the 8bit fb */
		/*
		 * first clean the 8bit fb - for aesthetic reasons do it while
		 * it's still not visible ( we hope... )
		 */
		cgtwelve_select_ovl(sc, CG12_SEL_8BIT);
		memset(sc->sc_int, 0x00, 0x100000);

		/* now write the right thing into the WID plane */
		cgtwelve_select_ovl(sc, CG12_SEL_WID);
		memset(sc->sc_wids, 0, 0x100000);

		/* hide the overlay */
		cgtwelve_select_ovl(sc, CG12_SEL_ENABLE);
		memset(sc->sc_fbaddr, 0, 0x20000);

		/* now clean the plane */
		cgtwelve_select_ovl(sc, CG12_SEL_OVL);
		memset(sc->sc_fbaddr, 0, 0x20000);

		/* and make sure we can write the 24bit fb */
		cgtwelve_select_ovl(sc, CG12_SEL_8BIT);
		break;
	case 24:
	case 32:
		/* setup the 24bit fb for X */
		/*
		 * first clean the 24bit fb - for aesthetic reasons do it while
		 * it's still not visible ( we hope... )
		 */
		cgtwelve_select_ovl(sc, CG12_SEL_24BIT);
		memset(sc->sc_int, 0x80, 0x400000);

		/* now write the right thing into the WID plane */
		cgtwelve_select_ovl(sc, CG12_SEL_WID);
		memset(sc->sc_wids, 1, 0x100000);

		/* hide the overlay */
		cgtwelve_select_ovl(sc, CG12_SEL_ENABLE);
		memset(sc->sc_fbaddr, 0, 0x20000);

		/* now clean the plane */
		cgtwelve_select_ovl(sc, CG12_SEL_OVL);
		memset(sc->sc_fbaddr, 0, 0x20000);

		/* and make sure we can write the 24bit fb */
		cgtwelve_select_ovl(sc, CG12_SEL_24BIT);
		break;
	}
}

static void
cgtwelve_init_screen(void *cookie, struct vcons_screen *scr,
    int existing, long *defattr)
{
	struct cgtwelve_softc *sc = cookie;
	struct rasops_info *ri = &scr->scr_ri;

#ifdef CG12_COLOR
	ri->ri_depth = 8;
#else
	ri->ri_depth = 1;
#endif
	ri->ri_width = sc->sc_width;
	ri->ri_height = sc->sc_height;
	ri->ri_stride = sc->sc_stride;
	ri->ri_flg = RI_CENTER;

	if (sc->sc_shadow == NULL) {
#ifdef CG12_COLOR
		ri->ri_bits = sc->sc_int;
#else
		ri->ri_bits = sc->sc_fbaddr;
#endif
		scr->scr_flags |= VCONS_DONT_READ;
	} else {
#ifdef CG12_COLOR
		ri->ri_hwbits = sc->sc_int;
#else
		ri->ri_hwbits = sc->sc_fbaddr;
#endif
		ri->ri_bits = sc->sc_shadow;
	}

	rasops_init(ri, 0, 0);
#ifdef CG12_COLOR
	ri->ri_caps = WSSCREEN_REVERSE | WSSCREEN_WSCOLORS;
#else
	ri->ri_caps = WSSCREEN_REVERSE;
#endif
	rasops_reconfig(ri, ri->ri_height / ri->ri_font->fontheight,
		    ri->ri_width / ri->ri_font->fontwidth);
}

static int
cgtwelve_ioctl(void *v, void *vs, u_long cmd, void *data, int flag,
    struct lwp *l)
{
	struct vcons_data *vd = v;
	struct cgtwelve_softc *sc = vd->cookie;
	struct wsdisplay_fbinfo *wdf;
	struct vcons_screen *ms = vd->active;

	switch (cmd) {
		case WSDISPLAYIO_GTYPE:
			*(u_int *)data = WSDISPLAY_TYPE_SUNCG12;
			return 0;

		case WSDISPLAYIO_GINFO:
			wdf = (void *)data;
			wdf->height = sc->sc_height;
			wdf->width = sc->sc_width;
			wdf->depth = 32;
			wdf->cmsize = 256;
			return 0;

		case FBIOGVIDEO:
		case WSDISPLAYIO_GVIDEO:
			*(int *)data = 1;
			return 0;

		case WSDISPLAYIO_SVIDEO:
		case FBIOSVIDEO:
			/* when we figure out how to do this... */
			/*cgtwelve_set_video(sc, *(int *)data);*/
			return 0;

		case WSDISPLAYIO_LINEBYTES:
			{
				int *ret = (int *)data;
				*ret = sc->sc_width << 2;
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
#ifdef CG12_COLOR
						cgtwelve_setup(sc, 8);
#else
						cgtwelve_setup(sc, 1);
#endif
						vcons_redraw_screen(ms);
					} else {
						cgtwelve_setup(sc, 32);
					}
				}
			}
	}

	return EPASSTHROUGH;
}

static paddr_t
cgtwelve_mmap(void *v, void *vs, off_t offset, int prot)
{
	struct vcons_data *vd = v;
	struct cgtwelve_softc *sc = vd->cookie;

	/* regular fb mapping at 0 */
	if ((offset >= 0) && (offset < 0x400000)) {
		return bus_space_mmap(sc->sc_tag, sc->sc_paddr + CG12_OFF_INTEN,
		    offset, prot, BUS_SPACE_MAP_LINEAR);
	}

	return -1;
}
