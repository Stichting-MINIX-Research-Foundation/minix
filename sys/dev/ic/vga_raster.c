/*	$NetBSD: vga_raster.c,v 1.44 2015/03/01 07:05:59 mlelstv Exp $	*/

/*
 * Copyright (c) 2001, 2002 Bang Jun-Young
 * Copyright (c) 2004 Julio M. Merino Vidal
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 * Copyright (c) 1995, 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: vga_raster.c,v 1.44 2015/03/01 07:05:59 mlelstv Exp $");

#include "opt_vga.h"
#include "opt_wsmsgattrs.h" /* for WSDISPLAY_CUSTOM_OUTPUT */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/queue.h>
#include <sys/bus.h>

#include <dev/ic/mc6845reg.h>
#include <dev/ic/pcdisplayvar.h>
#include <dev/ic/vgareg.h>
#include <dev/ic/vgavar.h>
#include <dev/videomode/videomode.h>

#include <dev/wscons/wsdisplayvar.h>
#include <dev/wscons/wsconsio.h>
#include <dev/wsfont/wsfont.h>

#include <dev/ic/pcdisplay.h>

int vga_no_builtinfont = 0;

u_int8_t builtinfont_data[256 * 16];

struct wsdisplay_font builtinfont = {
	"builtin",			/* typeface name */
	0,				/* firstchar */
	256,				/* numchars */
	WSDISPLAY_FONTENC_IBM,		/* encoding */
	8,				/* width */
	16,				/* height */
	1,				/* stride */
	WSDISPLAY_FONTORDER_L2R,	/* bit order */
	WSDISPLAY_FONTORDER_L2R,	/* byte order */
	builtinfont_data		/* data */
};

struct vga_scrmem {
	u_int16_t ch;
	u_int8_t attr;
	u_int8_t second;	/* XXXBJY should be u_int8_t len; */
	u_int8_t enc;
};

#ifdef VGA_CONSOLE_SCREENTYPE
#define VGA_SCRMEM_SIZE		(80 * 30)
#else
#define VGA_SCRMEM_SIZE		(80 * 25)
#endif

struct vga_scrmem boot_scrmem[VGA_SCRMEM_SIZE];

struct vga_raster_font {
	LIST_ENTRY(vga_raster_font) next;
	struct wsdisplay_font *font;
};

struct vgascreen {
	LIST_ENTRY(vgascreen) next;
	struct vga_config *cfg;
	struct vga_handle *hdl;
	const struct wsscreen_descr *type;

	int active;
	struct vga_scrmem *mem;
	int encoding;

	int dispoffset;
	int mindispoffset;
	int maxdispoffset;

	int cursoron;			/* Is cursor displayed? */
	int cursorcol;			/* Current cursor column */
	int cursorrow;			/* Current cursor row */
	struct vga_scrmem cursortmp;
	int cursorstride;

	LIST_HEAD(, vga_raster_font) fontset;
};

struct vga_moderegs {
	u_int8_t miscout;		/* Misc. output */
	u_int8_t crtc[MC6845_NREGS];	/* CRTC controller */
	u_int8_t atc[VGA_ATC_NREGS];	/* Attribute controller */
	u_int8_t ts[VGA_TS_NREGS];	/* Time sequencer */
	u_int8_t gdc[VGA_GDC_NREGS];	/* Graphics display controller */
};

static int vgaconsole, vga_console_type, vga_console_attached;
static struct vgascreen vga_console_screen;
static struct vga_config vga_console_vc;
static struct vga_raster_font vga_console_fontset_ascii;
static struct videomode vga_console_modes[2] = {
	/* 640x400 for 80x25, 80x40 and 80x50 modes */
	{
		25175, 640, 664, 760, 800, 400, 409, 411, 450, 0, NULL,
	},
	/* 640x480 for 80x30 mode */
	{
		25175, 640, 664, 760, 800, 480, 491, 493, 525, 0, NULL,
	}
};

static void vga_raster_init(struct vga_config *, bus_space_tag_t,
		bus_space_tag_t);
static void vga_raster_init_screen(struct vga_config *, struct vgascreen *,
		const struct wsscreen_descr *, int, long *);
static void vga_raster_setup_font(struct vga_config *, struct vgascreen *);
static void vga_setup_regs(struct videomode *, struct vga_moderegs *);
static void vga_set_mode(struct vga_handle *, struct vga_moderegs *);
static void vga_restore_screen(struct vgascreen *,
		const struct wsscreen_descr *, struct vga_scrmem *);
static void vga_raster_cursor_init(struct vgascreen *, int);
static void _vga_raster_putchar(void *, int, int, u_int, long,
		struct vga_raster_font *);

static void vga_raster_cursor(void *, int, int, int);
static int  vga_raster_mapchar(void *, int, u_int *);
static void vga_raster_putchar(void *, int, int, u_int, long);
static void vga_raster_copycols(void *, int, int, int, int);
static void vga_raster_erasecols(void *, int, int, int, long);
static void vga_raster_copyrows(void *, int, int, int);
static void vga_raster_eraserows(void *, int, int, long);
static int  vga_raster_allocattr(void *, int, int, int, long *);
#ifdef WSDISPLAY_CUSTOM_OUTPUT
static void vga_raster_replaceattr(void *, long, long);
#endif /* WSDISPLAY_CUSTOM_OUTPUT */

const struct wsdisplay_emulops vga_raster_emulops = {
	vga_raster_cursor,
	vga_raster_mapchar,
	vga_raster_putchar,
	vga_raster_copycols,
	vga_raster_erasecols,
	vga_raster_copyrows,
	vga_raster_eraserows,
	vga_raster_allocattr,
#ifdef WSDISPLAY_CUSTOM_OUTPUT
	vga_raster_replaceattr,
#else /* WSDISPLAY_CUSTOM_OUTPUT */
	NULL,
#endif /* WSDISPLAY_CUSTOM_OUTPUT */
};

/*
 * translate WS(=ANSI) color codes to standard pc ones
 */
static const unsigned char fgansitopc[] = {
#ifdef __alpha__
	/*
	 * XXX DEC HAS SWITCHED THE CODES FOR BLUE AND RED!!!
	 * XXX We should probably not bother with this
	 * XXX (reinitialize the palette registers).
	 */
	FG_BLACK, FG_BLUE, FG_GREEN, FG_CYAN, FG_RED,
	FG_MAGENTA, FG_BROWN, FG_LIGHTGREY
#else
	FG_BLACK, FG_RED, FG_GREEN, FG_BROWN, FG_BLUE,
	FG_MAGENTA, FG_CYAN, FG_LIGHTGREY
#endif
}, bgansitopc[] = {
#ifdef __alpha__
	BG_BLACK, BG_BLUE, BG_GREEN, BG_CYAN, BG_RED,
	BG_MAGENTA, BG_BROWN, BG_LIGHTGREY
#else
	BG_BLACK, BG_RED, BG_GREEN, BG_BROWN, BG_BLUE,
	BG_MAGENTA, BG_CYAN, BG_LIGHTGREY
#endif
};

const struct wsscreen_descr vga_25lscreen = {
	"80x25", 80, 25,
	&vga_raster_emulops,
	8, 16,
	WSSCREEN_WSCOLORS | WSSCREEN_HILIT | WSSCREEN_BLINK,
	&vga_console_modes[0]
}, vga_25lscreen_mono = {
	"80x25", 80, 25,
	&vga_raster_emulops,
	8, 16,
	WSSCREEN_HILIT | WSSCREEN_UNDERLINE | WSSCREEN_BLINK | WSSCREEN_REVERSE,
	&vga_console_modes[0]
}, vga_30lscreen = {
	"80x30", 80, 30,
	&vga_raster_emulops,
	8, 16,
	WSSCREEN_WSCOLORS | WSSCREEN_HILIT | WSSCREEN_BLINK,
	&vga_console_modes[1]
}, vga_30lscreen_mono = {
	"80x30", 80, 30,
	&vga_raster_emulops,
	8, 16,
	WSSCREEN_HILIT | WSSCREEN_UNDERLINE | WSSCREEN_BLINK | WSSCREEN_REVERSE,
	&vga_console_modes[1]
}, vga_40lscreen = {
	"80x40", 80, 40,
	&vga_raster_emulops,
	8, 10,
	WSSCREEN_WSCOLORS | WSSCREEN_HILIT | WSSCREEN_BLINK,
	&vga_console_modes[0]
}, vga_40lscreen_mono = {
	"80x40", 80, 40,
	&vga_raster_emulops,
	8, 10,
	WSSCREEN_HILIT | WSSCREEN_UNDERLINE | WSSCREEN_BLINK | WSSCREEN_REVERSE,
	&vga_console_modes[0]
}, vga_50lscreen = {
	"80x50", 80, 50,
	&vga_raster_emulops,
	8, 8,
	WSSCREEN_WSCOLORS | WSSCREEN_HILIT | WSSCREEN_BLINK,
	&vga_console_modes[0]
}, vga_50lscreen_mono = {
	"80x50", 80, 50,
	&vga_raster_emulops,
	8, 8,
	WSSCREEN_HILIT | WSSCREEN_UNDERLINE | WSSCREEN_BLINK | WSSCREEN_REVERSE,
	&vga_console_modes[0]
};

const struct wsscreen_descr *_vga_scrlist[] = {
	&vga_25lscreen,
	&vga_30lscreen,
	&vga_40lscreen,
	&vga_50lscreen,
}, *_vga_scrlist_mono[] = {
	&vga_25lscreen_mono,
	&vga_30lscreen_mono,
	&vga_40lscreen_mono,
	&vga_50lscreen_mono,
};

const struct wsscreen_list vga_screenlist = {
	sizeof(_vga_scrlist) / sizeof(struct wsscreen_descr *),
	_vga_scrlist
}, vga_screenlist_mono = {
	sizeof(_vga_scrlist_mono) / sizeof(struct wsscreen_descr *),
	_vga_scrlist_mono
};

static int	vga_raster_ioctl(void *, void *, u_long, void *, int,
		    struct lwp *);
static paddr_t	vga_raster_mmap(void *, void *, off_t, int);
static int	vga_raster_alloc_screen(void *, const struct wsscreen_descr *,
		    void **, int *, int *, long *);
static void	vga_raster_free_screen(void *, void *);
static int	vga_raster_show_screen(void *, void *, int,
		    void (*)(void *, int, int), void *);
static int	vga_raster_load_font(void *, void *, struct wsdisplay_font *);

static void 	vga_switch_screen(struct vga_config *);
static void 	vga_raster_setscreentype(struct vga_config *,
		    const struct wsscreen_descr *);

const struct wsdisplay_accessops vga_raster_accessops = {
	vga_raster_ioctl,
	vga_raster_mmap,
	vga_raster_alloc_screen,
	vga_raster_free_screen,
	vga_raster_show_screen,
	vga_raster_load_font,
	NULL,	/* pollc */
	NULL,	/* scroll */
};

int
vga_cnattach(bus_space_tag_t iot, bus_space_tag_t memt, int type, int check)
{
	long defattr;
	const struct wsscreen_descr *scr;
#ifdef VGA_CONSOLE_SCREENTYPE
	const char *typestr = NULL;
#endif

	if (check && !vga_common_probe(iot, memt))
		return (ENXIO);

	/* set up bus-independent VGA configuration */
	vga_raster_init(&vga_console_vc, iot, memt);
#ifdef VGA_CONSOLE_SCREENTYPE
	scr = wsdisplay_screentype_pick(vga_console_vc.hdl.vh_mono ?
	    &vga_screenlist_mono : &vga_screenlist, VGA_CONSOLE_SCREENTYPE);
	if (!scr)
		/* Invalid screen type, continue with the default mode. */
		typestr = "80x25";
	else if (scr->nrows > 30)
		/* Unsupported screen type, try 80x30. */
		typestr = "80x30";
	if (typestr)
		scr = wsdisplay_screentype_pick(vga_console_vc.hdl.vh_mono ?
		    &vga_screenlist_mono : &vga_screenlist, typestr);
	if (scr != vga_console_vc.currenttype)
		vga_console_vc.currenttype = scr;
#else
	scr = vga_console_vc.currenttype;
#endif
	vga_raster_init_screen(&vga_console_vc, &vga_console_screen, scr, 1,
	    &defattr);

	vga_console_screen.active = 1;
	vga_console_vc.active = &vga_console_screen;

	wsdisplay_cnattach(scr, &vga_console_screen,
	    vga_console_screen.cursorcol, vga_console_screen.cursorrow,
	    defattr);

	vgaconsole = 1;
	vga_console_type = type;
	return (0);
}

static void
vga_raster_init(struct vga_config *vc, bus_space_tag_t iot,
    bus_space_tag_t memt)
{
	struct vga_handle *vh = &vc->hdl;
	u_int8_t mor;
	struct vga_raster_font *vf;

	vh->vh_iot = iot;
	vh->vh_memt = memt;

	if (bus_space_map(vh->vh_iot, 0x3c0, 0x10, 0, &vh->vh_ioh_vga))
		panic("vga_raster_init: couldn't map vga io");

	/* read "misc output register" */
	mor = bus_space_read_1(vh->vh_iot, vh->vh_ioh_vga, VGA_MISC_DATAR);
	vh->vh_mono = !(mor & 1);

	if (bus_space_map(vh->vh_iot, (vh->vh_mono ? 0x3b0 : 0x3d0), 0x10, 0,
	    &vh->vh_ioh_6845))
		panic("vga_raster_init: couldn't map 6845 io");

	if (bus_space_map(vh->vh_memt, 0xa0000, 0x20000, 0, &vh->vh_allmemh))
		panic("vga_raster_init: couldn't map memory");

	if (bus_space_subregion(vh->vh_memt, vh->vh_allmemh, 0, 0x10000,
	    &vh->vh_memh))
		panic("vga_raster_init: mem subrange failed");

	vc->nscreens = 0;
	LIST_INIT(&vc->screens);
	vc->active = NULL;
	vc->currenttype = vh->vh_mono ? &vga_25lscreen_mono : &vga_25lscreen;
	callout_init(&vc->vc_switch_callout, 0);

	wsfont_init();
	vc->nfonts = 1;
	LIST_INIT(&vc->vc_fontlist);
	vf = &vga_console_fontset_ascii;
	if (vga_no_builtinfont) {
		struct wsdisplay_font *wf;
		int cookie;

		/* prefer 8x16 pixel font */
		cookie = wsfont_find(NULL, 8, 16, 0, WSDISPLAY_FONTORDER_L2R,
		    0, WSFONT_FIND_BITMAP);
		if (cookie == -1)
			cookie = wsfont_find(NULL, 0, 0, 0,
			    WSDISPLAY_FONTORDER_L2R, WSDISPLAY_FONTORDER_L2R,
			    WSFONT_FIND_BITMAP);
		if (cookie == -1 || wsfont_lock(cookie, &wf))
			panic("vga_raster_init: can't load console font");
		vf->font = wf;
	} else {
		vga_load_builtinfont(vh, builtinfont_data, 0, 256);
		vf->font = &builtinfont;
	}
	LIST_INSERT_HEAD(&vc->vc_fontlist, vf, next);
}

static void
vga_raster_init_screen(struct vga_config *vc, struct vgascreen *scr,
    const struct wsscreen_descr *type, int existing, long *attrp)
{
	int cpos;
	int res;
	struct vga_handle *vh;

	scr->cfg = vc;
	scr->hdl = &vc->hdl;
	scr->type = type;
	scr->mindispoffset = 0;
	scr->maxdispoffset = scr->dispoffset +
	    type->nrows * type->ncols * type->fontheight;
	vh = &vc->hdl;

	LIST_INIT(&scr->fontset);
	vga_raster_setup_font(vc, scr);

	if (existing) {
		int i;

		cpos = vga_6845_read(vh, cursorh) << 8;
		cpos |= vga_6845_read(vh, cursorl);

		/* make sure we have a valid cursor position */
		if (cpos < 0 || cpos >= type->nrows * type->ncols)
			cpos = 0;

		scr->dispoffset = vga_6845_read(vh, startadrh) << 9;
		scr->dispoffset |= vga_6845_read(vh, startadrl) << 1;

		/* make sure we have a valid memory offset */
		if (scr->dispoffset < scr->mindispoffset ||
		    scr->dispoffset > scr->maxdispoffset)
			scr->dispoffset = scr->mindispoffset;

		scr->mem = boot_scrmem;
		scr->active = 1;

		/* Save the current screen to memory. XXXBJY assume 80x25 */
		for (i = 0; i < 80 * 25; i++) {
			scr->mem[i].ch = bus_space_read_1(vh->vh_memt,
			    vh->vh_allmemh, 0x18000 + i * 2);
			scr->mem[i].attr = bus_space_read_1(vh->vh_memt,
			    vh->vh_allmemh, 0x18000 + i * 2 + 1);
			scr->mem[i].enc = scr->encoding;
		}

		vga_raster_setscreentype(vc, type);

		/* Clear the entire screen. */
		vga_gdc_write(vh, mode, 0x02);
		bus_space_set_region_4(vh->vh_memt, vh->vh_allmemh, 0, 0,
		    0x4000);

		vga_restore_screen(scr, type, scr->mem);
	} else {
		cpos = 0;
		scr->dispoffset = scr->mindispoffset;
		scr->mem = NULL;
		scr->active = 0;
	}

	scr->cursorrow = cpos / type->ncols;
	scr->cursorcol = cpos % type->ncols;
	vga_raster_cursor_init(scr, existing);

#ifdef __alpha__
	if (!vc->hdl.vh_mono)
		/*
		 * DEC firmware uses a blue background.
		 */
		res = vga_raster_allocattr(scr, WSCOL_WHITE, WSCOL_BLUE,
		    WSATTR_WSCOLORS, attrp);
	else
#endif
	res = vga_raster_allocattr(scr, 0, 0, 0, attrp);
#ifdef DIAGNOSTIC
	if (res)
		panic("vga_raster_init_screen: attribute botch");
#endif

	vc->nscreens++;
	LIST_INSERT_HEAD(&vc->screens, scr, next);
}

void
vga_common_attach(struct vga_softc *sc, bus_space_tag_t iot,
    bus_space_tag_t memt, int type, int quirks,
    const struct vga_funcs *vf)
{
	int console;
	struct vga_config *vc;
	struct wsemuldisplaydev_attach_args aa;

	console = vga_is_console(iot, type);

	if (console) {
		vc = &vga_console_vc;
		vga_console_attached = 1;
	} else {
		vc = malloc(sizeof(struct vga_config), M_DEVBUF, M_WAITOK);
		vga_raster_init(vc, iot, memt);
	}

	vc->vc_type = type;
	vc->vc_funcs = vf;

	sc->sc_vc = vc;
	vc->softc = sc;

	aa.console = console;
	aa.scrdata = (vc->hdl.vh_mono ? &vga_screenlist_mono : &vga_screenlist);
	aa.accessops = &vga_raster_accessops;
	aa.accesscookie = vc;

	config_found(sc->sc_dev, &aa, wsemuldisplaydevprint);
}

int
vga_cndetach(void)
{
	struct vga_config *vc;
	struct vga_handle *vh;

	vc = &vga_console_vc;
	vh = &vc->hdl;

	if (vgaconsole) {
		wsdisplay_cndetach();

		bus_space_unmap(vh->vh_iot, vh->vh_ioh_vga, 0x10);
		bus_space_unmap(vh->vh_iot, vh->vh_ioh_6845, 0x10);
		bus_space_unmap(vh->vh_memt, vh->vh_allmemh, 0x20000);

		vga_console_attached = 0;
		vgaconsole = 0;

		return 1;
	}

	return 0;
}

int
vga_is_console(bus_space_tag_t iot, int type)
{
	if (vgaconsole &&
	    !vga_console_attached &&
	    iot == vga_console_vc.hdl.vh_iot &&
	    (vga_console_type == -1 || (type == vga_console_type)))
		return (1);
	return (0);
}

static int
vga_get_video(struct vga_config *vc)
{

	return (vga_ts_read(&vc->hdl, mode) & VGA_TS_MODE_BLANK) == 0;
}

static void
vga_set_video(struct vga_config *vc, int state)
{
	int val;

	vga_ts_write(&vc->hdl, syncreset, 0x01);
	if (state) {					/* unblank screen */
		val = vga_ts_read(&vc->hdl, mode);
		vga_ts_write(&vc->hdl, mode, val & ~VGA_TS_MODE_BLANK);
#ifndef VGA_NO_VBLANK
		val = vga_6845_read(&vc->hdl, mode);
		vga_6845_write(&vc->hdl, mode, val | 0x80);
#endif
	} else {					/* blank screen */
		val = vga_ts_read(&vc->hdl, mode);
		vga_ts_write(&vc->hdl, mode, val | VGA_TS_MODE_BLANK);
#ifndef VGA_NO_VBLANK
		val = vga_6845_read(&vc->hdl, mode);
		vga_6845_write(&vc->hdl, mode, val & ~0x80);
#endif
	}
	vga_ts_write(&vc->hdl, syncreset, 0x03);
}

int
vga_raster_ioctl(void *v, void *vs, u_long cmd, void *data, int flag,
	struct lwp *l)
{
	struct vga_config *vc = v;
	const struct vga_funcs *vf = vc->vc_funcs;

	switch (cmd) {
	case WSDISPLAYIO_GTYPE:
		*(int *)data = vc->vc_type;
		return 0;

	case WSDISPLAYIO_GINFO: {
		struct wsdisplay_fbinfo *fbi = data;
		const struct wsscreen_descr *wd = vc->currenttype;
		const struct videomode *vm = wd->modecookie;
		fbi->width = vm->hdisplay;
		fbi->height = vm->vdisplay;
		fbi->depth = 24;	/* xxx: ? */
		fbi->cmsize = 256;	/* xxx: from palette */
		return 0;
	}

	case WSDISPLAYIO_GVIDEO:
		*(int *)data = (vga_get_video(vc) ?
		    WSDISPLAYIO_VIDEO_ON : WSDISPLAYIO_VIDEO_OFF);
		return 0;

	case WSDISPLAYIO_SVIDEO:
		vga_set_video(vc, *(int *)data == WSDISPLAYIO_VIDEO_ON);
		return 0;

	case WSDISPLAYIO_GETCMAP:
	case WSDISPLAYIO_PUTCMAP:
	case WSDISPLAYIO_GCURPOS:
	case WSDISPLAYIO_SCURPOS:
	case WSDISPLAYIO_GCURMAX:
	case WSDISPLAYIO_GCURSOR:
	case WSDISPLAYIO_SCURSOR:
#ifdef DIAGNOSTIC
		printf("%s: 0x%lx unsupported\n", __func__, cmd);
#endif
		/* NONE of these operations are by the generic VGA driver. */
		return EPASSTHROUGH;
	}

	if (vc->vc_funcs == NULL) {
#ifdef DIAGNOSTIC
		printf("%s: no vc_funcs\n", __func__);
#endif
		return EPASSTHROUGH;
	}

	if (vf->vf_ioctl == NULL) {
#ifdef DIAGNOSTIC
		printf("%s: no vf_ioctl\n", __func__);
#endif
		return EPASSTHROUGH;
	}

	return ((*vf->vf_ioctl)(v, cmd, data, flag, l));
}

static paddr_t
vga_raster_mmap(void *v, void *vs, off_t offset, int prot)
{
	struct vga_config *vc = v;
	const struct vga_funcs *vf = vc->vc_funcs;

	if (vc->vc_funcs == NULL)
		return (-1);

	if (vf->vf_mmap == NULL)
		return (-1);

	return ((*vf->vf_mmap)(v, offset, prot));
}

static int
vga_raster_alloc_screen(void *v, const struct wsscreen_descr *type,
    void **cookiep, int *curxp, int *curyp, long *defattrp)
{
	struct vga_config *vc = v;
	struct vgascreen *scr;

	if (vc->nscreens == 1) {
		vc->screens.lh_first->mem = boot_scrmem;
	}

	scr = malloc(sizeof(struct vgascreen), M_DEVBUF, M_WAITOK);
	vga_raster_init_screen(vc, scr, type, vc->nscreens == 0, defattrp);

	if (vc->nscreens == 1) {
		scr->active = 1;
		vc->active = scr;
		vc->currenttype = type;
	} else {
		scr->mem = malloc(sizeof(struct vga_scrmem) *
		    type->ncols * type->nrows, M_DEVBUF, M_WAITOK);
		vga_raster_eraserows(scr, 0, type->nrows, *defattrp);
	}

	*cookiep = scr;
	*curxp = scr->cursorcol;
	*curyp = scr->cursorrow;

	return (0);
}

static void
vga_raster_free_screen(void *v, void *cookie)
{
	struct vgascreen *vs = cookie;
	struct vga_config *vc = vs->cfg;

	LIST_REMOVE(vs, next);
	vc->nscreens--;
	if (vs != &vga_console_screen)
		free(vs, M_DEVBUF);
	else
		panic("vga_raster_free_screen: console");

	if (vc->active == vs)
		vc->active = 0;
}

static int
vga_raster_show_screen(void *v, void *cookie, int waitok,
    void (*cb)(void *, int, int), void *cbarg)
{
	struct vgascreen *scr = cookie, *oldscr;
	struct vga_config *vc = scr->cfg;

	oldscr = vc->active; /* can be NULL! */
	if (scr == oldscr) {
		return (0);
	}

	vc->wantedscreen = cookie;
	vc->switchcb = cb;
	vc->switchcbarg = cbarg;
	if (cb) {
		callout_reset(&vc->vc_switch_callout, 0,
		    (void(*)(void *))vga_switch_screen, vc);
		return (EAGAIN);
	}

	vga_switch_screen(vc);
	return (0);
}

static void
vga_switch_screen(struct vga_config *vc)
{
	struct vgascreen *scr, *oldscr;
	struct vga_handle *vh = &vc->hdl;
	const struct wsscreen_descr *type;

	scr = vc->wantedscreen;
	if (!scr) {
		printf("vga_switch_screen: disappeared\n");
		(*vc->switchcb)(vc->switchcbarg, EIO, 0);
		return;
	}
	type = scr->type;
	oldscr = vc->active; /* can be NULL! */
#ifdef DIAGNOSTIC
	if (oldscr) {
		if (!oldscr->active)
			panic("vga_raster_show_screen: not active");
		if (oldscr->type != vc->currenttype)
			panic("vga_raster_show_screen: bad type");
	}
#endif
	if (scr == oldscr) {
		return;
	}
#ifdef DIAGNOSTIC
	if (scr->active)
		panic("vga_raster_show_screen: active");
#endif

	if (oldscr)
		oldscr->active = 0;

	if (vc->currenttype != type) {
		vga_raster_setscreentype(vc, type);
		vc->currenttype = type;
	}

	scr->dispoffset = scr->mindispoffset;

	if (!oldscr || (scr->dispoffset != oldscr->dispoffset)) {
		vga_6845_write(vh, startadrh, scr->dispoffset >> 8);
		vga_6845_write(vh, startadrl, scr->dispoffset);
	}

	/* Clear the entire screen. */
	vga_gdc_write(vh, mode, 0x02);
	bus_space_set_region_4(vh->vh_memt, vh->vh_allmemh, 0, 0, 0x2000);

	scr->active = 1;
	vga_restore_screen(scr, type, scr->mem);

	vc->active = scr;

	vga_raster_cursor(scr, scr->cursoron, scr->cursorrow, scr->cursorcol);

	vc->wantedscreen = 0;
	if (vc->switchcb)
		(*vc->switchcb)(vc->switchcbarg, 0, 0);
}

static int
vga_raster_load_font(void *v, void *id,
    struct wsdisplay_font *data)
{
	/* XXX */
	printf("vga_raster_load_font: called\n");

	return (0);
}

static void
vga_raster_setup_font(struct vga_config *vc, struct vgascreen *scr)
{
	struct vga_raster_font *vf;
	struct wsdisplay_font *wf;
	int cookie;

	LIST_FOREACH(vf, &vc->vc_fontlist, next) {
		if (wsfont_matches(vf->font, 0, 0, scr->type->fontheight, 0,
		    WSFONT_FIND_BITMAP)) {
			scr->encoding = vf->font->encoding;
			LIST_INSERT_HEAD(&scr->fontset, vf, next);
			return;
		}
	}

	cookie = wsfont_find(NULL, 0, scr->type->fontheight, 0,
	    WSDISPLAY_FONTORDER_L2R, 0, WSFONT_FIND_BITMAP);
	if (cookie == -1)
		return;

	if (wsfont_lock(cookie, &wf))
		return;

	vf = malloc(sizeof(struct vga_raster_font), M_DEVBUF, M_NOWAIT);
	if (!vf) {
		wsfont_unlock(cookie);
		return;
	}

	vf->font = wf;
	scr->encoding = vf->font->encoding;
	LIST_INSERT_HEAD(&scr->fontset, vf, next);
}

static void
vga_setup_regs(struct videomode *mode, struct vga_moderegs *regs)
{
	int i;
	int depth = 4;			/* XXXBJY hardcoded for now */
	const u_int8_t palette[] = {
		0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x14, 0x07,
		0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f
	};

	/*
	 * Compute hsync and vsync polarity.
	 */
	if ((mode->flags & (VID_PHSYNC | VID_NHSYNC))
	    && (mode->flags & (VID_PVSYNC | VID_NVSYNC))) {
	    	regs->miscout = 0x23;
		if (mode->flags & VID_NHSYNC)
			regs->miscout |= 0x40;
		if (mode->flags & VID_NVSYNC)
			regs->miscout |= 0x80;
	} else {
		if (mode->flags & VID_DBLSCAN)
			mode->vdisplay *= 2;
		if (mode->vdisplay < 400)
			regs->miscout = 0xa3;
		else if (mode->vdisplay < 480)
			regs->miscout = 0x63;
		else if (mode->vdisplay < 768)
			regs->miscout = 0xe3;
		else
			regs->miscout = 0x23;
	}

	/*
	 * Time sequencer
	 */
	if (depth == 4)
		regs->ts[0] = 0x02;
	else
		regs->ts[0] = 0x00;
	if (mode->flags & VID_CLKDIV2)
		regs->ts[1] = 0x09;
	else
		regs->ts[1] = 0x01;
	regs->ts[2] = 0x0f;
	regs->ts[3] = 0x00;
	if (depth < 8)
		regs->ts[4] = 0x06;
	else
		regs->ts[4] = 0x0e;

	/*
	 * CRTC controller
	 */
	regs->crtc[0] = (mode->htotal >> 3) - 5;
	regs->crtc[1] = (mode->hdisplay >> 3) - 1;
	regs->crtc[2] = (mode->hsync_start >> 3) - 1;
	regs->crtc[3] = (((mode->hsync_end >> 3) - 1) & 0x1f) | 0x80;
	regs->crtc[4] = mode->hsync_start >> 3;
	regs->crtc[5] = ((((mode->hsync_end >> 3) - 1) & 0x20) << 2)
	    | (((mode->hsync_end >> 3)) & 0x1f);
	regs->crtc[6] = (mode->vtotal - 2) & 0xff;
	regs->crtc[7] = (((mode->vtotal - 2) & 0x100) >> 8)
	    | (((mode->vdisplay - 1) & 0x100) >> 7)
	    | ((mode->vsync_start & 0x100) >> 6)
	    | (((mode->vsync_start - 1) & 0x100) >> 5)
	    | 0x10
	    | (((mode->vtotal - 2) & 0x200) >> 4)
	    | (((mode->vdisplay - 1) & 0x200) >> 3)
	    | ((mode->vsync_start & 0x200) >> 2);
	regs->crtc[8] = 0x00;
	regs->crtc[9] = (((mode->vsync_start - 1) & 0x200) >> 4) | 0x40;
	if (mode->flags & VID_DBLSCAN)
		regs->crtc[9] |= 0x80;
	regs->crtc[10] = 0x00;
	regs->crtc[11] = 0x00;
	regs->crtc[12] = 0x00;
	regs->crtc[13] = 0x00;
	regs->crtc[14] = 0x00;
	regs->crtc[15] = 0x00;
	regs->crtc[16] = mode->vsync_start & 0xff;
	regs->crtc[17] = (mode->vsync_end & 0x0f) | 0x20;
	regs->crtc[18] = (mode->vdisplay - 1) & 0xff;
	regs->crtc[19] = mode->hdisplay >> 4;	/* XXXBJY */
	regs->crtc[20] = 0x00;
	regs->crtc[21] = (mode->vsync_start - 1) & 0xff;
	regs->crtc[22] = (mode->vsync_end - 1) & 0xff;
	if (depth < 8)
		regs->crtc[23] = 0xe3;
	else
		regs->crtc[23] = 0xc3;
	regs->crtc[24] = 0xff;

	/*
	 * Graphics display controller
	 */
	regs->gdc[0] = 0x00;
	regs->gdc[1] = 0x00;
	regs->gdc[2] = 0x00;
	regs->gdc[3] = 0x00;
	regs->gdc[4] = 0x00;
	if (depth == 4)
		regs->gdc[5] = 0x02;
	else
		regs->gdc[5] = 0x40;
	regs->gdc[6] = 0x01;
	regs->gdc[7] = 0x0f;
	regs->gdc[8] = 0xff;

	/*
	 * Attribute controller
	 */
	/* Set palette registers. */
	for (i = 0; i < 16; i++)
		regs->atc[i] = palette[i];
	if (depth == 4)
		regs->atc[16] = 0x01;	/* XXXBJY was 0x81 in XFree86 */
	else
		regs->atc[16] = 0x41;
	regs->atc[17] = 0x00;		/* XXXBJY just a guess */
	regs->atc[18] = 0x0f;
	regs->atc[19] = 0x00;
	regs->atc[20] = 0x00;
}

static void
vga_set_mode(struct vga_handle *vh, struct vga_moderegs *regs)
{
	int i;

	/* Disable display. */
	vga_ts_write(vh, mode, vga_ts_read(vh, mode) | VGA_TS_MODE_BLANK);

	/* Write misc output register. */
	bus_space_write_1(vh->vh_iot, vh->vh_ioh_vga, VGA_MISC_DATAW,
	    regs->miscout);

	/* Set synchronous reset. */
	vga_ts_write(vh, syncreset, 0x01);
	vga_ts_write(vh, mode, regs->ts[1] | VGA_TS_MODE_BLANK);
	for (i = 2; i < VGA_TS_NREGS; i++)
		_vga_ts_write(vh, i, regs->ts[i]);
	/* Clear synchronous reset. */
	vga_ts_write(vh, syncreset, 0x03);

	/* Unprotect CRTC registers 0-7. */
	vga_6845_write(vh, vsynce, vga_6845_read(vh, vsynce) & ~0x80);
	/* Write CRTC registers. */
	for (i = 0; i < MC6845_NREGS; i++)
		_vga_6845_write(vh, i, regs->crtc[i]);

	/* Write graphics display registers. */
	for (i = 0; i < VGA_GDC_NREGS; i++)
		_vga_gdc_write(vh, i, regs->gdc[i]);

	/* Write attribute controller registers. */
	for (i = 0; i < VGA_ATC_NREGS; i++)
		_vga_attr_write(vh, i, regs->atc[i]);

	/* Enable display. */
	vga_ts_write(vh, mode, vga_ts_read(vh, mode) & ~VGA_TS_MODE_BLANK);
}

static void
vga_raster_cursor_init(struct vgascreen *scr, int existing)
{
	int off;

	if (existing) {
		/*
		 * This is the first screen. At this point, scr->active is
		 * false, so we can't use vga_raster_cursor() to do this.
		 */
		off = (scr->cursorrow * scr->type->ncols + scr->cursorcol) +
		    scr->dispoffset / 8;

		scr->cursortmp = scr->mem[off];
		vga_raster_putchar(scr, scr->cursorrow, scr->cursorcol,
		    scr->cursortmp.ch, scr->cursortmp.attr ^ 0x77);
	} else {
		scr->cursortmp.ch = 0;
		scr->cursortmp.attr = 0;
		scr->cursortmp.second = 0;
		scr->cursortmp.enc = scr->encoding;
	}

	scr->cursoron = 1;
}

static void
vga_raster_cursor(void *id, int on, int row, int col)
{
	struct vgascreen *scr = id;
	int off, tmp;

	/* Remove old cursor image */
	if (scr->cursoron) {
		off = scr->cursorrow * scr->type->ncols + scr->cursorcol;
		if (scr->active) {
			tmp = scr->encoding;
			scr->encoding = scr->cursortmp.enc;
			if (scr->cursortmp.second)
				vga_raster_putchar(id, scr->cursorrow,
				    scr->cursorcol - 1, scr->cursortmp.ch,
				    scr->cursortmp.attr);
			else
				vga_raster_putchar(id, scr->cursorrow,
				    scr->cursorcol, scr->cursortmp.ch,
				    scr->cursortmp.attr);
			scr->encoding = tmp;
		}
	}

	scr->cursorrow = row;
	scr->cursorcol = col;

	if ((scr->cursoron = on) == 0)
		return;

	off = scr->cursorrow * scr->type->ncols + scr->cursorcol;
	scr->cursortmp = scr->mem[off];
	if (scr->active) {
		tmp = scr->encoding;
		scr->encoding = scr->cursortmp.enc;
		if (scr->cursortmp.second)
			vga_raster_putchar(id, scr->cursorrow,
			    scr->cursorcol - 1, scr->cursortmp.ch,
			    scr->cursortmp.attr ^ 0x77);
		else
			vga_raster_putchar(id, scr->cursorrow,
			    scr->cursorcol, scr->cursortmp.ch,
			    scr->cursortmp.attr ^ 0x77);
		scr->encoding = tmp;
	}
}

static int
vga_raster_mapchar(void *id, int uni, u_int *index)
{
	struct vgascreen *scr = id;

	if (scr->encoding == WSDISPLAY_FONTENC_IBM)
		return pcdisplay_mapchar(id, uni, index);
	else {
		*index = uni;
		return 5;
	}
}

static void
vga_raster_putchar(void *id, int row, int col, u_int c, long attr)
{
	struct vgascreen *scr = id;
	size_t off;
	struct vga_raster_font *fs;
	u_int tmp_ch;

	off = row * scr->type->ncols + col;

	if (__predict_false(off >= (scr->type->ncols * scr->type->nrows)))
		return;

	LIST_FOREACH(fs, &scr->fontset, next) {
		if ((scr->encoding == fs->font->encoding) &&
		    (c >= fs->font->firstchar) &&
		    (c < fs->font->firstchar + fs->font->numchars) &&
		    (scr->type->fontheight == fs->font->fontheight)) {
			if (scr->active) {
				tmp_ch = c - fs->font->firstchar;
				_vga_raster_putchar(scr, row, col, tmp_ch,
				    attr, fs);
			}

			scr->mem[off].ch = c;
			scr->mem[off].attr = attr;
			scr->mem[off].second = 0;
			scr->mem[off].enc = fs->font->encoding;

			if (fs->font->stride == 2) {
				scr->mem[off + 1].ch = c;
				scr->mem[off + 1].attr = attr;
				scr->mem[off + 1].second = 1;
				scr->mem[off + 1].enc = fs->font->encoding;
			}

			return;
		}
	}

	/*
	 * No match found.
	 */
	if (scr->active)
		/*
		 * Put a single width space character no matter what the
		 * actual width of the character is.
		 */
		_vga_raster_putchar(scr, row, col, ' ', attr,
		    &vga_console_fontset_ascii);
	scr->mem[off].ch = c;
	scr->mem[off].attr = attr;
	scr->mem[off].second = 0;
	scr->mem[off].enc = scr->encoding;
}

static void
_vga_raster_putchar(void *id, int row, int col, u_int c, long attr,
    struct vga_raster_font *fs)
{
	struct vgascreen *scr = id;
	struct vga_handle *vh = scr->hdl;
	bus_space_tag_t memt = vh->vh_memt;
	bus_space_handle_t memh = vh->vh_memh;
	int i;
	int rasoff, rasoff2;
	int fheight = scr->type->fontheight;
	volatile u_int8_t pattern;
	u_int8_t fgcolor, bgcolor;

	rasoff = scr->dispoffset + row * scr->type->ncols * fheight + col;
	rasoff2 = rasoff;

#if 0
	bgcolor = bgansitopc[attr >> 4];
	fgcolor = fgansitopc[attr & 0x0f];
#else
	bgcolor = ((attr >> 4) & 0x0f);
	fgcolor = attr & 0x0f;
#endif

	if (fs->font->stride == 1) {
		/* Paint background. */
		vga_gdc_write(vh, mode, 0x02);
		for (i = 0; i < fheight; i++) {
			bus_space_write_1(memt, memh, rasoff, bgcolor);
			rasoff += scr->type->ncols;
		}

		/* Draw a single width character. */
		vga_gdc_write(vh, mode, 0x03);
		vga_gdc_write(vh, setres, fgcolor);
		for (i = 0; i < fheight; i++) {
			pattern = ((u_int8_t *)fs->font->data)[c * fheight + i];
			/* When pattern is 0, skip output for speed-up. */
			if (pattern != 0) {
				bus_space_read_1(memt, memh, rasoff2);
				bus_space_write_1(memt, memh, rasoff2, pattern);
			}
			rasoff2 += scr->type->ncols;
		}
	} else if (fs->font->stride == 2) {
		/* Paint background. */
		vga_gdc_write(vh, mode, 0x02);
		for (i = 0; i < fheight; i++) {
			bus_space_write_1(memt, memh, rasoff, bgcolor);
			bus_space_write_1(memt, memh, rasoff + 1, bgcolor);
			rasoff += scr->type->ncols;
		}

		/* Draw a double width character. */
		vga_gdc_write(vh, mode, 0x03);
		vga_gdc_write(vh, setres, fgcolor);
		for (i = 0; i < fheight; i++) {
			pattern = ((u_int8_t *)fs->font->data)
			    [(c * fheight + i) * 2];
			if (pattern != 0) {
				bus_space_read_1(memt, memh, rasoff2);
				bus_space_write_1(memt, memh, rasoff2, pattern);
			}
			pattern = ((u_int8_t *)fs->font->data)
			    [(c * fheight + i) * 2 + 1];
			if (pattern != 0) {
				rasoff2++;
				bus_space_read_1(memt, memh, rasoff2);
				bus_space_write_1(memt, memh, rasoff2, pattern);
				rasoff2--;
			}
			rasoff2 += scr->type->ncols;
		}
	}
}

static void
vga_raster_copycols(void *id, int row, int srccol, int dstcol, int ncols)
{
	struct vgascreen *scr = id;
	struct vga_handle *vh = scr->hdl;
	bus_space_tag_t memt = vh->vh_memt;
	bus_space_handle_t memh = vh->vh_memh;
	bus_size_t srcoff, dstoff;
	bus_size_t rassrcoff, rasdstoff;
	int i;
	int fheight = scr->type->fontheight;

	srcoff = row * scr->type->ncols + srccol;
	dstoff = row * scr->type->ncols + dstcol;
	rassrcoff = scr->dispoffset + row * scr->type->ncols * fheight + srccol;
	rasdstoff = scr->dispoffset + row * scr->type->ncols * fheight + dstcol;

	memcpy(&scr->mem[dstoff], &scr->mem[srcoff],
	    ncols * sizeof(struct vga_scrmem));

	vga_gdc_write(vh, mode, 0x01);
	if (scr->active) {
		for (i = 0; i < fheight; i++) {
			bus_space_copy_region_1(memt, memh,
			    rassrcoff + i * scr->type->ncols, memh,
			    rasdstoff + i * scr->type->ncols, ncols);
		}
	}
}

static void
vga_raster_erasecols(void *id, int row, int startcol, int ncols, long fillattr)
{
	struct vgascreen *scr = id;
	int i;

	if (scr->active == 0)
		return;

	for (i = startcol; i < startcol + ncols; i++)
		vga_raster_putchar(id, row, i, ' ', fillattr);
}

static void
vga_raster_copyrows(void *id, int srcrow, int dstrow, int nrows)
{
	struct vgascreen *scr = id;
	struct vga_handle *vh = scr->hdl;
	bus_space_tag_t memt = vh->vh_memt;
	bus_space_handle_t memh = vh->vh_memh;
	int ncols;
	bus_size_t srcoff, dstoff;
	bus_size_t rassrcoff, rasdstoff;
	int fheight;

	ncols = scr->type->ncols;
	fheight = scr->type->fontheight;

	srcoff = srcrow * ncols;
	dstoff = dstrow * ncols;
	rassrcoff = srcoff * fheight;
	rasdstoff = dstoff * fheight;

	if (scr->active) {
		vga_gdc_write(vh, mode, 0x01);
		if (dstrow == 0 && (srcrow + nrows == scr->type->nrows)) {
			int cursoron = scr->cursoron;

			if (cursoron)
				/* Disable cursor. */
				vga_raster_cursor(scr, 0,
				    scr->cursorrow, scr->cursorcol);

			/* scroll up whole screen */
			if ((scr->dispoffset + srcrow * ncols * fheight)
			    <= scr->maxdispoffset)
				scr->dispoffset += srcrow * ncols * fheight;
			else {
				bus_space_copy_region_1(memt, memh,
				    scr->dispoffset + rassrcoff,
				    memh, scr->mindispoffset,
				    nrows * ncols * fheight);
				scr->dispoffset = scr->mindispoffset;
			}
			vga_6845_write(vh, startadrh, scr->dispoffset >> 8);
			vga_6845_write(vh, startadrl, scr->dispoffset);

			if (cursoron)
				/* Enable cursor. */
				vga_raster_cursor(scr, 1, scr->cursorrow,
				    scr->cursorcol);
		} else
			bus_space_copy_region_1(memt, memh,
			    scr->dispoffset + rassrcoff, memh,
			    scr->dispoffset + rasdstoff,
			    nrows * ncols * fheight);
	}
	memcpy(&scr->mem[dstoff], &scr->mem[srcoff],
	    nrows * ncols * sizeof(struct vga_scrmem));
}

static void
vga_raster_eraserows(void *id, int startrow, int nrows, long fillattr)
{
	struct vgascreen *scr = id;
	struct vga_handle *vh = scr->hdl;
	bus_space_tag_t memt = vh->vh_memt;
	bus_space_handle_t memh = vh->vh_memh;
	bus_size_t off, count;
	bus_size_t rasoff, rascount;
	int i;

	off = startrow * scr->type->ncols;
	count = nrows * scr->type->ncols;
	rasoff = off * scr->type->fontheight;
	rascount = count * scr->type->fontheight;

	if (scr->active) {
		u_int8_t bgcolor = (fillattr >> 4) & 0x0F;

		/* Paint background. */
		vga_gdc_write(vh, mode, 0x02);
		if (scr->type->ncols % 4 == 0) {
			u_int32_t fill = bgcolor | (bgcolor << 8) |
			    (bgcolor << 16) | (bgcolor << 24);
			/* We can speed up I/O */
			for (i = rasoff; i < rasoff + rascount; i += 4)
				bus_space_write_4(memt, memh,
				    scr->dispoffset + i, fill);
		} else {
			u_int16_t fill = bgcolor | (bgcolor << 8);
			for (i = rasoff; i < rasoff + rascount; i += 2)
				bus_space_write_2(memt, memh,
				    scr->dispoffset + i, fill);
		}
	}
	for (i = 0; i < count; i++) {
		scr->mem[off + i].ch = ' ';
		scr->mem[off + i].attr = fillattr;
		scr->mem[off + i].second = 0;
		scr->mem[off + i].enc = scr->encoding;
	}
}

static int
vga_raster_allocattr(void *id, int fg, int bg, int flags, long *attrp)
{
	struct vgascreen *scr = id;
	struct vga_config *vc = scr->cfg;

	if (__predict_false((unsigned int)fg >= sizeof(fgansitopc) || 
	    (unsigned int)bg >= sizeof(bgansitopc)))
	    	return (EINVAL);

	if (vc->hdl.vh_mono) {
		if (flags & WSATTR_WSCOLORS)
			return (EINVAL);
		if (flags & WSATTR_REVERSE)
			*attrp = 0x70;
		else
			*attrp = 0x07;
		if (flags & WSATTR_UNDERLINE)
			*attrp |= FG_UNDERLINE;
		if (flags & WSATTR_HILIT)
			*attrp |= FG_INTENSE;
	} else {
		if (flags & (WSATTR_UNDERLINE | WSATTR_REVERSE))
			return (EINVAL);
		if (flags & WSATTR_WSCOLORS)
			*attrp = fgansitopc[fg] | bgansitopc[bg];
		else
			*attrp = 7;
		if (flags & WSATTR_HILIT)
			*attrp += 8;
	}
	if (flags & WSATTR_BLINK)
		*attrp |= FG_BLINK;
	return (0);
}

static void
vga_restore_screen(struct vgascreen *scr,
    const struct wsscreen_descr *type, struct vga_scrmem *mem)
{
	int i, j, off, tmp;

	tmp = scr->encoding;
	for (i = 0; i < type->nrows; i++) {
		for (j = 0; j < type->ncols; j++) {
			off = i * type->ncols + j;
			if (mem[off].second != 1) {
				scr->encoding = mem[off].enc;
				vga_raster_putchar(scr, i, j, mem[off].ch,
				    mem[off].attr);
			}
		}
	}
	scr->encoding = tmp;
}

static void
vga_raster_setscreentype(struct vga_config *vc,
    const struct wsscreen_descr *type)
{
	struct vga_handle *vh = &vc->hdl;
	struct vga_moderegs moderegs;

	vga_setup_regs((struct videomode *)type->modecookie, &moderegs);
	vga_set_mode(vh, &moderegs);
}

#ifdef WSDISPLAY_CUSTOM_OUTPUT
static void
vga_raster_replaceattr(void *id, long oldattr, long newattr)
{
	struct vgascreen *scr = id;
	const struct wsscreen_descr *type = scr->type;
	int off;

	for (off = 0; off < type->nrows * type->ncols; off++) {
		if (scr->mem[off].attr == oldattr)
			scr->mem[off].attr = newattr;
	}

	/* Repaint the whole screen, if needed */
	if (scr->active)
		vga_restore_screen(scr, type, scr->mem);
}
#endif /* WSDISPLAY_CUSTOM_OUTPUT */

void
vga_resume(struct vga_softc *sc)
{
#ifdef VGA_RESET_ON_RESUME
	vga_initregs(&sc->sc_vc->hdl);
#endif
}
