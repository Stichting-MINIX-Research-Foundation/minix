/*	$NetBSD: bwtwo.c,v 1.34 2014/07/25 08:10:39 dholland Exp $ */

/*-
 * Copyright (c) 1996, 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)bwtwo.c	8.1 (Berkeley) 6/11/93
 */

/*
 * black & white display (bwtwo) driver.
 *
 * Does not handle interrupts, even though they can occur.
 *
 * P4 and overlay plane support by Jason R. Thorpe <thorpej@NetBSD.org>.
 * Overlay plane handling hints and ideas provided by Brad Spencer.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: bwtwo.c,v 1.34 2014/07/25 08:10:39 dholland Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>
#include <sys/mman.h>
#include <sys/tty.h>
#include <sys/conf.h>

#include <machine/autoconf.h>
#include <machine/eeprom.h>

#include <dev/sun/fbio.h>
#include <dev/sun/fbvar.h>

#include <dev/sun/btreg.h>
#include <dev/sun/bwtworeg.h>
#include <dev/sun/bwtwovar.h>
#include <dev/sun/pfourreg.h>

#if NWSDISPLAY > 0
#include <dev/wscons/wsconsio.h>
#include <dev/wsfont/wsfont.h>
#include <dev/rasops/rasops.h>

#include "opt_wsemul.h"
#endif

#include "ioconf.h"

dev_type_open(bwtwoopen);
dev_type_ioctl(bwtwoioctl);
dev_type_mmap(bwtwommap);

const struct cdevsw bwtwo_cdevsw = {
	.d_open = bwtwoopen,
	.d_close = nullclose,
	.d_read = noread,
	.d_write = nowrite,
	.d_ioctl = bwtwoioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = nopoll,
	.d_mmap = bwtwommap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = D_OTHER
};

/* XXX we do not handle frame buffer interrupts (do not know how) */
static void	bwtwounblank(device_t);

/* frame buffer generic driver */
static struct fbdriver bwtwofbdriver = {
	bwtwounblank, bwtwoopen, nullclose, bwtwoioctl, nopoll, bwtwommap,
	nokqfilter
};

#if NWSDISPLAY > 0
#ifdef RASTERCONSOLE
#error RASTERCONSOLE and wsdisplay are mutually exclusive
#endif

struct wsscreen_descr bwtwo_defaultscreen = {
	"std",
	0, 0,	/* will be filled in -- XXX shouldn't, it's global */
	NULL,		/* textops */
	8, 16,	/* font width/height */
	0,	/* capabilities */
	NULL	/* modecookie */
};

static int 	bwtwo_ioctl(void *, void *, u_long, void *, int, struct lwp *);
static paddr_t	bwtwo_mmap(void *, void *, off_t, int);
static void	bwtwo_init_screen(void *, struct vcons_screen *, int, long *);

struct wsdisplay_accessops bwtwo_accessops = {
	bwtwo_ioctl,
	bwtwo_mmap,
	NULL,	/* alloc_screen */
	NULL,	/* free_screen */
	NULL,	/* show_screen */
	NULL, 	/* load_font */
	NULL,	/* pollc */
	NULL	/* scroll */
};

const struct wsscreen_descr *_bwtwo_scrlist[] = {
	&bwtwo_defaultscreen
};

struct wsscreen_list bwtwo_screenlist = {
	sizeof(_bwtwo_scrlist) / sizeof(struct wsscreen_descr *),
	_bwtwo_scrlist
};


static struct vcons_screen bw2_console_screen;
#endif /* NWSDISPLAY > 0 */

int
bwtwo_pfour_probe(void *vaddr, void *arg)
{
	cfdata_t cf = arg;

	switch (fb_pfour_id(vaddr)) {
	case PFOUR_ID_BW:
	case PFOUR_ID_COLOR8P1:		/* bwtwo in ... */
	case PFOUR_ID_COLOR24:		/* ...overlay plane */
		/* This is wrong; should be done in bwtwo_attach() */
		cf->cf_flags |= FB_PFOUR;
		/* FALLTHROUGH */
	case PFOUR_NOTPFOUR:
		return (1);
	}
	return (0);
}

void
bwtwoattach(struct bwtwo_softc *sc, const char *name, int isconsole)
{
	struct fbdevice *fb = &sc->sc_fb;
	int isoverlay;
#if NWSDISPLAY > 0
	struct wsemuldisplaydev_attach_args aa;
	struct rasops_info *ri = &bw2_console_screen.scr_ri;
	unsigned long defattr = 0;
#endif

	/* Fill in the remaining fbdevice values */
	fb->fb_driver = &bwtwofbdriver;
	fb->fb_device = sc->sc_dev;
	fb->fb_type.fb_type = FBTYPE_SUN2BW;
	fb->fb_type.fb_cmsize = 0;
	fb->fb_type.fb_size = fb->fb_type.fb_height * fb->fb_linebytes;
	printf(": %s, %d x %d", name,
	       fb->fb_type.fb_width, fb->fb_type.fb_height);

	/* Are we an overlay bw2? */
	if ((fb->fb_flags & FB_PFOUR) == 0 || (sc->sc_ovtype == BWO_NONE))
		isoverlay = 0;
	else
		isoverlay = 1;

	/* Insure video is enabled */
	sc->sc_set_video(sc, 1);

	if (isconsole) {
		printf(" (console)\n");
#ifdef RASTERCONSOLE
		/*
		 * XXX rcons doesn't seem to work properly on the overlay
		 * XXX plane.  This is a temporary kludge until someone
		 * XXX fixes it.
		 */
		if (!isoverlay)
			fbrcons_init(fb);
#endif
	} else
		printf("\n");

	if (isoverlay) {
		const char *ovnam;

		switch (sc->sc_ovtype) {
		case BWO_CGFOUR:
			ovnam = "cgfour";
			break;

		case BWO_CGEIGHT:
			ovnam = "cgeight";
			break;

		default:
			ovnam = "unknown";
			break;
		}
		printf("%s: %s overlay plane\n",
		    device_xname(sc->sc_dev), ovnam);
	}

	/*
	 * If we're on an overlay plane of a color framebuffer,
	 * then we don't force the issue in fb_attach() because
	 * we'd like the color framebuffer to actually be the
	 * "console framebuffer".  We're only around to speed
	 * up rconsole.
	 */
	if (isoverlay)
		fb_attach(fb, 0);
	else
		fb_attach(fb, isconsole);

#if NWSDISPLAY > 0
	sc->sc_width = fb->fb_type.fb_width;
	sc->sc_stride = fb->fb_type.fb_width/8;
	sc->sc_height = fb->fb_type.fb_height;

	/* setup rasops and so on for wsdisplay */
	sc->sc_mode = WSDISPLAYIO_MODE_EMUL;

	vcons_init(&sc->vd, sc, &bwtwo_defaultscreen, &bwtwo_accessops);
	sc->vd.init_screen = bwtwo_init_screen;

	if(isconsole && !isoverlay) {
		/* we mess with bw2_console_screen only once */
		vcons_init_screen(&sc->vd, &bw2_console_screen, 1,
		    &defattr);
		bw2_console_screen.scr_flags |= VCONS_SCREEN_IS_STATIC;

		bwtwo_defaultscreen.textops = &ri->ri_ops;
		bwtwo_defaultscreen.capabilities = ri->ri_caps;
		bwtwo_defaultscreen.nrows = ri->ri_rows;
		bwtwo_defaultscreen.ncols = ri->ri_cols;
		sc->vd.active = &bw2_console_screen;
		wsdisplay_cnattach(&bwtwo_defaultscreen, ri, 0, 0, defattr);
	} else {
		/* 
		 * we're not the console so we just clear the screen and don't 
		 * set up any sort of text display
		 */
		if (bwtwo_defaultscreen.textops == NULL) {
			/* 
			 * ugly, but...
			 * we want the console settings to win, so we only
			 * touch anything when we find an untouched screen
			 * definition. In this case we fill it from fb to
			 * avoid problems in case no bwtwo is the console
			 */
			ri = &sc->sc_fb.fb_rinfo;
			bwtwo_defaultscreen.textops = &ri->ri_ops;
			bwtwo_defaultscreen.capabilities = ri->ri_caps;
			bwtwo_defaultscreen.nrows = ri->ri_rows;
			bwtwo_defaultscreen.ncols = ri->ri_cols;
		}
	}

	aa.scrdata = &bwtwo_screenlist;
	if (isoverlay)
		aa.console = 0;
	else
		aa.console = isconsole;
	aa.accessops = &bwtwo_accessops;
	aa.accesscookie = &sc->vd;
	config_found(sc->sc_dev, &aa, wsemuldisplaydevprint);
#endif

}

int
bwtwoopen(dev_t dev, int flags, int mode, struct lwp *l)
{
	int unit = minor(dev);

	if (device_lookup(&bwtwo_cd, unit) == NULL)
		return (ENXIO);

	return (0);
}

int
bwtwoioctl(dev_t dev, u_long cmd, void *data, int flags, struct lwp *l)
{
	struct bwtwo_softc *sc = device_lookup_private(&bwtwo_cd, minor(dev));

	switch (cmd) {

	case FBIOGTYPE:
		*(struct fbtype *)data = sc->sc_fb.fb_type;
		break;

	case FBIOGVIDEO:
		*(int *)data = sc->sc_get_video(sc);
		break;

	case FBIOSVIDEO:
		sc->sc_set_video(sc, (*(int *)data));
		break;

	default:
		return (ENOTTY);
	}
	return (0);
}

static void
bwtwounblank(device_t dev)
{
	struct bwtwo_softc *sc = device_private(dev);

	sc->sc_set_video(sc, 1);
}

/*
 * Return the address that would map the given device at the given
 * offset, allowing for the given protection, or return -1 for error.
 */
paddr_t
bwtwommap(dev_t dev, off_t off, int prot)
{
	struct bwtwo_softc *sc = device_lookup_private(&bwtwo_cd, minor(dev));

	if (off & PGOFSET)
		panic("bwtwommap");

	if (off >= sc->sc_fb.fb_type.fb_size)
		return (-1);

	return (bus_space_mmap(sc->sc_bustag,
		sc->sc_paddr, sc->sc_pixeloffset + off,
		prot, BUS_SPACE_MAP_LINEAR));
}

#if NWSDISPLAY > 0

int
bwtwo_ioctl(void *v, void *vs, u_long cmd, void *data, int flag,
	struct lwp *l)
{
	/* we'll probably need to add more stuff here */
	struct vcons_data *vd = v;
	struct bwtwo_softc *sc = vd->cookie;
	struct wsdisplay_fbinfo *wdf;
	struct rasops_info *ri = &sc->sc_fb.fb_rinfo;
	struct vcons_screen *ms = sc->vd.active;
	switch (cmd) {
		case WSDISPLAYIO_GTYPE:
			*(u_int *)data = WSDISPLAY_TYPE_GENFB;
			return 0;
		case WSDISPLAYIO_GINFO:
			wdf = (void *)data;
			wdf->height = ri->ri_height;
			wdf->width = ri->ri_width;
			wdf->depth = ri->ri_depth;
			wdf->cmsize = 0;
			return 0;

		case WSDISPLAYIO_GETCMAP:
			return EINVAL;
		case WSDISPLAYIO_PUTCMAP:
			return EINVAL;

		case WSDISPLAYIO_SMODE:
			{
				int new_mode = *(int*)data;
				if (new_mode != sc->sc_mode)
				{
					sc->sc_mode = new_mode;
					if(new_mode == WSDISPLAYIO_MODE_EMUL)
					{
						vcons_redraw_screen(ms);
					}
				}
			}
	}
	return EPASSTHROUGH;
}

paddr_t
bwtwo_mmap(void *v, void *vs, off_t offset, int prot)
{
	/* I'm not at all sure this is the right thing to do */
	return bwtwommap(0, offset, prot); /* assume minor dev 0 for now */
}

void
bwtwo_init_screen(void *cookie, struct vcons_screen *scr,
    int existing, long *defattr)
{
	struct bwtwo_softc *sc = cookie;
	struct rasops_info *ri = &scr->scr_ri;
	char *bits;

	ri->ri_depth = 1;
	ri->ri_width = sc->sc_width;
	ri->ri_height = sc->sc_height;
	ri->ri_stride = sc->sc_stride;
	ri->ri_flg = RI_CENTER;

	ri->ri_bits = sc->sc_fb.fb_pixels;

	/*
	 * Make sure that we set a maximum of 32 bits at a time,
	 * otherwise we'll see VME write errors if this is a P4 BW2.
	 */
	for (bits = (char *) ri->ri_bits;
	    bits < (char *) ri->ri_bits + ri->ri_stride * ri->ri_height;
	    bits += 4)
		memset(bits, (*defattr >> 16) & 0xff, 4);
	rasops_init(ri, 0, 0);
	ri->ri_caps = 0;
	rasops_reconfig(ri, sc->sc_height / ri->ri_font->fontheight,
		    sc->sc_width / ri->ri_font->fontwidth);

	ri->ri_hw = scr;
}

#endif /* NWSDISPLAY > 0 */
