/*	$NetBSD: cgthree.c,v 1.31 2014/07/25 08:10:39 dholland Exp $ */

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Paul Kranenburg.
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
 * color display (cgthree) driver.
 *
 * Does not handle interrupts, even though they can occur.
 *
 * XXX should defer colormap updates to vertical retrace interrupts
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: cgthree.c,v 1.31 2014/07/25 08:10:39 dholland Exp $");

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
#include <dev/sun/cgthreereg.h>
#include <dev/sun/cgthreevar.h>

#if NWSDISPLAY > 0
#include <dev/wscons/wsconsio.h>
#include <dev/wsfont/wsfont.h>
#include <dev/rasops/rasops.h>

#include "opt_wsemul.h"
#endif

#include "ioconf.h"

static void	cgthreeunblank(device_t);
static void	cgthreeloadcmap(struct cgthree_softc *, int, int);
static void	cgthree_set_video(struct cgthree_softc *, int);
static int	cgthree_get_video(struct cgthree_softc *);

dev_type_open(cgthreeopen);
dev_type_ioctl(cgthreeioctl);
dev_type_mmap(cgthreemmap);

const struct cdevsw cgthree_cdevsw = {
	.d_open = cgthreeopen,
	.d_close = nullclose,
	.d_read = noread,
	.d_write = nowrite,
	.d_ioctl = cgthreeioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = nopoll,
	.d_mmap = cgthreemmap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = D_OTHER
};

/* frame buffer generic driver */
static struct fbdriver cgthreefbdriver = {
	cgthreeunblank, cgthreeopen, nullclose, cgthreeioctl, nopoll,
	cgthreemmap, nokqfilter
};

/* Video control parameters */
struct cg3_videoctrl {
	unsigned char	sense;		/* Monitor sense value */
	unsigned char	vctrl[12];
} cg3_videoctrl[] = {
/* Missing entries: sense 0x10, 0x30, 0x50 */
	{ 0x40, /* this happens to be my 19'' 1152x900 gray-scale monitor */
	   {0xbb, 0x2b, 0x3, 0xb, 0xb3, 0x3, 0xaf, 0x2b, 0x2, 0xa, 0xff, 0x1}
	},
	{ 0x00, /* default? must be last */
	   {0xbb, 0x2b, 0x3, 0xb, 0xb3, 0x3, 0xaf, 0x2b, 0x2, 0xa, 0xff, 0x1}
	}
};

#if NWSDISPLAY > 0
#ifdef RASTERCONSOLE
#error RASTERCONSOLE and wsdisplay are mutually exclusive
#endif

static void cg3_setup_palette(struct cgthree_softc *);

struct wsscreen_descr cgthree_defaultscreen = {
	"std",
	0, 0,	/* will be filled in -- XXX shouldn't, it's global */
	NULL,		/* textops */
	8, 16,	/* font width/height */
	WSSCREEN_WSCOLORS,	/* capabilities */
	NULL	/* modecookie */
};

static int 	cgthree_ioctl(void *, void *, u_long, void *, int, struct lwp *);
static paddr_t	cgthree_mmap(void *, void *, off_t, int);
static void	cgthree_init_screen(void *, struct vcons_screen *, int, long *);

int	cgthree_putcmap(struct cgthree_softc *, struct wsdisplay_cmap *);
int	cgthree_getcmap(struct cgthree_softc *, struct wsdisplay_cmap *);

struct wsdisplay_accessops cgthree_accessops = {
	cgthree_ioctl,
	cgthree_mmap,
	NULL,	/* alloc_screen */
	NULL,	/* free_screen */
	NULL,	/* show_screen */
	NULL, 	/* load_font */
	NULL,	/* pollc */
	NULL	/* scroll */
};

const struct wsscreen_descr *_cgthree_scrlist[] = {
	&cgthree_defaultscreen
};

struct wsscreen_list cgthree_screenlist = {
	sizeof(_cgthree_scrlist) / sizeof(struct wsscreen_descr *),
	_cgthree_scrlist
};


extern const u_char rasops_cmap[768];

static struct vcons_screen cg3_console_screen;
#endif /* NWSDISPLAY > 0 */

void
cgthreeattach(struct cgthree_softc *sc, const char *name, int isconsole)
{
	int i;
	struct fbdevice *fb = &sc->sc_fb;
#if NWSDISPLAY > 0
	struct wsemuldisplaydev_attach_args aa;
	struct rasops_info *ri = &cg3_console_screen.scr_ri;
	unsigned long defattr;
#endif
	volatile struct fbcontrol *fbc = sc->sc_fbc;
	volatile struct bt_regs *bt = &fbc->fbc_dac;

	fb->fb_driver = &cgthreefbdriver;
	fb->fb_type.fb_cmsize = 256;
	fb->fb_type.fb_size = fb->fb_type.fb_height * fb->fb_linebytes;
	printf(": %s, %d x %d", name,
		fb->fb_type.fb_width, fb->fb_type.fb_height);

	/* Transfer video magic to board, if it's not running */
	if ((fbc->fbc_ctrl & FBC_TIMING) == 0) {
		int sense = (fbc->fbc_status & FBS_MSENSE);
		/* Search table for video timings fitting this monitor */
		for (i = 0; i < sizeof(cg3_videoctrl)/sizeof(cg3_videoctrl[0]);
		     i++) {
			int j;
			if (sense != cg3_videoctrl[i].sense)
				continue;

			printf(" setting video ctrl");
			for (j = 0; j < 12; j++)
				fbc->fbc_vcontrol[j] =
					cg3_videoctrl[i].vctrl[j];
			fbc->fbc_ctrl |= FBC_TIMING;
			break;
		}
	}

	/* make sure we are not blanked */
	cgthree_set_video(sc, 1);
	BT_INIT(bt, 0);

	if (isconsole) {
		printf(" (console)\n");
#ifdef RASTERCONSOLE
		fbrcons_init(fb);
#endif
	} else
		printf("\n");

	fb_attach(fb, isconsole);

#if NWSDISPLAY > 0
	sc->sc_width = fb->fb_type.fb_width;
	sc->sc_stride = fb->fb_type.fb_width;
	sc->sc_height = fb->fb_type.fb_height;

	/* setup rasops and so on for wsdisplay */
	sc->sc_mode = WSDISPLAYIO_MODE_EMUL;

	vcons_init(&sc->vd, sc, &cgthree_defaultscreen, &cgthree_accessops);
	sc->vd.init_screen = cgthree_init_screen;

	if(isconsole) {
		/* we mess with cg3_console_screen only once */
		vcons_init_screen(&sc->vd, &cg3_console_screen, 1,
		    &defattr);
		memset(sc->sc_fb.fb_pixels, (defattr >> 16) & 0xff,
		    sc->sc_stride * sc->sc_height);
		cg3_console_screen.scr_flags |= VCONS_SCREEN_IS_STATIC;

		cgthree_defaultscreen.textops = &ri->ri_ops;
		cgthree_defaultscreen.capabilities = ri->ri_caps;
		cgthree_defaultscreen.nrows = ri->ri_rows;
		cgthree_defaultscreen.ncols = ri->ri_cols;
		sc->vd.active = &cg3_console_screen;
		wsdisplay_cnattach(&cgthree_defaultscreen, ri, 0, 0, defattr);
		vcons_replay_msgbuf(&cg3_console_screen);
	} else {
		/* 
		 * we're not the console so we just clear the screen and don't 
		 * set up any sort of text display
		 */
	}

	/* Initialize the default color map. */
	cg3_setup_palette(sc);

	aa.scrdata = &cgthree_screenlist;
	aa.console = isconsole;
	aa.accessops = &cgthree_accessops;
	aa.accesscookie = &sc->vd;
	config_found(sc->sc_dev, &aa, wsemuldisplaydevprint);
#else
	/* Initialize the default color map. */
	bt_initcmap(&sc->sc_cmap, 256);
	cgthreeloadcmap(sc, 0, 256);
#endif

}


int
cgthreeopen(dev_t dev, int flags, int mode, struct lwp *l)
{
	int unit = minor(dev);

	if (device_lookup(&cgthree_cd, unit) == NULL)
		return (ENXIO);
	return (0);
}

int
cgthreeioctl(dev_t dev, u_long cmd, void *data, int flags, struct lwp *l)
{
	struct cgthree_softc *sc = device_lookup_private(&cgthree_cd,
							 minor(dev));
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
		/* XXX should use retrace interrupt */
		cgthreeloadcmap(sc, p->index, p->count);
#undef p
		break;

	case FBIOGVIDEO:
		*(int *)data = cgthree_get_video(sc);
		break;

	case FBIOSVIDEO:
		cgthree_set_video(sc, *(int *)data);
		break;

	default:
		return (ENOTTY);
	}
	return (0);
}

/*
 * Undo the effect of an FBIOSVIDEO that turns the video off.
 */
static void
cgthreeunblank(device_t self)
{
	struct cgthree_softc *sc = device_private(self);

	cgthree_set_video(sc, 1);
}

static void
cgthree_set_video(struct cgthree_softc *sc, int enable)
{

	if (enable)
		sc->sc_fbc->fbc_ctrl |= FBC_VENAB;
	else
		sc->sc_fbc->fbc_ctrl &= ~FBC_VENAB;
}

static int
cgthree_get_video(struct cgthree_softc *sc)
{

	return ((sc->sc_fbc->fbc_ctrl & FBC_VENAB) != 0);
}

/*
 * Load a subset of the current (new) colormap into the Brooktree DAC.
 */
static void
cgthreeloadcmap(struct cgthree_softc *sc, int start, int ncolors)
{
	volatile struct bt_regs *bt;
	u_int *ip;
	int count;

	ip = &sc->sc_cmap.cm_chip[BT_D4M3(start)];	/* start/4 * 3 */
	count = BT_D4M3(start + ncolors - 1) - BT_D4M3(start) + 3;
	bt = &sc->sc_fbc->fbc_dac;
	bt->bt_addr = BT_D4M4(start);
	while (--count >= 0)
		bt->bt_cmap = *ip++;
}

/*
 * Return the address that would map the given device at the given
 * offset, allowing for the given protection, or return -1 for error.
 *
 * The cg3 is mapped starting at 256KB, for pseudo-compatibility with
 * the cg4 (which had an overlay plane in the first 128K and an enable
 * plane in the next 128K).  X11 uses only 256k+ region but tries to
 * map the whole thing, so we repeatedly map the first 256K to the
 * first page of the color screen.  If someone tries to use the overlay
 * and enable regions, they will get a surprise....
 *
 * As well, mapping at an offset of 0x04000000 causes the cg3 to be
 * mapped in flat mode without the cg4 emulation.
 */
paddr_t
cgthreemmap(dev_t dev, off_t off, int prot)
{
	struct cgthree_softc *sc = device_lookup_private(&cgthree_cd,
							 minor(dev));

#define START		(128*1024 + 128*1024)
#define NOOVERLAY	(0x04000000)

	if (off & PGOFSET)
		panic("cgthreemmap");
	if (off < 0)
		return (-1);
	if ((u_int)off >= NOOVERLAY)
		off -= NOOVERLAY;
	else if ((u_int)off >= START)
		off -= START;
	else
		off = 0;

	if (off >= sc->sc_fb.fb_type.fb_size)
		return (-1);

	return (bus_space_mmap(sc->sc_bustag,
		sc->sc_paddr, CG3REG_MEM + off,
		prot, BUS_SPACE_MAP_LINEAR));
}

#if NWSDISPLAY > 0

static void
cg3_setup_palette(struct cgthree_softc *sc)
{
	int i, j;

	j = 0;
	for (i = 0; i < 256; i++) {
		sc->sc_cmap.cm_map[i][0] = rasops_cmap[j];
		j++;
		sc->sc_cmap.cm_map[i][1] = rasops_cmap[j];
		j++;
		sc->sc_cmap.cm_map[i][2] = rasops_cmap[j];
		j++;
	}
	cgthreeloadcmap(sc, 0, 256);
}

int
cgthree_ioctl(void *v, void *vs, u_long cmd, void *data, int flag,
	struct lwp *l)
{
	/* we'll probably need to add more stuff here */
	struct vcons_data *vd = v;
	struct cgthree_softc *sc = vd->cookie;
	struct wsdisplay_fbinfo *wdf;
	struct rasops_info *ri = &sc->sc_fb.fb_rinfo;
	struct vcons_screen *ms = sc->vd.active;
	switch (cmd) {
		case WSDISPLAYIO_GTYPE:
			*(u_int *)data = WSDISPLAY_TYPE_SUNTCX;
			return 0;
		case WSDISPLAYIO_GINFO:
			wdf = (void *)data;
			wdf->height = ri->ri_height;
			wdf->width = ri->ri_width;
			wdf->depth = ri->ri_depth;
			wdf->cmsize = 256;
			return 0;

		case WSDISPLAYIO_GETCMAP:
			return cgthree_getcmap(sc, 
			    (struct wsdisplay_cmap *)data);
		case WSDISPLAYIO_PUTCMAP:
			return cgthree_putcmap(sc, 
			    (struct wsdisplay_cmap *)data);

		case WSDISPLAYIO_SMODE:
			{
				int new_mode = *(int*)data;
				if (new_mode != sc->sc_mode)
				{
					sc->sc_mode = new_mode;
					if(new_mode == WSDISPLAYIO_MODE_EMUL)
					{
						cg3_setup_palette(sc);
						vcons_redraw_screen(ms);
					}
				}
			}
	}
	return EPASSTHROUGH;
}

paddr_t
cgthree_mmap(void *v, void *vs, off_t offset, int prot)
{
	/* I'm not at all sure this is the right thing to do */
	return cgthreemmap(0, offset, prot); /* assume minor dev 0 for now */
}

int
cgthree_putcmap(struct cgthree_softc *sc, struct wsdisplay_cmap *cm)
{
	u_int index = cm->index;
	u_int count = cm->count;
	int error,i;
	if (index >= 256 || count > 256 || index + count > 256)
		return EINVAL;

	for (i = 0; i < count; i++)
	{
		error = copyin(&cm->red[i],
		    &sc->sc_cmap.cm_map[index + i][0], 1);
		if (error)
			return error;
		error = copyin(&cm->green[i],
		    &sc->sc_cmap.cm_map[index + i][1],
		    1);
		if (error)
			return error;
		error = copyin(&cm->blue[i],
		    &sc->sc_cmap.cm_map[index + i][2], 1);
		if (error)
			return error;
	}
	cgthreeloadcmap(sc, index, count);

	return 0;
}

int
cgthree_getcmap(struct cgthree_softc *sc, struct wsdisplay_cmap *cm)
{
	u_int index = cm->index;
	u_int count = cm->count;
	int error,i;

	if (index >= 256 || count > 256 || index + count > 256)
		return EINVAL;

	for (i = 0; i < count; i++)
	{
		error = copyout(&sc->sc_cmap.cm_map[index + i][0],
		    &cm->red[i], 1);
		if (error)
			return error;
		error = copyout(&sc->sc_cmap.cm_map[index + i][1],
		    &cm->green[i], 1);
		if (error)
			return error;
		error = copyout(&sc->sc_cmap.cm_map[index + i][2],
		    &cm->blue[i], 1);
		if (error)
			return error;
	}

	return 0;
}

void
cgthree_init_screen(void *cookie, struct vcons_screen *scr,
    int existing, long *defattr)
{
	struct cgthree_softc *sc = cookie;
	struct rasops_info *ri = &scr->scr_ri;

	scr->scr_flags |= VCONS_DONT_READ;

	ri->ri_depth = 8;
	ri->ri_width = sc->sc_width;
	ri->ri_height = sc->sc_height;
	ri->ri_stride = sc->sc_stride;
	ri->ri_flg = RI_CENTER;

	ri->ri_bits = sc->sc_fb.fb_pixels;

	rasops_init(ri, 0, 0);
	ri->ri_caps = WSSCREEN_WSCOLORS | WSSCREEN_REVERSE;
	rasops_reconfig(ri, sc->sc_height / ri->ri_font->fontheight,
		    sc->sc_width / ri->ri_font->fontwidth);

	ri->ri_hw = scr;
}

#endif /* NWSDISPLAY > 0 */
