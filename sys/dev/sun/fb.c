/*	$NetBSD: fb.c,v 1.35 2014/07/25 08:10:39 dholland Exp $ */

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
 *	@(#)fb.c	8.1 (Berkeley) 6/11/93
 */

/*
 * /dev/fb (indirect frame buffer driver).  This is gross; we should
 * just build cdevsw[] dynamically.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: fb.c,v 1.35 2014/07/25 08:10:39 dholland Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/conf.h>
#include <sys/malloc.h>
#include <sys/types.h>

#include <machine/promlib.h>
#include <machine/autoconf.h>
#include <machine/kbd.h>
#include <machine/eeprom.h>
#include <sparc/dev/cons.h>

#include <dev/sun/fbio.h>
#include <dev/sun/fbvar.h>

#include "kbd.h"
#include "pfour.h"


struct fbdevlist {
	struct fbdevice *fb_dev;
	struct fbdevlist *fb_next;
};

static struct fbdevlist fblist = {
    NULL,
    NULL,
};

dev_type_open(fbopen);
dev_type_close(fbclose);
dev_type_ioctl(fbioctl);
dev_type_poll(fbpoll);
dev_type_mmap(fbmmap);
dev_type_kqfilter(fbkqfilter);

const struct cdevsw fb_cdevsw = {
	.d_open = fbopen,
	.d_close = fbclose,
	.d_read = noread,
	.d_write = nowrite,
	.d_ioctl = fbioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = fbpoll,
	.d_mmap = fbmmap,
	.d_kqfilter = fbkqfilter,
	.d_discard = nodiscard,
	.d_flag = D_OTHER
};

void
fb_unblank(void)
{

	struct fbdevlist *fbl = &fblist;

	while (fbl != NULL && fbl->fb_dev != NULL) {
		(*fbl->fb_dev->fb_driver->fbd_unblank)(fbl->fb_dev->fb_device);
		fbl = fbl->fb_next;
	}
}

/*
 * Helper function for frame buffer devices. Decides whether
 * the device can be the console output device according to
 * PROM info. The result from this function may not be conclusive
 * on machines with old PROMs; in that case, drivers should consult
 * other sources of configuration information (e.g. EEPROM entries).
 */
int
fb_is_console(int node)
{
#if !defined(SUN4U)
	int fbnode;

	switch (prom_version()) {
	case PROM_OLDMON:
		/* `node' is not valid; just check for any fb device */
		return (prom_stdout() == PROMDEV_SCREEN);

	case PROM_OBP_V0:
		/*
		 * First, check if prom_stdout() represents a frame buffer,
		 * then match on the `fb' property on the root node, if any.
		 */
		if (prom_stdout() != PROMDEV_SCREEN)
			return (0);

		fbnode = prom_getpropint(findroot(), "fb", 0);
		return (fbnode == 0 || node == fbnode);

	case PROM_OBP_V2:
	case PROM_OBP_V3:
	case PROM_OPENFIRM:
		/* Just match the nodes */
		return (node == prom_stdout_node);
	}

	return (0);
#else
		return (node == prom_stdout_node);
#endif
}

void
fb_attach(struct fbdevice *fb, int isconsole)
{
	static int seen_force = 0;
	int nfb = 0;
	struct fbdevlist *fbl = &fblist;

	/*
	 * Check to see if we're being forced into /dev/fb0, or if we're
	 * the console.  If we are, then move/replace the current fb0.
	 */
	if ((fb->fb_flags & FB_FORCE || (isconsole && !seen_force)) &&
	    fblist.fb_dev != NULL) {
		while (fbl->fb_next != NULL) {
			fbl = fbl->fb_next;
			nfb++;
		}
		if ((fbl->fb_next = malloc(sizeof (struct fbdevlist),
		    M_DEVBUF, M_NOWAIT)) == NULL)
			printf("%s: replacing %s at /dev/fb0\n",
			    device_xname(fb->fb_device),
			    device_xname(fblist.fb_dev->fb_device));
		else {
			fbl = fbl->fb_next;
			nfb++;
			fbl->fb_dev = fblist.fb_dev;
			fbl->fb_next = NULL;
			aprint_normal_dev(fbl->fb_dev->fb_device,
			    "moved to /dev/fb%d\n", nfb);
			aprint_normal_dev(fbl->fb_dev->fb_device,
			    "attached to /dev/fb0\n");
		}
		fblist.fb_dev = fb;
		if (fb->fb_flags & FB_FORCE)
			seen_force = 1;
	/* Add to end of fb list. */
	} else {
		if (fblist.fb_dev != NULL) {
			while (fbl->fb_next != NULL) {
				fbl = fbl->fb_next;
				nfb++;
			}
			if ((fbl->fb_next = malloc(sizeof (struct fbdevlist),
			    M_DEVBUF, M_NOWAIT)) == NULL) {
				aprint_error_dev(fb->fb_device,
				    "no space to attach after /dev/fb%d\n",
				    nfb);
				return;
			}
			fbl = fbl->fb_next;
			nfb++;
		}
		fbl->fb_dev = fb;
		fbl->fb_next = NULL;
		aprint_normal_dev(fbl->fb_dev->fb_device,
		     "attached to /dev/fb%d\n", nfb);
	}
}

int
fbopen(dev_t dev, int flags, int mode, struct lwp *l)
{
	int unit, nunit;
	struct fbdevlist *fbl = &fblist;

	unit = minor(dev);
	while (unit-- && fbl != NULL)
		fbl = fbl->fb_next;
	if (fbl == NULL || fbl->fb_dev == NULL)
		return (ENXIO);
		
	nunit = device_unit(fbl->fb_dev->fb_device);
	return (fbl->fb_dev->fb_driver->fbd_open)(makedev(0, nunit), flags,
	    mode, l);
}

int
fbclose(dev_t dev, int flags, int mode, struct lwp *l)
{
	int unit, nunit;
	struct fbdevlist *fbl = &fblist;

	unit = minor(dev);
	while (unit-- && fbl != NULL)
		fbl = fbl->fb_next;
	if (fbl == NULL || fbl->fb_dev == NULL)
		return (ENXIO);

	nunit = device_unit(fbl->fb_dev->fb_device);
	return (fbl->fb_dev->fb_driver->fbd_close)(makedev(0, nunit), flags,
	    mode, l);
}

int
fbioctl(dev_t dev, u_long cmd, void *data, int flags, struct lwp *l)
{
	int unit, nunit;
	struct fbdevlist *fbl = &fblist;

	unit = minor(dev);
	while (unit-- && fbl != NULL)
		fbl = fbl->fb_next;
	if (fbl == NULL || fbl->fb_dev == NULL)
		return (ENXIO);

	nunit = device_unit(fbl->fb_dev->fb_device);
	return (fbl->fb_dev->fb_driver->fbd_ioctl)(makedev(0, nunit), cmd, 
	    data, flags, l);
}

int
fbpoll(dev_t dev, int events, struct lwp *l)
{
	int unit, nunit;
	struct fbdevlist *fbl = &fblist;

	unit = minor(dev);
	while (unit-- && fbl != NULL)
		fbl = fbl->fb_next;
	if (fbl == NULL || fbl->fb_dev == NULL)
		return (ENXIO);

	nunit = device_unit(fbl->fb_dev->fb_device);
	return (fbl->fb_dev->fb_driver->fbd_poll)(makedev(0, nunit), events,
	    l);
}

int
fbkqfilter(dev_t dev, struct knote *kn)
{
	int unit, nunit;
	struct fbdevlist *fbl = &fblist;

	unit = minor(dev);
	while (unit-- && fbl != NULL)
		fbl = fbl->fb_next;
	if (fbl == NULL || fbl->fb_dev == NULL)
		return (ENXIO);

	nunit = device_unit(fbl->fb_dev->fb_device);
	return (fbl->fb_dev->fb_driver->fbd_kqfilter)(makedev(0, nunit), kn);
}

paddr_t
fbmmap(dev_t dev, off_t off, int prot)
{
	int unit, nunit;
	struct fbdevlist *fbl = &fblist;

	unit = minor(dev);
	while (unit-- && fbl != NULL)
		fbl = fbl->fb_next;
	if (fbl == NULL || fbl->fb_dev == NULL)
		return (ENXIO);

	nunit = device_unit(fbl->fb_dev->fb_device);
	paddr_t (*map)(dev_t, off_t, int) = fbl->fb_dev->fb_driver->fbd_mmap;

	if (map == NULL)
		return (-1);
	return (map(makedev(0, nunit), off, prot));
}

void
fb_setsize_obp(struct fbdevice *fb, int depth, int def_width, int def_height, int node)
{
	fb->fb_type.fb_width = prom_getpropint(node, "width", def_width);
	fb->fb_type.fb_height = prom_getpropint(node, "height", def_height);
	fb->fb_linebytes = prom_getpropint(node, "linebytes",
				     (fb->fb_type.fb_width * depth) / 8);
}

void
fb_setsize_eeprom(struct fbdevice *fb, int depth, int def_width, int def_height)
{
#if !defined(SUN4U)
	struct eeprom *eep = (struct eeprom *)eeprom_va;

	if (!CPU_ISSUN4) {
		printf("fb_setsize_eeprom: not sun4\n");
		return;
	}

	/* Set up some defaults. */
	fb->fb_type.fb_width = def_width;
	fb->fb_type.fb_height = def_height;

	if (fb->fb_flags & FB_PFOUR) {
#if NPFOUR > 0
		fb_setsize_pfour(fb);
#endif
	} else if (eep != NULL) {
		switch (eep->eeScreenSize) {
		case EE_SCR_1152X900:
			fb->fb_type.fb_width = 1152;
			fb->fb_type.fb_height = 900;
			break;

		case EE_SCR_1024X1024:
			fb->fb_type.fb_width = 1024;
			fb->fb_type.fb_height = 1024;
			break;

		case EE_SCR_1600X1280:
			fb->fb_type.fb_width = 1600;
			fb->fb_type.fb_height = 1280;
			break;

		case EE_SCR_1440X1440:
			fb->fb_type.fb_width = 1440;
			fb->fb_type.fb_height = 1440;
			break;

		default:
			/*
			 * XXX: Do nothing, I guess.
			 * Should we print a warning about
			 * an unknown value? --thorpej
			 */
			break;
		}
	}

	fb->fb_linebytes = (fb->fb_type.fb_width * depth) / 8;
#endif /* !SUN4U */
}



#ifdef RASTERCONSOLE
static void fb_bell(int);

static void
fb_bell(int on)
{
#if NKBD > 0
	kbd_bell(on);
#endif
}

void
fbrcons_init(struct fbdevice *fb)
{
	struct rconsole	*rc = &fb->fb_rcons;
	struct rasops_info *ri = &fb->fb_rinfo;
	int maxrow, maxcol;
#if !defined(RASTERCONS_FULLSCREEN)
	int *row, *col;
#endif

	/* Set up what rasops needs to know about */
	memset(ri, 0, sizeof *ri);
	ri->ri_stride = fb->fb_linebytes;
	ri->ri_bits = (void *)fb->fb_pixels;
	ri->ri_depth = fb->fb_type.fb_depth;
	ri->ri_width = fb->fb_type.fb_width;
	ri->ri_height = fb->fb_type.fb_height;
	maxrow = 5000;
	maxcol = 5000;

#if !defined(RASTERCONS_FULLSCREEN)
#if !defined(SUN4U)
	if (CPU_ISSUN4) {
		struct eeprom *eep = (struct eeprom *)eeprom_va;

		if (eep == NULL) {
			maxcol = 80;
			maxrow = 34;
		} else {
			maxcol = eep->eeTtyCols;
			maxrow = eep->eeTtyRows;
		}
	}
#endif /* !SUN4U */
	if (!CPU_ISSUN4) {
		char buf[6+1];	/* Enough for six digits */
		maxcol = (prom_getoption("screen-#columns", buf, sizeof buf) == 0)
			? strtoul(buf, NULL, 10)
			: 80;

		maxrow = (prom_getoption("screen-#rows", buf, sizeof buf) != 0)
			? strtoul(buf, NULL, 10)
			: 34;

	}
#endif /* !RASTERCONS_FULLSCREEN */
	/*
	 * - force monochrome output
	 * - eraserows() hack to clear the *entire* display
	 * - cursor is currently enabled
	 * - center output
	 */
	ri->ri_flg = RI_FULLCLEAR | RI_CURSOR | RI_CENTER;

	/* Get operations set and connect to rcons */
	if (rasops_init(ri, maxrow, maxcol))
		panic("fbrcons_init: rasops_init failed!");

	if (ri->ri_depth == 8) {
		int i;
		for (i = 0; i < 16; i++) {

			/*
			 * Cmap entries are repeated four times in the
			 * 32 bit wide `devcmap' entries for optimization
			 * purposes; see rasops(9)
			 */
#define I_TO_DEVCMAP(i)	((i) | ((i)<<8) | ((i)<<16) | ((i)<<24))

			/*
			 * Use existing colormap entries for black and white
			 */
			if ((i & 7) == WSCOL_BLACK) {
				ri->ri_devcmap[i] = I_TO_DEVCMAP(255);
				continue;
			}

			if ((i & 7) == WSCOL_WHITE) {
				ri->ri_devcmap[i] = I_TO_DEVCMAP(0);
				continue;
			}
			/*
			 * Other entries refer to ANSI map, which for now
			 * is setup in bt_subr.c
			 */
			ri->ri_devcmap[i] = I_TO_DEVCMAP(i + 1);
#undef I_TO_DEVCMAP
		}
	}

	rc->rc_row = rc->rc_col = 0;
#if !defined(RASTERCONS_FULLSCREEN)
	/* Determine addresses of prom emulator row and column */
	if (!CPU_ISSUN4 && !romgetcursoraddr(&row, &col)) {
		rc->rc_row = *row;
		rc->rc_col = *col;
	}
#endif
	ri->ri_crow = rc->rc_row;
	ri->ri_ccol = rc->rc_col;

	rc->rc_ops = &ri->ri_ops;
	rc->rc_cookie = ri;
	rc->rc_bell = fb_bell;
	rc->rc_maxcol = ri->ri_cols;
	rc->rc_maxrow = ri->ri_rows;
	rc->rc_width = ri->ri_emuwidth;
	rc->rc_height = ri->ri_emuheight;
	rc->rc_deffgcolor = WSCOL_BLACK;
	rc->rc_defbgcolor = WSCOL_WHITE;
	rcons_init(rc, 0);

	/* Hook up virtual console */
	v_putc = rcons_cnputc;
}

int
fbrcons_rows(void)
{
	return ((fblist.fb_dev != NULL) ?
	    fblist.fb_dev->fb_rcons.rc_maxrow : 0);
}

int
fbrcons_cols(void)
{
	return ((fblist.fb_dev != NULL) ?
	    fblist.fb_dev->fb_rcons.rc_maxcol : 0);
}
#endif /* RASTERCONSOLE */
