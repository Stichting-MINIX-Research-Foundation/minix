/*	$NetBSD: p9100.c,v 1.62 2014/07/25 08:10:38 dholland Exp $ */

/*-
 * Copyright (c) 1998, 2005, 2006 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Matt Thomas.
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
 * color display (p9100) driver.
 *
 * Does not handle interrupts, even though they can occur.
 *
 * XXX should defer colormap updates to vertical retrace interrupts
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: p9100.c,v 1.62 2014/07/25 08:10:38 dholland Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>
#include <sys/mman.h>
#include <sys/tty.h>
#include <sys/conf.h>

#include <sys/bus.h>
#include <machine/autoconf.h>

#include <dev/sun/fbio.h>
#include <dev/sun/fbvar.h>
#include <dev/sun/btreg.h>
#include <dev/sun/btvar.h>

#include <dev/sbus/p9100reg.h>

#include <dev/sbus/sbusvar.h>

#include <dev/wscons/wsdisplayvar.h>
#include <dev/wscons/wsconsio.h>
#include <dev/wsfont/wsfont.h>
#include <dev/rasops/rasops.h>

#include <dev/wscons/wsdisplay_vconsvar.h>
#include <dev/wscons/wsdisplay_glyphcachevar.h>

#include "opt_wsemul.h"
#include "rasops_glue.h"
#include "opt_pnozz.h"

#include "ioconf.h"

#include "tctrl.h"
#if NTCTRL > 0
#include <machine/tctrl.h>
#include <sparc/dev/tctrlvar.h>	/*XXX*/
#endif

#ifdef PNOZZ_DEBUG
#define DPRINTF aprint_normal
#else
#define DPRINTF while (0) aprint_normal
#endif

struct pnozz_cursor {
	short	pc_enable;		/* cursor is enabled */
	struct	fbcurpos pc_pos;	/* position */
	struct	fbcurpos pc_hot;	/* hot-spot */
	struct	fbcurpos pc_size;	/* size of mask & image fields */
	uint32_t pc_bits[0x100];	/* space for mask & image bits */
	unsigned char red[3], green[3];
	unsigned char blue[3];		/* cursor palette */
};

/* per-display variables */
struct p9100_softc {
	device_t	sc_dev;		/* base device */
	struct fbdevice	sc_fb;		/* frame buffer device */

	bus_space_tag_t	sc_bustag;

	bus_addr_t	sc_ctl_paddr;	/* phys address description */
	bus_size_t	sc_ctl_psize;	/*   for device mmap() */
	bus_space_handle_t sc_ctl_memh;	/*   bus space handle */

	bus_addr_t	sc_fb_paddr;	/* phys address description */
	bus_size_t	sc_fb_psize;	/*   for device mmap() */
#ifdef PNOZZ_USE_LATCH
	bus_space_handle_t sc_fb_memh;	/*   bus space handle */
#endif
	uint32_t 	sc_mono_width;	/* for setup_mono */

	uint32_t	sc_width;
	uint32_t	sc_height;	/* panel width / height */
	uint32_t	sc_stride;
	uint32_t	sc_depth;
	int		sc_depthshift;	/* blitter works on bytes not pixels */
	
	union	bt_cmap sc_cmap;	/* Brooktree color map */

	struct pnozz_cursor sc_cursor;

	int 		sc_mode;
	int 		sc_video, sc_powerstate;
	uint32_t 	sc_bg;
	volatile uint32_t sc_last_offset;
	struct vcons_data vd;
	uint8_t		sc_dac_power;
	glyphcache	sc_gc;
};


static struct vcons_screen p9100_console_screen;

extern const u_char rasops_cmap[768];

struct wsscreen_descr p9100_defscreendesc = {
	"default",
	0, 0,
	NULL,
	8, 16,
	WSSCREEN_WSCOLORS,
};

const struct wsscreen_descr *_p9100_scrlist[] = {
	&p9100_defscreendesc,
	/* XXX other formats, graphics screen? */
};

struct wsscreen_list p9100_screenlist = {
	sizeof(_p9100_scrlist) / sizeof(struct wsscreen_descr *),
	_p9100_scrlist
};

/* autoconfiguration driver */
static int	p9100_sbus_match(device_t, cfdata_t, void *);
static void	p9100_sbus_attach(device_t, device_t, void *);

static void	p9100unblank(device_t);

CFATTACH_DECL_NEW(pnozz, sizeof(struct p9100_softc),
    p9100_sbus_match, p9100_sbus_attach, NULL, NULL);

static dev_type_open(p9100open);
static dev_type_close(p9100close);
static dev_type_ioctl(p9100ioctl);
static dev_type_mmap(p9100mmap);

const struct cdevsw pnozz_cdevsw = {
	.d_open = p9100open,
	.d_close = nullclose,
	.d_read = noread,
	.d_write = nowrite,
	.d_ioctl = p9100ioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = nopoll,
	.d_mmap = p9100mmap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = 0
};

/* frame buffer generic driver */
static struct fbdriver p9100fbdriver = {
	p9100unblank, p9100open, p9100close, p9100ioctl, nopoll,
	p9100mmap, nokqfilter
};

static void	p9100loadcmap(struct p9100_softc *, int, int);
static void	p9100_set_video(struct p9100_softc *, int);
static int	p9100_get_video(struct p9100_softc *);
static uint32_t p9100_ctl_read_4(struct p9100_softc *, bus_size_t);
static void	p9100_ctl_write_4(struct p9100_softc *, bus_size_t, uint32_t);
static uint8_t	p9100_ramdac_read(struct p9100_softc *, bus_size_t);
static void	p9100_ramdac_write(struct p9100_softc *, bus_size_t, uint8_t);

static uint8_t	p9100_ramdac_read_ctl(struct p9100_softc *, int);
static void	p9100_ramdac_write_ctl(struct p9100_softc *, int, uint8_t);

static void 	p9100_init_engine(struct p9100_softc *);
static int	p9100_set_depth(struct p9100_softc *, int);

#if NWSDISPLAY > 0
static void	p9100_sync(struct p9100_softc *);
static void	p9100_bitblt(void *, int, int, int, int, int, int, int);
static void 	p9100_rectfill(void *, int, int, int, int, uint32_t);
static void	p9100_clearscreen(struct p9100_softc *);

static void	p9100_setup_mono(struct p9100_softc *, int, int, int, int,
		    uint32_t, uint32_t);
static void	p9100_feed_line(struct p9100_softc *, int, uint8_t *);
static void	p9100_set_color_reg(struct p9100_softc *, int, int32_t);

static void	p9100_copycols(void *, int, int, int, int);
static void	p9100_erasecols(void *, int, int, int, long);
static void	p9100_copyrows(void *, int, int, int);
static void	p9100_eraserows(void *, int, int, long);
/*static int	p9100_mapchar(void *, int, u_int *);*/
static void	p9100_putchar(void *, int, int, u_int, long);
static void	p9100_putchar_aa(void *, int, int, u_int, long);
static void	p9100_cursor(void *, int, int, int);

static int	p9100_putcmap(struct p9100_softc *, struct wsdisplay_cmap *);
static int 	p9100_getcmap(struct p9100_softc *, struct wsdisplay_cmap *);
static int	p9100_ioctl(void *, void *, u_long, void *, int, struct lwp *);
static paddr_t	p9100_mmap(void *, void *, off_t, int);

/*static int	p9100_load_font(void *, void *, struct wsdisplay_font *);*/

static void	p9100_init_screen(void *, struct vcons_screen *, int,
		    long *);
#endif

static void	p9100_init_cursor(struct p9100_softc *);

static void	p9100_set_fbcursor(struct p9100_softc *);
static void	p9100_setcursorcmap(struct p9100_softc *);
static void	p9100_loadcursor(struct p9100_softc *);

#if 0
static int	p9100_intr(void *);
#endif

/* power management stuff */
static bool p9100_suspend(device_t, const pmf_qual_t *);
static bool p9100_resume(device_t, const pmf_qual_t *);

#if NTCTRL > 0
static void p9100_set_extvga(void *, int);
#endif

#if NWSDISPLAY > 0
struct wsdisplay_accessops p9100_accessops = {
	p9100_ioctl,
	p9100_mmap,
	NULL,	/* vcons_alloc_screen */
	NULL,	/* vcons_free_screen */
	NULL,	/* vcons_show_screen */
	NULL,	/* load_font */
	NULL,	/* polls */
	NULL,	/* scroll */
};
#endif

#ifdef PNOZZ_USE_LATCH
#define PNOZZ_LATCH(sc, off) if(sc->sc_last_offset != (off & 0xffffff80)) { \
	(void)bus_space_read_4(sc->sc_bustag, sc->sc_fb_memh, off); \
	sc->sc_last_offset = off & 0xffffff80; }
#else
#define PNOZZ_LATCH(a, b)
#endif

/*
 * Match a p9100.
 */
static int
p9100_sbus_match(device_t parent, cfdata_t cf, void *aux)
{
	struct sbus_attach_args *sa = aux;

	if (strcmp("p9100", sa->sa_name) == 0)
		return 100;
	return 0;
}


/*
 * Attach a display.  We need to notice if it is the console, too.
 */
static void
p9100_sbus_attach(device_t parent, device_t self, void *args)
{
	struct p9100_softc *sc = device_private(self);
	struct sbus_attach_args *sa = args;
	struct fbdevice *fb = &sc->sc_fb;
	int isconsole;
	int node = sa->sa_node;
	int i, j;
	uint8_t ver, cmap[768];

#if NWSDISPLAY > 0
	struct wsemuldisplaydev_attach_args aa;
	struct rasops_info *ri;
	unsigned long defattr;
#endif

	sc->sc_last_offset = 0xffffffff;
	sc->sc_dev = self;

	/*
	 * When the ROM has mapped in a p9100 display, the address
	 * maps only the video RAM, so in any case we have to map the
	 * registers ourselves.
	 */

	if (sa->sa_npromvaddrs != 0)
		fb->fb_pixels = (void *)sa->sa_promvaddrs[0];

	/* Remember cookies for p9100_mmap() */
	sc->sc_bustag = sa->sa_bustag;

	sc->sc_ctl_paddr = sbus_bus_addr(sa->sa_bustag,
		sa->sa_reg[0].oa_space, sa->sa_reg[0].oa_base);
	sc->sc_ctl_psize = 0x8000;/*(bus_size_t)sa->sa_reg[0].oa_size;*/

	sc->sc_fb_paddr = sbus_bus_addr(sa->sa_bustag,
		sa->sa_reg[2].oa_space, sa->sa_reg[2].oa_base);
	sc->sc_fb_psize = (bus_size_t)sa->sa_reg[2].oa_size;

	if (sbus_bus_map(sc->sc_bustag,
	    sa->sa_reg[0].oa_space,
	    sa->sa_reg[0].oa_base,
	    /*
	     * XXX for some reason the SBus resources don't cover
	     * all registers, so we just map what we need
	     */
	    0x8000,
	    0, &sc->sc_ctl_memh) != 0) {
		printf("%s: cannot map control registers\n",
		    device_xname(self));
		return;
	}

	/*
	 * we need to map the framebuffer even though we never write to it,
	 * thanks to some weirdness in the SPARCbook's SBus glue for the
	 * P9100 - all register accesses need to be 'latched in' whenever we
	 * go to another 0x80 aligned 'page' by reading the framebuffer at the
	 * same offset
	 * XXX apparently the latter isn't true - my SP3GX works fine without
	 */
#ifdef PNOZZ_USE_LATCH
	if (fb->fb_pixels == NULL) {
		if (sbus_bus_map(sc->sc_bustag,
		    sa->sa_reg[2].oa_space,
		    sa->sa_reg[2].oa_base,
		    sc->sc_fb_psize,
		    BUS_SPACE_MAP_LINEAR | BUS_SPACE_MAP_LARGE,
		    &sc->sc_fb_memh) != 0) {
			printf("%s: cannot map framebuffer\n",
			    device_xname(self));
			return;
		}
		fb->fb_pixels = (char *)sc->sc_fb_memh;
	} else {
		sc->sc_fb_memh = (bus_space_handle_t) fb->fb_pixels;
	}
#endif
	sc->sc_width = prom_getpropint(node, "width", 800);
	sc->sc_height = prom_getpropint(node, "height", 600);
	sc->sc_depth = prom_getpropint(node, "depth", 8) >> 3;

	sc->sc_stride = prom_getpropint(node, "linebytes",
	    sc->sc_width * sc->sc_depth);

	fb->fb_driver = &p9100fbdriver;
	fb->fb_device = sc->sc_dev;
	fb->fb_flags = device_cfdata(sc->sc_dev)->cf_flags & FB_USERMASK;
#ifdef PNOZZ_EMUL_CG3
	fb->fb_type.fb_type = FBTYPE_SUN3COLOR;
#else
	fb->fb_type.fb_type = FBTYPE_P9100;
#endif
	fb->fb_pixels = NULL;

	sc->sc_mode = WSDISPLAYIO_MODE_EMUL;

	isconsole = fb_is_console(node);
#if 0
	if (!isconsole) {
		aprint_normal("\n");
		aprint_error_dev(self, "fatal error: PROM didn't configure device\n");
		return;
	}
#endif

    	fb->fb_type.fb_depth = 8;
	sc->sc_depth = 1;
	sc->sc_depthshift = 0;

	/* check the RAMDAC */
	ver = p9100_ramdac_read_ctl(sc, DAC_VERSION);

	p9100_init_engine(sc);
	p9100_set_depth(sc, 8);
	
	fb_setsize_obp(fb, fb->fb_type.fb_depth, sc->sc_width, sc->sc_height,
	    node);

#if 0
	bus_intr_establish(sc->sc_bustag, sa->sa_pri, IPL_BIO,
	    p9100_intr, sc);
#endif

	fb->fb_type.fb_cmsize = prom_getpropint(node, "cmsize", 256);
	if ((1 << fb->fb_type.fb_depth) != fb->fb_type.fb_cmsize)
		printf(", %d entry colormap", fb->fb_type.fb_cmsize);

	/* make sure we are not blanked */
	if (isconsole)
		p9100_set_video(sc, 1);

	/* register with power management */
	sc->sc_video = 1;
	sc->sc_powerstate = PWR_RESUME;
	if (!pmf_device_register(self, p9100_suspend, p9100_resume)) {
		panic("%s: could not register with PMF",
		      device_xname(sc->sc_dev));
	}

	if (isconsole) {
		printf(" (console)\n");
#ifdef RASTERCONSOLE
		/*p9100loadcmap(sc, 255, 1);*/
		fbrcons_init(fb);
#endif
	} else
		printf("\n");

#if NWSDISPLAY > 0
	wsfont_init();

#ifdef PNOZZ_DEBUG
	/* make the glyph cache visible */
	sc->sc_height -= 100;
#endif

	sc->sc_gc.gc_bitblt = p9100_bitblt;
	sc->sc_gc.gc_blitcookie = sc;
	sc->sc_gc.gc_rop = ROP_SRC;

	vcons_init(&sc->vd, sc, &p9100_defscreendesc, &p9100_accessops);
	sc->vd.init_screen = p9100_init_screen;

	vcons_init_screen(&sc->vd, &p9100_console_screen, 1, &defattr);
	p9100_console_screen.scr_flags |= VCONS_SCREEN_IS_STATIC;

	/* Initialize the default color map. */
	rasops_get_cmap(&p9100_console_screen.scr_ri, cmap, 768);

	j = 0;
	for (i = 0; i < 256; i++) {
		sc->sc_cmap.cm_map[i][0] = cmap[j];
		j++;
		sc->sc_cmap.cm_map[i][1] = cmap[j];
		j++;
		sc->sc_cmap.cm_map[i][2] = cmap[j];
		j++;
	}
	p9100loadcmap(sc, 0, 256);

	sc->sc_bg = (defattr >> 16) & 0xff;
	p9100_clearscreen(sc);

	ri = &p9100_console_screen.scr_ri;

	p9100_defscreendesc.nrows = ri->ri_rows;
	p9100_defscreendesc.ncols = ri->ri_cols;
	p9100_defscreendesc.textops = &ri->ri_ops;
	p9100_defscreendesc.capabilities = ri->ri_caps;

	glyphcache_init(&sc->sc_gc, sc->sc_height + 5,
			(0x200000 / sc->sc_stride) - sc->sc_height - 5,
			sc->sc_width,
			ri->ri_font->fontwidth,
			ri->ri_font->fontheight,
			defattr);

	if(isconsole) {
		wsdisplay_cnattach(&p9100_defscreendesc, ri, 0, 0, defattr);
		vcons_replay_msgbuf(&p9100_console_screen);
	}

	aa.console = isconsole;
	aa.scrdata = &p9100_screenlist;
	aa.accessops = &p9100_accessops;
	aa.accesscookie = &sc->vd;

	config_found(self, &aa, wsemuldisplaydevprint);
#endif
	fb->fb_type.fb_size = fb->fb_type.fb_height * fb->fb_linebytes;
	printf("%s: rev %d / %x, %dx%d, depth %d mem %x\n",
		device_xname(self),
		(i & 7), ver, fb->fb_type.fb_width, fb->fb_type.fb_height,
		fb->fb_type.fb_depth, (unsigned int)sc->sc_fb_psize);
	/* cursor sprite handling */
	p9100_init_cursor(sc);

	/* attach the fb */
	fb_attach(fb, isconsole);

#if NTCTRL > 0
	/* register callback for external monitor status change */
	tadpole_register_callback(p9100_set_extvga, sc);
#endif
}

int
p9100open(dev_t dev, int flags, int mode, struct lwp *l)
{
	int unit = minor(dev);

	if (device_lookup(&pnozz_cd, unit) == NULL)
		return (ENXIO);
	return (0);
}

int
p9100close(dev_t dev, int flags, int mode, struct lwp *l)
{
	struct p9100_softc *sc = device_lookup_private(&pnozz_cd, minor(dev));

#if NWSDISPLAY > 0
	p9100_init_engine(sc);
	p9100_set_depth(sc, 8);
	p9100loadcmap(sc, 0, 256);
	p9100_clearscreen(sc);
	glyphcache_wipe(&sc->sc_gc);
	vcons_redraw_screen(sc->vd.active);
#endif
	return 0;
}

int
p9100ioctl(dev_t dev, u_long cmd, void *data, int flags, struct lwp *l)
{
	struct p9100_softc *sc = device_lookup_private(&pnozz_cd, minor(dev));
	struct fbgattr *fba;
	int error, v;

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
		/* XXX should use retrace interrupt */
		p9100loadcmap(sc, p->index, p->count);
#undef p
		break;

	case FBIOGVIDEO:
		*(int *)data = p9100_get_video(sc);
		break;

	case FBIOSVIDEO:
		p9100_set_video(sc, *(int *)data);
		break;

/* these are for both FBIOSCURSOR and FBIOGCURSOR */
#define p ((struct fbcursor *)data)
#define pc (&sc->sc_cursor)

	case FBIOGCURSOR:
		p->set = FB_CUR_SETALL;	/* close enough, anyway */
		p->enable = pc->pc_enable;
		p->pos = pc->pc_pos;
		p->hot = pc->pc_hot;
		p->size = pc->pc_size;

		if (p->image != NULL) {
			error = copyout(pc->pc_bits, p->image, 0x200);
			if (error)
				return error;
			error = copyout(&pc->pc_bits[0x80], p->mask, 0x200);
			if (error)
				return error;
		}

		p->cmap.index = 0;
		p->cmap.count = 3;
		if (p->cmap.red != NULL) {
			copyout(pc->red, p->cmap.red, 3);
			copyout(pc->green, p->cmap.green, 3);
			copyout(pc->blue, p->cmap.blue, 3);
		}
		break;

	case FBIOSCURSOR:
	{
		int count;
		uint32_t image[0x80], mask[0x80];
		uint8_t red[3], green[3], blue[3];

		v = p->set;
		if (v & FB_CUR_SETCMAP) {
			error = copyin(p->cmap.red, red, 3);
			error |= copyin(p->cmap.green, green, 3);
			error |= copyin(p->cmap.blue, blue, 3);
			if (error)
				return error;
		}
		if (v & FB_CUR_SETSHAPE) {
			if (p->size.x > 64 || p->size.y > 64)
				return EINVAL;
			memset(&mask, 0, 0x200);
			memset(&image, 0, 0x200);
			count = p->size.y * 8;
			error = copyin(p->image, image, count);
			if (error)
				return error;
			error = copyin(p->mask, mask, count);
			if (error)
				return error;
		}

		/* parameters are OK; do it */
		if (v & (FB_CUR_SETCUR | FB_CUR_SETPOS | FB_CUR_SETHOT)) {
			if (v & FB_CUR_SETCUR)
				pc->pc_enable = p->enable;
			if (v & FB_CUR_SETPOS)
				pc->pc_pos = p->pos;
			if (v & FB_CUR_SETHOT)
				pc->pc_hot = p->hot;
			p9100_set_fbcursor(sc);
		}

		if (v & FB_CUR_SETCMAP) {
			memcpy(pc->red, red, 3);
			memcpy(pc->green, green, 3);
			memcpy(pc->blue, blue, 3);
			p9100_setcursorcmap(sc);
		}

		if (v & FB_CUR_SETSHAPE) {
			memcpy(pc->pc_bits, image, 0x200);
			memcpy(&pc->pc_bits[0x80], mask, 0x200);
			p9100_loadcursor(sc);
		}
	}
	break;

#undef p
#undef cc

	case FBIOGCURPOS:
		*(struct fbcurpos *)data = sc->sc_cursor.pc_pos;
		break;

	case FBIOSCURPOS:
		sc->sc_cursor.pc_pos = *(struct fbcurpos *)data;
		p9100_set_fbcursor(sc);
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

static uint32_t
p9100_ctl_read_4(struct p9100_softc *sc, bus_size_t off)
{

	PNOZZ_LATCH(sc, off);
	return bus_space_read_4(sc->sc_bustag, sc->sc_ctl_memh, off);
}

static void
p9100_ctl_write_4(struct p9100_softc *sc, bus_size_t off, uint32_t v)
{

	PNOZZ_LATCH(sc, off);
	bus_space_write_4(sc->sc_bustag, sc->sc_ctl_memh, off, v);
}

/* initialize the drawing engine */
static void
p9100_init_engine(struct p9100_softc *sc)
{
	/* reset clipping rectangles */
	uint32_t rmax = ((sc->sc_width & 0x3fff) << 16) |
	    (sc->sc_height & 0x3fff);

	sc->sc_last_offset = 0xffffffff;

	p9100_ctl_write_4(sc, WINDOW_OFFSET, 0);
	p9100_ctl_write_4(sc, WINDOW_MIN, 0);
	p9100_ctl_write_4(sc, WINDOW_MAX, rmax);
	p9100_ctl_write_4(sc, BYTE_CLIP_MIN, 0);
	p9100_ctl_write_4(sc, BYTE_CLIP_MAX, 0x3fff3fff);
	p9100_ctl_write_4(sc, DRAW_MODE, 0);
	p9100_ctl_write_4(sc, PLANE_MASK, 0xffffffff);
	p9100_ctl_write_4(sc, PATTERN0, 0xffffffff);
	p9100_ctl_write_4(sc, PATTERN1, 0xffffffff);
	p9100_ctl_write_4(sc, PATTERN2, 0xffffffff);
	p9100_ctl_write_4(sc, PATTERN3, 0xffffffff);

}

/* we only need these in the wsdisplay case */
#if NWSDISPLAY > 0

/* wait until the engine is idle */
static void
p9100_sync(struct p9100_softc *sc)
{
	while((p9100_ctl_read_4(sc, ENGINE_STATUS) &
	    (ENGINE_BUSY | BLITTER_BUSY)) != 0);
}

static void
p9100_set_color_reg(struct p9100_softc *sc, int reg, int32_t col)
{
	uint32_t out;

	switch(sc->sc_depth)
	{
		case 1:	/* 8 bit */
			out = (col << 8) | col;
			out |= out << 16;
			break;
		case 2: /* 16 bit */
			out = col | (col << 16);
			break;
		default:
			out = col;
	}
	p9100_ctl_write_4(sc, reg, out);
}

/* screen-to-screen blit */
static void
p9100_bitblt(void *cookie, int xs, int ys, int xd, int yd, int wi,
    int he, int rop)
{
	struct p9100_softc *sc = cookie;
	uint32_t src, dst, srcw, dstw;

	sc->sc_last_offset = 0xffffffff;

	src = ((xs & 0x3fff) << 16) | (ys & 0x3fff);
	dst = ((xd & 0x3fff) << 16) | (yd & 0x3fff);
	srcw = (((xs + wi - 1) & 0x3fff) << 16) | ((ys + he - 1) & 0x3fff);
	dstw = (((xd + wi - 1) & 0x3fff) << 16) | ((yd + he - 1) & 0x3fff);

	p9100_sync(sc);
	
	p9100_ctl_write_4(sc, RASTER_OP, rop);
	p9100_ctl_write_4(sc, BYTE_CLIP_MAX, 0x3fff3fff);

	p9100_ctl_write_4(sc, ABS_XY0, src << sc->sc_depthshift);
	p9100_ctl_write_4(sc, ABS_XY1, srcw << sc->sc_depthshift);
	p9100_ctl_write_4(sc, ABS_XY2, dst << sc->sc_depthshift);
	p9100_ctl_write_4(sc, ABS_XY3, dstw << sc->sc_depthshift);

	(void)p9100_ctl_read_4(sc, COMMAND_BLIT);
}

/* solid rectangle fill */
static void
p9100_rectfill(void *cookie, int xs, int ys, int wi, int he, uint32_t col)
{
	struct p9100_softc *sc = cookie;
	uint32_t src, srcw;

	sc->sc_last_offset = 0xffffffff;

	src = ((xs & 0x3fff) << 16) | (ys & 0x3fff);
	srcw = (((xs + wi) & 0x3fff) << 16) | ((ys + he) & 0x3fff);
	p9100_sync(sc);
	p9100_ctl_write_4(sc, BYTE_CLIP_MAX, 0x3fff3fff);
	p9100_set_color_reg(sc, FOREGROUND_COLOR, col);
	p9100_set_color_reg(sc, BACKGROUND_COLOR, col);
	p9100_ctl_write_4(sc, RASTER_OP, ROP_PAT);
	p9100_ctl_write_4(sc, COORD_INDEX, 0);
	p9100_ctl_write_4(sc, RECT_RTW_XY, src);
	p9100_ctl_write_4(sc, RECT_RTW_XY, srcw);
	(void)p9100_ctl_read_4(sc, COMMAND_QUAD);
}

/* setup for mono->colour expansion */
static void
p9100_setup_mono(struct p9100_softc *sc, int x, int y, int wi, int he,
    uint32_t fg, uint32_t bg)
{

	sc->sc_last_offset = 0xffffffff;

	p9100_sync(sc);
	/*
	 * this doesn't make any sense to me either, but for some reason the
	 * chip applies the foreground colour to 0 pixels
	 */

	p9100_set_color_reg(sc,FOREGROUND_COLOR,bg);
	p9100_set_color_reg(sc,BACKGROUND_COLOR,fg);

	p9100_ctl_write_4(sc, BYTE_CLIP_MAX, 0x3fff3fff);
	p9100_ctl_write_4(sc, RASTER_OP, ROP_SRC);
	p9100_ctl_write_4(sc, ABS_X0, x);
	p9100_ctl_write_4(sc, ABS_XY1, (x << 16) | (y & 0xFFFFL));
	p9100_ctl_write_4(sc, ABS_X2, (x + wi));
	p9100_ctl_write_4(sc, ABS_Y3, he);
	/* now feed the data into the chip */
	sc->sc_mono_width = wi;
}

/* write monochrome data to the screen through the blitter */
static void
p9100_feed_line(struct p9100_softc *sc, int count, uint8_t *data)
{
	int i;
	uint32_t latch = 0, bork;
	int shift = 24;
	int to_go = sc->sc_mono_width;

	PNOZZ_LATCH(sc, PIXEL_1);

	for (i = 0; i < count; i++) {
		bork = data[i];
		latch |= (bork << shift);
		if (shift == 0) {
			/* check how many bits are significant */
			if (to_go > 31) {
				bus_space_write_4(sc->sc_bustag, 
				    sc->sc_ctl_memh,
				    (PIXEL_1 + (31 << 2)), latch);
				to_go -= 32;
			} else
			{
				bus_space_write_4(sc->sc_bustag, 
				    sc->sc_ctl_memh,
				    (PIXEL_1 + ((to_go - 1) << 2)), latch);
				to_go = 0;
			}
			latch = 0;
			shift = 24;
		} else
			shift -= 8;
		}
	if (shift != 24)
		p9100_ctl_write_4(sc, (PIXEL_1 + ((to_go - 1) << 2)), latch);
}

static void
p9100_clearscreen(struct p9100_softc *sc)
{

	p9100_rectfill(sc, 0, 0, sc->sc_width, sc->sc_height, sc->sc_bg);
}
#endif /* NWSDISPLAY > 0 */

static uint8_t
p9100_ramdac_read(struct p9100_softc *sc, bus_size_t off)
{

	(void)p9100_ctl_read_4(sc, PWRUP_CNFG);
	return ((bus_space_read_4(sc->sc_bustag,
	    sc->sc_ctl_memh, off) >> 16) & 0xff);
}

static void
p9100_ramdac_write(struct p9100_softc *sc, bus_size_t off, uint8_t v)
{

	(void)p9100_ctl_read_4(sc, PWRUP_CNFG);
	bus_space_write_4(sc->sc_bustag, sc->sc_ctl_memh, off,
	    ((uint32_t)v) << 16);
}

static uint8_t
p9100_ramdac_read_ctl(struct p9100_softc *sc, int off)
{
	p9100_ramdac_write(sc, DAC_INDX_LO, off & 0xff);
	p9100_ramdac_write(sc, DAC_INDX_HI, (off & 0xff00) >> 8);
	return p9100_ramdac_read(sc, DAC_INDX_DATA);
}

static void
p9100_ramdac_write_ctl(struct p9100_softc *sc, int off, uint8_t val)
{
	p9100_ramdac_write(sc, DAC_INDX_LO, off & 0xff);
	p9100_ramdac_write(sc, DAC_INDX_HI, (off & 0xff00) >> 8);
	p9100_ramdac_write(sc, DAC_INDX_DATA, val);
}

/*
 * Undo the effect of an FBIOSVIDEO that turns the video off.
 */
static void
p9100unblank(device_t dev)
{
	struct p9100_softc *sc = device_private(dev);

	p9100_set_video(sc, 1);

	/*
	 * Check if we're in terminal mode. If not force the console screen
	 * to front so we can see ddb, panic messages and so on
	 */
	if (sc->sc_mode != WSDISPLAYIO_MODE_EMUL) {
		sc->sc_mode = WSDISPLAYIO_MODE_EMUL;
		if (sc->vd.active != &p9100_console_screen) {
			SCREEN_INVISIBLE(sc->vd.active);
			sc->vd.active = &p9100_console_screen;
			SCREEN_VISIBLE(&p9100_console_screen);
		}
		p9100_init_engine(sc);
		p9100_set_depth(sc, 8);
		vcons_redraw_screen(&p9100_console_screen);
	}
}

static void
p9100_set_video(struct p9100_softc *sc, int enable)
{
	uint32_t v = p9100_ctl_read_4(sc, SCRN_RPNT_CTL_1);

	if (enable)
		v |= VIDEO_ENABLED;
	else
		v &= ~VIDEO_ENABLED;
	p9100_ctl_write_4(sc, SCRN_RPNT_CTL_1, v);
#if NTCTRL > 0
	/* Turn On/Off the TFT if we know how.
	 */
	tadpole_set_video(enable);
#endif
}

static int
p9100_get_video(struct p9100_softc *sc)
{
	return (p9100_ctl_read_4(sc, SCRN_RPNT_CTL_1) & VIDEO_ENABLED) != 0;
}

static bool
p9100_suspend(device_t dev, const pmf_qual_t *qual)
{
	struct p9100_softc *sc = device_private(dev);

	if (sc->sc_powerstate == PWR_SUSPEND)
		return TRUE;

	sc->sc_video = p9100_get_video(sc);
	sc->sc_dac_power = p9100_ramdac_read_ctl(sc, DAC_POWER_MGT);
	p9100_ramdac_write_ctl(sc, DAC_POWER_MGT,
		DAC_POWER_SCLK_DISABLE |
		DAC_POWER_DDOT_DISABLE |
		DAC_POWER_SYNC_DISABLE |
		DAC_POWER_ICLK_DISABLE |
		DAC_POWER_IPWR_DISABLE);
	p9100_set_video(sc, 0);
	sc->sc_powerstate = PWR_SUSPEND;
	return TRUE;
}

static bool
p9100_resume(device_t dev, const pmf_qual_t *qual)
{
	struct p9100_softc *sc = device_private(dev);

	if (sc->sc_powerstate == PWR_RESUME)
		return TRUE;

	p9100_ramdac_write_ctl(sc, DAC_POWER_MGT, sc->sc_dac_power);	
	p9100_set_video(sc, sc->sc_video);

	sc->sc_powerstate = PWR_RESUME;
	return TRUE;
}

/*
 * Load a subset of the current (new) colormap into the IBM RAMDAC.
 */
static void
p9100loadcmap(struct p9100_softc *sc, int start, int ncolors)
{
	int i;
	sc->sc_last_offset = 0xffffffff;

	p9100_ramdac_write(sc, DAC_CMAP_WRIDX, start);

	for (i=0;i<ncolors;i++) {
		p9100_ramdac_write(sc, DAC_CMAP_DATA,
		    sc->sc_cmap.cm_map[i + start][0]);
		p9100_ramdac_write(sc, DAC_CMAP_DATA,
		    sc->sc_cmap.cm_map[i + start][1]);
		p9100_ramdac_write(sc, DAC_CMAP_DATA,
		    sc->sc_cmap.cm_map[i + start][2]);
	}
}

/*
 * Return the address that would map the given device at the given
 * offset, allowing for the given protection, or return -1 for error.
 */
static paddr_t
p9100mmap(dev_t dev, off_t off, int prot)
{
	struct p9100_softc *sc = device_lookup_private(&pnozz_cd, minor(dev));

	if (off & PGOFSET)
		panic("p9100mmap");
	if (off < 0)
		return (-1);

#ifdef PNOZZ_EMUL_CG3
#define CG3_MMAP_OFFSET	0x04000000
	/* Make Xsun think we are a CG3 (SUN3COLOR)
	 */
	if (off >= CG3_MMAP_OFFSET && off < CG3_MMAP_OFFSET + sc->sc_fb_psize) {
		off -= CG3_MMAP_OFFSET;
		return (bus_space_mmap(sc->sc_bustag,
			sc->sc_fb_paddr,
			off,
			prot,
			BUS_SPACE_MAP_LINEAR));
	}
#endif

	if (off >= sc->sc_fb_psize + sc->sc_ctl_psize/* + sc->sc_cmd_psize*/)
		return (-1);

	if (off < sc->sc_fb_psize) {
		return (bus_space_mmap(sc->sc_bustag,
			sc->sc_fb_paddr,
			off,
			prot,
			BUS_SPACE_MAP_LINEAR));
	}

	off -= sc->sc_fb_psize;
	if (off < sc->sc_ctl_psize) {
		return (bus_space_mmap(sc->sc_bustag,
			sc->sc_ctl_paddr,
			off,
			prot,
			BUS_SPACE_MAP_LINEAR));
	}

	return EINVAL;
}

/* wscons stuff */
#if NWSDISPLAY > 0

static void
p9100_cursor(void *cookie, int on, int row, int col)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;
	struct p9100_softc *sc = scr->scr_cookie;
	int x, y, wi,he;

	wi = ri->ri_font->fontwidth;
	he = ri->ri_font->fontheight;

	if (ri->ri_flg & RI_CURSOR) {
		x = ri->ri_ccol * wi + ri->ri_xorigin;
		y = ri->ri_crow * he + ri->ri_yorigin;
		p9100_bitblt(sc, x, y, x, y, wi, he, ROP_SRC ^ 0xff);
		ri->ri_flg &= ~RI_CURSOR;
	}

	ri->ri_crow = row;
	ri->ri_ccol = col;

	if (on)
	{
		x = ri->ri_ccol * wi + ri->ri_xorigin;
		y = ri->ri_crow * he + ri->ri_yorigin;
		p9100_bitblt(sc, x, y, x, y, wi, he, ROP_SRC ^ 0xff);
		ri->ri_flg |= RI_CURSOR;
	}
}

#if 0
static int
p9100_mapchar(void *cookie, int uni, u_int *index)
{
	return 0;
}
#endif

static void
p9100_putchar(void *cookie, int row, int col, u_int c, long attr)
{
	struct rasops_info *ri = cookie;
	struct wsdisplay_font *font = PICK_FONT(ri, c);
	struct vcons_screen *scr = ri->ri_hw;
	struct p9100_softc *sc = scr->scr_cookie;

	int fg, bg, i;
	uint8_t *data;
	int x, y, wi, he;

	wi = font->fontwidth;
	he = font->fontheight;

	if (!CHAR_IN_FONT(c, font))
		return;

	bg = (u_char)ri->ri_devcmap[(attr >> 16) & 0xff];
	fg = (u_char)ri->ri_devcmap[(attr >> 24) & 0xff];
	x = ri->ri_xorigin + col * wi;
	y = ri->ri_yorigin + row * he;

	if (c == 0x20) {
		p9100_rectfill(sc, x, y, wi, he, bg);
	} else {
		data = WSFONT_GLYPH(c, font);

		p9100_setup_mono(sc, x, y, wi, 1, fg, bg);
		for (i = 0; i < he; i++) {
			p9100_feed_line(sc, font->stride,
			    data);
			data += font->stride;
		}
	}
}

static void
p9100_putchar_aa(void *cookie, int row, int col, u_int c, long attr)
{
	struct rasops_info *ri = cookie;
	struct wsdisplay_font *font = PICK_FONT(ri, c);
	struct vcons_screen *scr = ri->ri_hw;
	struct p9100_softc *sc = scr->scr_cookie;
	uint32_t bg, latch = 0, bg8, fg8, pixel;
	int i, j, x, y, wi, he, r, g, b, aval, rwi;
	int r1, g1, b1, r0, g0, b0, fgo, bgo;
	uint8_t *data8;
	int rv;

	if (sc->sc_mode != WSDISPLAYIO_MODE_EMUL) 
		return;

	if (!CHAR_IN_FONT(c, font))
		return;

	wi = font->fontwidth;
	rwi = (wi + 3) & ~3;
	he = font->fontheight;

	bg = ri->ri_devcmap[(attr >> 16) & 0xf];
	x = ri->ri_xorigin + col * wi;
	y = ri->ri_yorigin + row * he;

	if (c == 0x20) {
		p9100_rectfill(sc, x, y, wi, he, bg);
		return;
	}

	rv = glyphcache_try(&sc->sc_gc, c, x, y, attr);
	if (rv == GC_OK)
		return;

	data8 = WSFONT_GLYPH(c, font);

	p9100_sync(sc);

	p9100_ctl_write_4(sc, RASTER_OP, ROP_SRC);
	p9100_ctl_write_4(sc, ABS_X0, x);
	p9100_ctl_write_4(sc, ABS_XY1, (x << 16) | (y & 0xFFFFL));
	p9100_ctl_write_4(sc, ABS_X2, (x + rwi));
	p9100_ctl_write_4(sc, ABS_Y3, 1);
	p9100_ctl_write_4(sc, BYTE_CLIP_MAX, ((x + wi - 1) << 16) | 0x3fff);

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

	//r128fb_wait(sc, 16);

	for (i = 0; i < he; i++) {
		for (j = 0; j < wi; j++) {
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
			if ((j & 3) == 3) {
				bus_space_write_4(sc->sc_bustag, sc->sc_ctl_memh,
				    COMMAND_PIXEL8, latch);
				latch = 0;
			}
			data8++;
		}
		/* if we have pixels left in latch write them out */
		if ((j & 3) != 0) {
			latch = latch << ((4 - (j & 3)) << 3);	
			bus_space_write_4(sc->sc_bustag, sc->sc_ctl_memh,
			    COMMAND_PIXEL8, latch);
		}
	}
	if (rv == GC_ADD) {
		glyphcache_add(&sc->sc_gc, c, x, y);
	}
}

/*
 * wsdisplay_accessops
 */

int
p9100_ioctl(void *v, void *vs, u_long cmd, void *data, int flag,
	struct lwp *l)
{
	struct vcons_data *vd = v;
	struct p9100_softc *sc = vd->cookie;
	struct wsdisplay_fbinfo *wdf;
	struct vcons_screen *ms = vd->active;

	switch (cmd) {
		case WSDISPLAYIO_GTYPE:
			*(u_int *)data = WSDISPLAY_TYPE_SB_P9100;
			return 0;

		case FBIOGVIDEO:
		case WSDISPLAYIO_GVIDEO:
			*(int *)data = p9100_get_video(sc);
			return 0;

		case WSDISPLAYIO_SVIDEO:
		case FBIOSVIDEO:
			p9100_set_video(sc, *(int *)data);
			return 0;

		case WSDISPLAYIO_GINFO:
			wdf = (void *)data;
			wdf->height = ms->scr_ri.ri_height;
			wdf->width = ms->scr_ri.ri_width;
			wdf->depth = ms->scr_ri.ri_depth;
			wdf->cmsize = 256;
			return 0;

		case WSDISPLAYIO_GETCMAP:
			return p9100_getcmap(sc, (struct wsdisplay_cmap *)data);

		case WSDISPLAYIO_PUTCMAP:
			return p9100_putcmap(sc, (struct wsdisplay_cmap *)data);

		case WSDISPLAYIO_SMODE:
			{
				int new_mode = *(int*)data;
				if (new_mode != sc->sc_mode)
				{
					sc->sc_mode = new_mode;
					if (new_mode == WSDISPLAYIO_MODE_EMUL)
					{
						p9100_init_engine(sc);
						p9100_set_depth(sc, 8);
						p9100loadcmap(sc, 0, 256);
						p9100_clearscreen(sc);
						glyphcache_wipe(&sc->sc_gc);
						vcons_redraw_screen(ms);
					}
				}
			}
	}
	return EPASSTHROUGH;
}

static paddr_t
p9100_mmap(void *v, void *vs, off_t offset, int prot)
{
	struct vcons_data *vd = v;
	struct p9100_softc *sc = vd->cookie;
	paddr_t pa;

	/* 'regular' framebuffer mmap()ing */
	if (offset < sc->sc_fb_psize) {
		pa = bus_space_mmap(sc->sc_bustag, sc->sc_fb_paddr + offset, 0,
		    prot, BUS_SPACE_MAP_LINEAR);
		return pa;
	}

	if ((offset >= sc->sc_fb_paddr) && (offset < (sc->sc_fb_paddr +
	    sc->sc_fb_psize))) {
		pa = bus_space_mmap(sc->sc_bustag, offset, 0, prot,
		    BUS_SPACE_MAP_LINEAR);
		return pa;
	}

	if ((offset >= sc->sc_ctl_paddr) && (offset < (sc->sc_ctl_paddr +
	    sc->sc_ctl_psize))) {
		pa = bus_space_mmap(sc->sc_bustag, offset, 0, prot,
		    BUS_SPACE_MAP_LINEAR);
		return pa;
	}

	return -1;
}

static void
p9100_init_screen(void *cookie, struct vcons_screen *scr,
    int existing, long *defattr)
{
	struct p9100_softc *sc = cookie;
	struct rasops_info *ri = &scr->scr_ri;

	ri->ri_depth = sc->sc_depth << 3;
	ri->ri_width = sc->sc_width;
	ri->ri_height = sc->sc_height;
	ri->ri_stride = sc->sc_stride;
	ri->ri_flg = RI_CENTER | RI_FULLCLEAR;
	if (ri->ri_depth == 8)
		ri->ri_flg |= RI_8BIT_IS_RGB | RI_ENABLE_ALPHA;

#ifdef PNOZZ_USE_LATCH
	ri->ri_bits = bus_space_vaddr(sc->sc_bustag, sc->sc_fb_memh);
	DPRINTF("addr: %08lx\n",(ulong)ri->ri_bits);
#endif

	rasops_init(ri, 0, 0);
	ri->ri_caps = WSSCREEN_WSCOLORS;
	rasops_reconfig(ri, sc->sc_height / ri->ri_font->fontheight,
		    sc->sc_width / ri->ri_font->fontwidth);

	/* enable acceleration */
	ri->ri_ops.cursor    = p9100_cursor;
	ri->ri_ops.copyrows  = p9100_copyrows;
	ri->ri_ops.eraserows = p9100_eraserows;
	ri->ri_ops.copycols  = p9100_copycols;
	ri->ri_ops.erasecols = p9100_erasecols;
	if (FONT_IS_ALPHA(ri->ri_font)) {
		ri->ri_ops.putchar = p9100_putchar_aa;
	} else
		ri->ri_ops.putchar = p9100_putchar;
}

static int
p9100_putcmap(struct p9100_softc *sc, struct wsdisplay_cmap *cm)
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
		sc->sc_cmap.cm_map[index][0] = *r;
		sc->sc_cmap.cm_map[index][1] = *g;
		sc->sc_cmap.cm_map[index][2] = *b;
		index++;
		r++, g++, b++;
	}
	p9100loadcmap(sc, 0, 256);
	return 0;
}

static int
p9100_getcmap(struct p9100_softc *sc, struct wsdisplay_cmap *cm)
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

static void
p9100_copycols(void *cookie, int row, int srccol, int dstcol, int ncols)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;
	int32_t xs, xd, y, width, height;

	xs = ri->ri_xorigin + ri->ri_font->fontwidth * srccol;
	xd = ri->ri_xorigin + ri->ri_font->fontwidth * dstcol;
	y = ri->ri_yorigin + ri->ri_font->fontheight * row;
	width = ri->ri_font->fontwidth * ncols;
	height = ri->ri_font->fontheight;
	p9100_bitblt(scr->scr_cookie, xs, y, xd, y, width, height, ROP_SRC);
}

static void
p9100_erasecols(void *cookie, int row, int startcol, int ncols, long fillattr)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;
	int32_t x, y, width, height, bg;

	x = ri->ri_xorigin + ri->ri_font->fontwidth * startcol;
	y = ri->ri_yorigin + ri->ri_font->fontheight * row;
	width = ri->ri_font->fontwidth * ncols;
	height = ri->ri_font->fontheight;
	bg = (uint32_t)ri->ri_devcmap[(fillattr >> 16) & 0xff];
	p9100_rectfill(scr->scr_cookie, x, y, width, height, bg);
}

static void
p9100_copyrows(void *cookie, int srcrow, int dstrow, int nrows)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;
	int32_t x, ys, yd, width, height;

	x = ri->ri_xorigin;
	ys = ri->ri_yorigin + ri->ri_font->fontheight * srcrow;
	yd = ri->ri_yorigin + ri->ri_font->fontheight * dstrow;
	width = ri->ri_emuwidth;
	height = ri->ri_font->fontheight * nrows;
	p9100_bitblt(scr->scr_cookie, x, ys, x, yd, width, height, ROP_SRC);
}

static void
p9100_eraserows(void *cookie, int row, int nrows, long fillattr)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;
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
	p9100_rectfill(scr->scr_cookie, x, y, width, height, bg);
}

#if 0
static int
p9100_load_font(void *v, void *cookie, struct wsdisplay_font *data)
{

	return 0;
}
#endif

#endif /* NWSDISPLAY > 0 */

#if 0
static int
p9100_intr(void *arg)
{
	/*p9100_softc *sc=arg;*/
	DPRINTF(".");
	return 1;
}
#endif

static void
p9100_init_cursor(struct p9100_softc *sc)
{

	memset(&sc->sc_cursor, 0, sizeof(struct pnozz_cursor));
	sc->sc_cursor.pc_size.x = 64;
	sc->sc_cursor.pc_size.y = 64;

}

static void
p9100_set_fbcursor(struct p9100_softc *sc)
{
#ifdef PNOZZ_PARANOID
	int s;

	s = splhigh();	/* just in case... */
#endif
	sc->sc_last_offset = 0xffffffff;

	/* set position and hotspot */
	p9100_ramdac_write(sc, DAC_INDX_CTL, DAC_INDX_AUTOINCR);
	p9100_ramdac_write(sc, DAC_INDX_HI, 0);
	p9100_ramdac_write(sc, DAC_INDX_LO, DAC_CURSOR_CTL);
	if (sc->sc_cursor.pc_enable) {
		p9100_ramdac_write(sc, DAC_INDX_DATA, DAC_CURSOR_X11 |
		    DAC_CURSOR_64);
	} else
		p9100_ramdac_write(sc, DAC_INDX_DATA, DAC_CURSOR_OFF);
	/* next two registers - x low, high, y low, high */
	p9100_ramdac_write(sc, DAC_INDX_DATA, sc->sc_cursor.pc_pos.x & 0xff);
	p9100_ramdac_write(sc, DAC_INDX_DATA, (sc->sc_cursor.pc_pos.x >> 8) &
	    0xff);
	p9100_ramdac_write(sc, DAC_INDX_DATA, sc->sc_cursor.pc_pos.y & 0xff);
	p9100_ramdac_write(sc, DAC_INDX_DATA, (sc->sc_cursor.pc_pos.y >> 8) &
	    0xff);
	/* hotspot */
	p9100_ramdac_write(sc, DAC_INDX_DATA, sc->sc_cursor.pc_hot.x & 0xff);
	p9100_ramdac_write(sc, DAC_INDX_DATA, sc->sc_cursor.pc_hot.y & 0xff);

#ifdef PNOZZ_PARANOID
	splx(s);
#endif

}

static void
p9100_setcursorcmap(struct p9100_softc *sc)
{
	int i;

#ifdef PNOZZ_PARANOID
	int s;
	s = splhigh();	/* just in case... */
#endif
	sc->sc_last_offset = 0xffffffff;

	/* set cursor colours */
	p9100_ramdac_write(sc, DAC_INDX_CTL, DAC_INDX_AUTOINCR);
	p9100_ramdac_write(sc, DAC_INDX_HI, 0);
	p9100_ramdac_write(sc, DAC_INDX_LO, DAC_CURSOR_COL_1);

	for (i = 0; i < 3; i++) {
		p9100_ramdac_write(sc, DAC_INDX_DATA, sc->sc_cursor.red[i]);
		p9100_ramdac_write(sc, DAC_INDX_DATA, sc->sc_cursor.green[i]);
		p9100_ramdac_write(sc, DAC_INDX_DATA, sc->sc_cursor.blue[i]);
	}

#ifdef PNOZZ_PARANOID
	splx(s);
#endif
}

static void
p9100_loadcursor(struct p9100_softc *sc)
{
	uint32_t *image, *mask;
	uint32_t bit, bbit, im, ma;
	int i, j, k;
	uint8_t latch1, latch2;

#ifdef PNOZZ_PARANOID
	int s;
	s = splhigh();	/* just in case... */
#endif
	sc->sc_last_offset = 0xffffffff;

	/* set cursor shape */
	p9100_ramdac_write(sc, DAC_INDX_CTL, DAC_INDX_AUTOINCR);
	p9100_ramdac_write(sc, DAC_INDX_HI, 1);
	p9100_ramdac_write(sc, DAC_INDX_LO, 0);

	image = sc->sc_cursor.pc_bits;
	mask = &sc->sc_cursor.pc_bits[0x80];

	for (i = 0; i < 0x80; i++) {
		bit = 0x80000000;
		im = image[i];
		ma = mask[i];
		for (k = 0; k < 4; k++) {
			bbit = 0x1;
			latch1 = 0;
			for (j = 0; j < 4; j++) {
				if (im & bit)
					latch1 |= bbit;
				bbit <<= 1;
				if (ma & bit)
					latch1 |= bbit;
				bbit <<= 1;
				bit >>= 1;
			}
			bbit = 0x1;
			latch2 = 0;
			for (j = 0; j < 4; j++) {
				if (im & bit)
					latch2 |= bbit;
				bbit <<= 1;
				if (ma & bit)
					latch2 |= bbit;
				bbit <<= 1;
				bit >>= 1;
			}
			p9100_ramdac_write(sc, DAC_INDX_DATA, latch1);
			p9100_ramdac_write(sc, DAC_INDX_DATA, latch2);
		}
	}
#ifdef PNOZZ_DEBUG_CURSOR
	printf("image:\n");
	for (i=0;i<0x80;i+=2)
		printf("%08x %08x\n", image[i], image[i+1]);
	printf("mask:\n");
	for (i=0;i<0x80;i+=2)
		printf("%08x %08x\n", mask[i], mask[i+1]);
#endif
#ifdef PNOZZ_PARANOID
	splx(s);
#endif
}

#if NTCTRL > 0
static void
p9100_set_extvga(void *cookie, int status)
{
	struct p9100_softc *sc = cookie;
#ifdef PNOZZ_PARANOID
	int s;

	s = splhigh();
#endif

#ifdef PNOZZ_DEBUG
	printf("%s: external VGA %s\n", device_xname(sc->sc_dev),
	    status ? "on" : "off");
#endif

	sc->sc_last_offset = 0xffffffff;

	if (status) {
		p9100_ramdac_write_ctl(sc, DAC_POWER_MGT,
		    p9100_ramdac_read_ctl(sc, DAC_POWER_MGT) &
		    ~DAC_POWER_IPWR_DISABLE);
	} else {
		p9100_ramdac_write_ctl(sc, DAC_POWER_MGT,
		    p9100_ramdac_read_ctl(sc, DAC_POWER_MGT) |
		    DAC_POWER_IPWR_DISABLE);
	}
#ifdef PNOZZ_PARANOID
	splx(s);
#endif
}
#endif /* NTCTRL > 0 */

static int
upper_bit(uint32_t b)
{
        uint32_t mask=0x80000000;
        int cnt = 31;
        if (b == 0)  
                return -1;
        while ((mask != 0) && ((b & mask) == 0)) {
                mask = mask >> 1;
                cnt--;
        }
        return cnt;
}

static int
p9100_set_depth(struct p9100_softc *sc, int depth)
{
	int new_sls;
	uint32_t bits, scr, memctl, mem;
	int s0, s1, s2, s3, ps, crtcline;
	uint8_t pf, mc3, es;

	switch (depth) {
		case 8:
			sc->sc_depthshift = 0;
			ps = 2;
			pf = 3;
			mc3 = 0;
			es = 0;	/* no swapping */
			memctl = 3;
			break;
		case 16:
			sc->sc_depthshift = 1;
			ps = 3;
			pf = 4;
			mc3 = 0;
			es = 2;	/* swap bytes in 16bit words */
			memctl = 2;
			break;
		case 24:
			/* boo */
			printf("We don't DO 24bit pixels dammit!\n");
			return 0;
		case 32:
			sc->sc_depthshift = 2;
			ps = 5;
			pf = 6;
			mc3 = 0;
			es = 6;	/* swap both half-words and bytes */
			memctl = 1;	/* 0 */
			break;
		default:
			aprint_error("%s: bogus colour depth (%d)\n",
			    __func__, depth);
			return FALSE;
	}
	/*
	 * this could be done a lot shorter and faster but then nobody would 
	 * understand what the hell we're doing here without getting a major 
	 * headache. Scanline size is encoded as 4 shift values, 3 of them 3 bits 
	 * wide, 16 << n for n>0, one 2 bits, 512 << n for n>0. n==0 means 0
	 */
	new_sls = sc->sc_width << sc->sc_depthshift;
	sc->sc_stride = new_sls;
	bits = new_sls;
	s3 = upper_bit(bits);
	if (s3 > 9) {
		bits &= ~(1 << s3);
		s3 -= 9;
	} else
		s3 = 0;
	s2 = upper_bit(bits);
	if (s2 > 0) {
		bits &= ~(1 << s2);
		s2 -= 4;
	} else
		s2 = 0;
	s1 = upper_bit(bits);
	if (s1 > 0) {
	        bits &= ~(1 << s1);
	        s1 -= 4;
	} else
		s1 = 0;
	s0 = upper_bit(bits);
	if (s0 > 0) {
	        bits &= ~(1 << s0);
	        s0 -= 4;
	} else
		s0 = 0;


	DPRINTF("sls: %x sh: %d %d %d %d leftover: %x\n", new_sls, s0, s1,
	    s2, s3, bits);

	/* 
	 * now let's put these values into the System Config Register. No need to 
	 * read it here since we (hopefully) just saved the content 
	 */
	scr = p9100_ctl_read_4(sc, SYS_CONF);
	scr = (s0 << SHIFT_0) | (s1 << SHIFT_1) | (s2 << SHIFT_2) | 
	        (s3 << SHIFT_3) | (ps << PIXEL_SHIFT) | (es << SWAP_SHIFT);

	DPRINTF("new scr: %x DAC %x %x\n", scr, pf, mc3);
    
	mem = p9100_ctl_read_4(sc, VID_MEM_CONFIG);

	DPRINTF("old memctl: %08x\n", mem);

	/* set shift and crtc clock */
	mem &= ~(0x0000fc00);
	mem |= (memctl << 10) | (memctl << 13);
	p9100_ctl_write_4(sc, VID_MEM_CONFIG, mem);

	DPRINTF("new memctl: %08x\n", mem);

	/* whack the engine... */
	p9100_ctl_write_4(sc, SYS_CONF, scr);
    
	/* ok, whack the DAC */
	p9100_ramdac_write_ctl(sc, DAC_MISC_1, 0x11);
	p9100_ramdac_write_ctl(sc, DAC_MISC_2, 0x45);
	p9100_ramdac_write_ctl(sc, DAC_MISC_3, mc3);
	/* 
	 * despite the 3GX manual saying otherwise we don't need to mess with
	 * any clock dividers here
	 */
	p9100_ramdac_write_ctl(sc, DAC_MISC_CLK, 1);
	p9100_ramdac_write_ctl(sc, 3, 0);
	p9100_ramdac_write_ctl(sc, 4, 0);

	p9100_ramdac_write_ctl(sc, DAC_POWER_MGT, 0);
	p9100_ramdac_write_ctl(sc, DAC_OPERATION, 0);
	p9100_ramdac_write_ctl(sc, DAC_PALETTE_CTRL, 0);

	p9100_ramdac_write_ctl(sc, DAC_PIXEL_FMT, pf);

	/* TODO: distinguish between 15 and 16 bit */
	p9100_ramdac_write_ctl(sc, DAC_8BIT_CTRL, 0);
	/* direct colour, linear, 565 */
	p9100_ramdac_write_ctl(sc, DAC_16BIT_CTRL, 0xc6);
	/* direct colour */
	p9100_ramdac_write_ctl(sc, DAC_32BIT_CTRL, 3);

	/* From the 3GX manual. Needs magic number reduction */
	p9100_ramdac_write_ctl(sc, 0x10, 2);
	p9100_ramdac_write_ctl(sc, 0x11, 0);
	p9100_ramdac_write_ctl(sc, 0x14, 5);
	p9100_ramdac_write_ctl(sc, 0x08, 1);
	p9100_ramdac_write_ctl(sc, 0x15, 5);
	p9100_ramdac_write_ctl(sc, 0x16, 0x63);

	/* whack the CRTC */
	/* we always transfer 64bit in one go */
	crtcline = sc->sc_stride >> 3;

	DPRINTF("crtcline: %d\n", crtcline);

	p9100_ctl_write_4(sc, VID_HTOTAL, (24 << sc->sc_depthshift) + crtcline);
	p9100_ctl_write_4(sc, VID_HSRE, 8 << sc->sc_depthshift);
	p9100_ctl_write_4(sc, VID_HBRE, 18 << sc->sc_depthshift);
	p9100_ctl_write_4(sc, VID_HBFE, (18 << sc->sc_depthshift) + crtcline);

#ifdef PNOZZ_DEBUG
	{
		uint32_t sscr;
		sscr = p9100_ctl_read_4(sc, SYS_CONF);
		printf("scr: %x\n", sscr);
	}
#endif
	return TRUE;
}
