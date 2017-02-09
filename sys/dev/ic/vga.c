/* $NetBSD: vga.c,v 1.115 2015/03/01 07:05:59 mlelstv Exp $ */

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
__KERNEL_RCSID(0, "$NetBSD: vga.c,v 1.115 2015/03/01 07:05:59 mlelstv Exp $");

#include "opt_vga.h"
/* for WSCONS_SUPPORT_PCVTFONTS */
#include "opt_wsdisplay_compat.h"
/* for WSDISPLAY_CUSTOM_BORDER */
#include "opt_wsdisplay_border.h"
/* for WSDISPLAY_CUSTOM_OUTPUT */
#include "opt_wsmsgattrs.h"

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

#include <dev/wscons/wsdisplayvar.h>
#include <dev/wscons/wsconsio.h>
#include <dev/wscons/unicode.h>
#include <dev/wsfont/wsfont.h>

#include <dev/ic/pcdisplay.h>

int vga_no_builtinfont = 0;

static struct wsdisplay_font _vga_builtinfont = {
	"builtin",			/* typeface name */
	0,				/* firstchar */
	256,				/* numbers */
	WSDISPLAY_FONTENC_IBM,		/* encoding */
	8,				/* width */
	16,				/* height */
	1,				/* stride */
	WSDISPLAY_FONTORDER_L2R,	/* bit order */
	0,				/* byte order */
	NULL				/* data */
};

struct egavga_font {
	struct wsdisplay_font *wsfont;
	int cookie; /* wsfont handle, -1 invalid */
	int slot; /* in adapter RAM */
	int usecount;
	TAILQ_ENTRY(egavga_font) next; /* LRU queue */
};

static struct egavga_font vga_builtinfont = {
	.wsfont = &_vga_builtinfont,
	.cookie = -1,
	.slot = 0,
};

#ifdef VGA_CONSOLE_SCREENTYPE
static struct egavga_font vga_consolefont;
#endif

struct vgascreen {
	struct pcdisplayscreen pcs;

	LIST_ENTRY(vgascreen) next;

	struct vga_config *cfg;

	/* videostate */
	struct egavga_font *fontset1, *fontset2;
	/* font data */

	int mindispoffset, maxdispoffset;
	int vga_rollover;
	int visibleoffset;
};

static int vgaconsole, vga_console_type, vga_console_attached;
static struct vgascreen vga_console_screen;
static struct vga_config vga_console_vc;

static struct egavga_font *egavga_getfont(struct vga_config *, struct vgascreen *,
				   const char *, int);
static void egavga_unreffont(struct vga_config *, struct egavga_font *);

static int vga_selectfont(struct vga_config *, struct vgascreen *, const char *,
				   const char *);
static void vga_init_screen(struct vga_config *, struct vgascreen *,
		     const struct wsscreen_descr *, int, long *);
static void vga_init(struct vga_config *, bus_space_tag_t, bus_space_tag_t);
static void vga_setfont(struct vga_config *, struct vgascreen *);

static int vga_mapchar(void *, int, unsigned int *);
static void vga_putchar(void *, int, int, u_int, long);
static int vga_allocattr(void *, int, int, int, long *);
static void vga_copyrows(void *, int, int, int);
#ifdef WSDISPLAY_SCROLLSUPPORT
static void vga_scroll (void *, void *, int);
#endif

const struct wsdisplay_emulops vga_emulops = {
	pcdisplay_cursor,
	vga_mapchar,
	vga_putchar,
	pcdisplay_copycols,
	pcdisplay_erasecols,
	vga_copyrows,
	pcdisplay_eraserows,
	vga_allocattr,
#ifdef WSDISPLAY_CUSTOM_OUTPUT
	pcdisplay_replaceattr,
#else
	NULL,
#endif
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
	&vga_emulops,
	8, 16,
	WSSCREEN_WSCOLORS | WSSCREEN_HILIT | WSSCREEN_BLINK,
	NULL,
}, vga_25lscreen_mono = {
	"80x25", 80, 25,
	&vga_emulops,
	8, 16,
	WSSCREEN_HILIT | WSSCREEN_UNDERLINE | WSSCREEN_BLINK | WSSCREEN_REVERSE,
	NULL,
}, vga_25lscreen_bf = {
	"80x25bf", 80, 25,
	&vga_emulops,
	8, 16,
	WSSCREEN_WSCOLORS | WSSCREEN_BLINK,
	NULL,
}, vga_40lscreen = {
	"80x40", 80, 40,
	&vga_emulops,
	8, 10,
	WSSCREEN_WSCOLORS | WSSCREEN_HILIT | WSSCREEN_BLINK,
	NULL,
}, vga_40lscreen_mono = {
	"80x40", 80, 40,
	&vga_emulops,
	8, 10,
	WSSCREEN_HILIT | WSSCREEN_UNDERLINE | WSSCREEN_BLINK | WSSCREEN_REVERSE,
	NULL,
}, vga_40lscreen_bf = {
	"80x40bf", 80, 40,
	&vga_emulops,
	8, 10,
	WSSCREEN_WSCOLORS | WSSCREEN_BLINK,
	NULL,
}, vga_50lscreen = {
	"80x50", 80, 50,
	&vga_emulops,
	8, 8,
	WSSCREEN_WSCOLORS | WSSCREEN_HILIT | WSSCREEN_BLINK,
	NULL,
}, vga_50lscreen_mono = {
	"80x50", 80, 50,
	&vga_emulops,
	8, 8,
	WSSCREEN_HILIT | WSSCREEN_UNDERLINE | WSSCREEN_BLINK | WSSCREEN_REVERSE,
	NULL,
}, vga_50lscreen_bf = {
	"80x50bf", 80, 50,
	&vga_emulops,
	8, 8,
	WSSCREEN_WSCOLORS | WSSCREEN_BLINK,
	NULL,
}, vga_24lscreen = {
	"80x24", 80, 24,
	&vga_emulops,
	8, 16,
	WSSCREEN_WSCOLORS | WSSCREEN_HILIT | WSSCREEN_BLINK,
	NULL,
}, vga_24lscreen_mono = {
	"80x24", 80, 24,
	&vga_emulops,
	8, 16,
	WSSCREEN_HILIT | WSSCREEN_UNDERLINE | WSSCREEN_BLINK | WSSCREEN_REVERSE,
	NULL,
}, vga_24lscreen_bf = {
	"80x24bf", 80, 24,
	&vga_emulops,
	8, 16,
	WSSCREEN_WSCOLORS | WSSCREEN_BLINK,
	NULL,
};

#define VGA_SCREEN_CANTWOFONTS(type) (!((type)->capabilities & WSSCREEN_HILIT))

const struct wsscreen_descr *_vga_scrlist[] = {
	&vga_25lscreen,
	&vga_25lscreen_bf,
	&vga_40lscreen,
	&vga_40lscreen_bf,
	&vga_50lscreen,
	&vga_50lscreen_bf,
	&vga_24lscreen,
	&vga_24lscreen_bf,
	/* XXX other formats, graphics screen? */
}, *_vga_scrlist_mono[] = {
	&vga_25lscreen_mono,
	&vga_40lscreen_mono,
	&vga_50lscreen_mono,
	&vga_24lscreen_mono,
	/* XXX other formats, graphics screen? */
};

const struct wsscreen_list vga_screenlist = {
	sizeof(_vga_scrlist) / sizeof(struct wsscreen_descr *),
	_vga_scrlist
}, vga_screenlist_mono = {
	sizeof(_vga_scrlist_mono) / sizeof(struct wsscreen_descr *),
	_vga_scrlist_mono
};

static int	vga_ioctl(void *, void *, u_long, void *, int, struct lwp *);
static paddr_t	vga_mmap(void *, void *, off_t, int);
static int	vga_alloc_screen(void *, const struct wsscreen_descr *,
				 void **, int *, int *, long *);
static void	vga_free_screen(void *, void *);
static int	vga_show_screen(void *, void *, int,
				void (*)(void *, int, int), void *);
static int	vga_load_font(void *, void *, struct wsdisplay_font *);
#ifdef WSDISPLAY_CUSTOM_BORDER
static int	vga_getborder(struct vga_config *, u_int *);
static int	vga_setborder(struct vga_config *, u_int);
#endif /* WSDISPLAY_CUSTOM_BORDER */

static void vga_doswitch(struct vga_config *);
static void    vga_save_palette(struct vga_config *);
static void    vga_restore_palette(struct vga_config *);


const struct wsdisplay_accessops vga_accessops = {
	vga_ioctl,
	vga_mmap,
	vga_alloc_screen,
	vga_free_screen,
	vga_show_screen,
	vga_load_font,
	NULL,
#ifdef WSDISPLAY_SCROLLSUPPORT
	vga_scroll,
#else
	NULL,
#endif
};

/*
 * We want at least ASCII 32..127 be present in the
 * first font slot.
 */
#define vga_valid_primary_font(f) \
	(f->wsfont->encoding == WSDISPLAY_FONTENC_IBM || \
	f->wsfont->encoding == WSDISPLAY_FONTENC_ISO || \
	f->wsfont->encoding == WSDISPLAY_FONTENC_ISO2 || \
	f->wsfont->encoding == WSDISPLAY_FONTENC_ISO7 || \
	f->wsfont->encoding == WSDISPLAY_FONTENC_KOI8_R)

static struct egavga_font *
egavga_getfont(struct vga_config *vc, struct vgascreen *scr, const char *name,
	       int primary)
{
	struct egavga_font *f;
	int cookie;
	struct wsdisplay_font *wf;

	TAILQ_FOREACH(f, &vc->vc_fontlist, next) {
		if (wsfont_matches(f->wsfont, name,
		    8, scr->pcs.type->fontheight, 0, WSFONT_FIND_BITMAP) &&
		    (!primary || vga_valid_primary_font(f))) {
#ifdef VGAFONTDEBUG
			if (scr != &vga_console_screen || vga_console_attached)
				printf("vga_getfont: %s already present\n",
				    name ? name : "<default>");
#endif
			goto found;
		}
	}

	cookie = wsfont_find(name, 8, scr->pcs.type->fontheight, 0,
	    WSDISPLAY_FONTORDER_L2R, 0, WSFONT_FIND_BITMAP);
	/* XXX obey "primary" */
	if (cookie == -1) {
#ifdef VGAFONTDEBUG
		if (scr != &vga_console_screen || vga_console_attached)
			printf("vga_getfont: %s not found\n",
			    name ? name : "<default>");
#endif
		return (0);
	}

	if (wsfont_lock(cookie, &wf))
		return (0);

#ifdef VGA_CONSOLE_SCREENTYPE
	if (scr == &vga_console_screen)
		f = &vga_consolefont;
	else
#endif
	f = malloc(sizeof(struct egavga_font), M_DEVBUF, M_NOWAIT);
	if (!f) {
		wsfont_unlock(cookie);
		return (0);
	}
	f->wsfont = wf;
	f->cookie = cookie;
	f->slot = -1; /* not yet loaded */
	f->usecount = 0; /* incremented below */
	TAILQ_INSERT_TAIL(&vc->vc_fontlist, f, next);

found:
	f->usecount++;
#ifdef VGAFONTDEBUG
	if (scr != &vga_console_screen || vga_console_attached)
		printf("vga_getfont: usecount=%d\n", f->usecount);
#endif
	return (f);
}

static void
egavga_unreffont(struct vga_config *vc, struct egavga_font *f)
{

	f->usecount--;
#ifdef VGAFONTDEBUG
	printf("vga_unreffont: usecount=%d\n", f->usecount);
#endif
	if (f->usecount == 0 && f->cookie != -1) {
		TAILQ_REMOVE(&vc->vc_fontlist, f, next);
		if (f->slot != -1) {
			KASSERT(vc->vc_fonts[f->slot] == f);
			vc->vc_fonts[f->slot] = 0;
		}
		wsfont_unlock(f->cookie);
#ifdef VGA_CONSOLE_SCREENTYPE
		if (f != &vga_consolefont)
#endif
		free(f, M_DEVBUF);
	}
}

static int
vga_selectfont(struct vga_config *vc, struct vgascreen *scr, const char *name1,
	       const char *name2)
{
	const struct wsscreen_descr *type = scr->pcs.type;
	struct egavga_font *f1, *f2;

	f1 = egavga_getfont(vc, scr, name1, 1);
	if (!f1)
		return (ENXIO);

	if (VGA_SCREEN_CANTWOFONTS(type) && name2) {
		f2 = egavga_getfont(vc, scr, name2, 0);
		if (!f2) {
			egavga_unreffont(vc, f1);
			return (ENXIO);
		}
	} else
		f2 = 0;

#ifdef VGAFONTDEBUG
	if (scr != &vga_console_screen || vga_console_attached) {
		printf("vga (%s): font1=%s (slot %d)", type->name,
		    f1->wsfont->name, f1->slot);
		if (f2)
			printf(", font2=%s (slot %d)",
			    f2->wsfont->name, f2->slot);
		printf("\n");
	}
#endif
	if (scr->fontset1)
		egavga_unreffont(vc, scr->fontset1);
	scr->fontset1 = f1;
	if (scr->fontset2)
		egavga_unreffont(vc, scr->fontset2);
	scr->fontset2 = f2;
	return (0);
}

static void
vga_init_screen(struct vga_config *vc, struct vgascreen *scr,
		const struct wsscreen_descr *type, int existing, long *attrp)
{
	int cpos;
	int res __diagused;

	scr->cfg = vc;
	scr->pcs.hdl = (struct pcdisplay_handle *)&vc->hdl;
	scr->pcs.type = type;
	scr->pcs.active = existing;
	scr->mindispoffset = 0;
	if (vc->vc_quirks & VGA_QUIRK_NOFASTSCROLL)
		scr->maxdispoffset = 0;
	else
		scr->maxdispoffset = 0x8000 - type->nrows * type->ncols * 2;

	if (existing) {
		vc->active = scr;

		cpos = vga_6845_read(&vc->hdl, cursorh) << 8;
		cpos |= vga_6845_read(&vc->hdl, cursorl);

		/* make sure we have a valid cursor position */
		if (cpos < 0 || cpos >= type->nrows * type->ncols)
			cpos = 0;

		scr->pcs.dispoffset = vga_6845_read(&vc->hdl, startadrh) << 9;
		scr->pcs.dispoffset |= vga_6845_read(&vc->hdl, startadrl) << 1;

		/* make sure we have a valid memory offset */
		if (scr->pcs.dispoffset < scr->mindispoffset ||
		    scr->pcs.dispoffset > scr->maxdispoffset)
			scr->pcs.dispoffset = scr->mindispoffset;

		if (type != vc->currenttype) {
			vga_setscreentype(&vc->hdl, type);
			vc->currenttype = type;
		}
	} else {
		cpos = 0;
		scr->pcs.dispoffset = scr->mindispoffset;
	}

	scr->pcs.visibleoffset = scr->pcs.dispoffset;
	scr->vga_rollover = 0;

	scr->pcs.cursorrow = cpos / type->ncols;
	scr->pcs.cursorcol = cpos % type->ncols;
	pcdisplay_cursor_init(&scr->pcs, existing);

#ifdef __alpha__
	if (!vc->hdl.vh_mono)
		/*
		 * DEC firmware uses a blue background.
		 * XXX These should be specified as kernel options for
		 * XXX alpha only, not hardcoded here (which is wrong
		 * XXX anyway because the emulation layer will assume
		 * XXX the default attribute is white on black).
		 */
		res = vga_allocattr(scr, WSCOL_WHITE, WSCOL_BLUE,
		    WSATTR_WSCOLORS, attrp);
	else
#endif
	res = vga_allocattr(scr, 0, 0, 0, attrp);
#ifdef DIAGNOSTIC
	if (res)
		panic("vga_init_screen: attribute botch");
#endif

	scr->pcs.mem = NULL;

	scr->fontset1 = scr->fontset2 = 0;
	if (vga_selectfont(vc, scr, 0, 0)) {
		if (scr == &vga_console_screen)
			panic("vga_init_screen: no font");
		else
			printf("vga_init_screen: no font\n");
	}
	if (existing)
		vga_setfont(vc, scr);

	vc->nscreens++;
	LIST_INSERT_HEAD(&vc->screens, scr, next);
}

static void
vga_init(struct vga_config *vc, bus_space_tag_t iot, bus_space_tag_t memt)
{
	struct vga_handle *vh = &vc->hdl;
	uint8_t mor;
	int i;

	vh->vh_iot = iot;
	vh->vh_memt = memt;

	if (bus_space_map(vh->vh_iot, 0x3c0, 0x10, 0, &vh->vh_ioh_vga))
		panic("vga_init: couldn't map vga io");

	/* read "misc output register" */
	mor = vga_raw_read(vh, VGA_MISC_DATAR);
	vh->vh_mono = !(mor & 1);

	if (bus_space_map(vh->vh_iot, (vh->vh_mono ? 0x3b0 : 0x3d0), 0x10, 0,
	    &vh->vh_ioh_6845))
		panic("vga_init: couldn't map 6845 io");

	if (bus_space_map(vh->vh_memt, 0xa0000, 0x20000, 0, &vh->vh_allmemh))
		panic("vga_init: couldn't map memory");

	if (bus_space_subregion(vh->vh_memt, vh->vh_allmemh,
	    (vh->vh_mono ? 0x10000 : 0x18000), 0x8000, &vh->vh_memh))
		panic("vga_init: mem subrange failed");

	vc->nscreens = 0;
	LIST_INIT(&vc->screens);
	vc->active = NULL;
	vc->currenttype = vh->vh_mono ? &vga_25lscreen_mono : &vga_25lscreen;
	callout_init(&vc->vc_switch_callout, 0);

	wsfont_init();
	if (vga_no_builtinfont) {
		struct wsdisplay_font *wf;
		int cookie;

		cookie = wsfont_find(NULL, 8, 16, 0,
		     WSDISPLAY_FONTORDER_L2R, 0, WSFONT_FIND_BITMAP);
		if (cookie == -1 || wsfont_lock(cookie, &wf))
			panic("vga_init: can't load console font");
		vga_loadchars(&vc->hdl, 0, wf->firstchar, wf->numchars,
		    wf->fontheight, wf->data);
		vga_builtinfont.wsfont = wf;
		vga_builtinfont.cookie = cookie;
		vga_builtinfont.slot = 0;
	}
	vc->vc_fonts[0] = &vga_builtinfont;
	for (i = 1; i < 8; i++)
		vc->vc_fonts[i] = 0;
	TAILQ_INIT(&vc->vc_fontlist);
	TAILQ_INSERT_HEAD(&vc->vc_fontlist, &vga_builtinfont, next);

	vc->currentfontset1 = vc->currentfontset2 = 0;

	if (!vh->vh_mono && (u_int)WSDISPLAY_BORDER_COLOR < sizeof(fgansitopc))
		_vga_attr_write(vh, VGA_ATC_OVERSCAN,
		                fgansitopc[WSDISPLAY_BORDER_COLOR]);
	vga_save_palette(vc);
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
		vga_init(vc, iot, memt);
	}

	if (quirks & VGA_QUIRK_ONEFONT) {
		vc->vc_nfontslots = 1;
#ifndef VGA_CONSOLE_ATI_BROKEN_FONTSEL
		/*
		 * XXX maybe invalidate font in slot > 0, but this can
		 * only be happen with VGA_CONSOLE_SCREENTYPE, and then
		 * we require VGA_CONSOLE_ATI_BROKEN_FONTSEL anyway.
		 */
#endif
	} else {
		vc->vc_nfontslots = 8;
#ifndef VGA_CONSOLE_ATI_BROKEN_FONTSEL
		/*
		 * XXX maybe validate builtin font shifted to slot 1 if
		 * slot 0 got overwritten because of VGA_CONSOLE_SCREENTYPE,
		 * but it will be reloaded anyway if needed.
		 */
#endif
	}

	/*
	 * Save the builtin font to memory. In case it got overwritten
	 * in console initialization, use the copy in slot 1.
	 */
#ifdef VGA_CONSOLE_ATI_BROKEN_FONTSEL
#define BUILTINFONTLOC (vga_builtinfont.slot == -1 ? 1 : 0)
#else
	KASSERT(vga_builtinfont.slot == 0);
#define BUILTINFONTLOC (0)
#endif
	if (!vga_no_builtinfont) {
		char *data =
		    malloc(256 * vga_builtinfont.wsfont->fontheight,
		    M_DEVBUF, M_WAITOK);
		vga_readoutchars(&vc->hdl, BUILTINFONTLOC, 0, 256,
		    vga_builtinfont.wsfont->fontheight, data);
		vga_builtinfont.wsfont->data = data;
	}

	vc->vc_type = type;
	vc->vc_funcs = vf;
	vc->vc_quirks = quirks;

	sc->sc_vc = vc;
	vc->softc = sc;

	aa.console = console;
	aa.scrdata = (vc->hdl.vh_mono ? &vga_screenlist_mono : &vga_screenlist);
	aa.accessops = &vga_accessops;
	aa.accesscookie = vc;

	config_found_ia(sc->sc_dev, "wsemuldisplaydev", &aa, wsemuldisplaydevprint);
}

int
vga_cnattach(bus_space_tag_t iot, bus_space_tag_t memt, int type, int check)
{
	long defattr;
	const struct wsscreen_descr *scr;

	if (check && !vga_common_probe(iot, memt))
		return (ENXIO);

	/* set up bus-independent VGA configuration */
	vga_init(&vga_console_vc, iot, memt);
#ifdef VGA_CONSOLE_SCREENTYPE
	scr = wsdisplay_screentype_pick(vga_console_vc.hdl.vh_mono ?
	    &vga_screenlist_mono : &vga_screenlist, VGA_CONSOLE_SCREENTYPE);
	if (!scr)
		panic("vga_cnattach: invalid screen type");
#else
	scr = vga_console_vc.currenttype;
#endif
#ifdef VGA_CONSOLE_ATI_BROKEN_FONTSEL
	/*
	 * On some (most/all?) ATI cards, only font slot 0 is usable.
	 * vga_init_screen() might need font slot 0 for a non-default
	 * console font, so save the builtin VGA font to another font slot.
	 * The attach() code will take care later.
	 */
	vga_console_vc.vc_quirks |= VGA_QUIRK_ONEFONT; /* redundant */
	vga_copyfont01(&vga_console_vc.hdl);
	vga_console_vc.vc_nfontslots = 1;
#else
	vga_console_vc.vc_nfontslots = 8;
#endif
#ifdef notdef
	/* until we know better, assume "fast scrolling" does not work */
	vga_console_vc.vc_quirks |= VGA_QUIRK_NOFASTSCROLL;
#endif

	vga_init_screen(&vga_console_vc, &vga_console_screen, scr, 1, &defattr);

	wsdisplay_cnattach(scr, &vga_console_screen,
	    vga_console_screen.pcs.cursorcol,
	    vga_console_screen.pcs.cursorrow, defattr);

	vgaconsole = 1;
	vga_console_type = type;
	return (0);
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
	    bus_space_is_equal(iot, vga_console_vc.hdl.vh_iot) &&
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

static int
vga_ioctl(void *v, void *vs, u_long cmd, void *data, int flag, struct lwp *l)
{
	struct vga_config *vc = v;
	struct vgascreen *scr = vs;
	const struct vga_funcs *vf = vc->vc_funcs;

	switch (cmd) {
	case WSDISPLAYIO_SMODE:
		if (*(u_int *)data == WSDISPLAYIO_MODE_EMUL)
			vga_restore_palette(vc);
		return 0;

	case WSDISPLAYIO_GTYPE:
		*(int *)data = vc->vc_type;
		return 0;

	case WSDISPLAYIO_GINFO:
		/* XXX should get detailed hardware information here */
		return EPASSTHROUGH;

	case WSDISPLAYIO_GVIDEO:
		*(int *)data = (vga_get_video(vc) ?
		    WSDISPLAYIO_VIDEO_ON : WSDISPLAYIO_VIDEO_OFF);
		return 0;

	case WSDISPLAYIO_SVIDEO:
		vga_set_video(vc, *(int *)data == WSDISPLAYIO_VIDEO_ON);
		return 0;

	case WSDISPLAYIO_GETWSCHAR:
		KASSERT(scr != NULL);
		return pcdisplay_getwschar(&scr->pcs,
		    (struct wsdisplay_char *)data);

	case WSDISPLAYIO_PUTWSCHAR:
		KASSERT(scr != NULL);
		return pcdisplay_putwschar(&scr->pcs,
		    (struct wsdisplay_char *)data);

#ifdef WSDISPLAY_CUSTOM_BORDER
	case WSDISPLAYIO_GBORDER:
		return (vga_getborder(vc, (u_int *)data));

	case WSDISPLAYIO_SBORDER:
		return (vga_setborder(vc, *(u_int *)data));
#endif

	case WSDISPLAYIO_GETCMAP:
	case WSDISPLAYIO_PUTCMAP:
	case WSDISPLAYIO_GCURPOS:
	case WSDISPLAYIO_SCURPOS:
	case WSDISPLAYIO_GCURMAX:
	case WSDISPLAYIO_GCURSOR:
	case WSDISPLAYIO_SCURSOR:
		/* NONE of these operations are by the generic VGA driver. */
		return EPASSTHROUGH;
	}

	if (vc->vc_funcs == NULL)
		return (EPASSTHROUGH);

	if (vf->vf_ioctl == NULL)
		return (EPASSTHROUGH);

	return ((*vf->vf_ioctl)(v, cmd, data, flag, l));
}

static paddr_t
vga_mmap(void *v, void *vs, off_t offset, int prot)
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
vga_alloc_screen(void *v, const struct wsscreen_descr *type, void **cookiep,
		 int *curxp, int *curyp, long *defattrp)
{
	struct vga_config *vc = v;
	struct vgascreen *scr;

	if (vc->nscreens == 1) {
		struct vgascreen *scr1 = vc->screens.lh_first;
		/*
		 * When allocating the second screen, get backing store
		 * for the first one too.
		 * XXX We could be more clever and use video RAM.
		 */
		scr1->pcs.mem =
		    malloc(scr1->pcs.type->ncols * scr1->pcs.type->nrows * 2,
		    M_DEVBUF, M_WAITOK);
	}

	scr = malloc(sizeof(struct vgascreen), M_DEVBUF, M_WAITOK);
	vga_init_screen(vc, scr, type, vc->nscreens == 0, defattrp);

	if (vc->nscreens > 1) {
		scr->pcs.mem = malloc(type->ncols * type->nrows * 2,
		    M_DEVBUF, M_WAITOK);
		pcdisplay_eraserows(&scr->pcs, 0, type->nrows, *defattrp);
	}

	*cookiep = scr;
	*curxp = scr->pcs.cursorcol;
	*curyp = scr->pcs.cursorrow;

	return (0);
}

static void
vga_free_screen(void *v, void *cookie)
{
	struct vgascreen *vs = cookie;
	struct vga_config *vc = vs->cfg;

	LIST_REMOVE(vs, next);
	vc->nscreens--;
	if (vs->fontset1)
		egavga_unreffont(vc, vs->fontset1);
	if (vs->fontset2)
		egavga_unreffont(vc, vs->fontset2);

	if (vs != &vga_console_screen)
		free(vs, M_DEVBUF);
	else
		panic("vga_free_screen: console");

	if (vc->active == vs)
		vc->active = 0;
}

static void vga_usefont(struct vga_config *, struct egavga_font *);

static void
vga_usefont(struct vga_config *vc, struct egavga_font *f)
{
	int slot;
	struct egavga_font *of;

	if (f->slot != -1)
		goto toend;

	for (slot = 0; slot < vc->vc_nfontslots; slot++) {
		if (!vc->vc_fonts[slot])
			goto loadit;
	}

	/* have to kick out another one */
	TAILQ_FOREACH(of, &vc->vc_fontlist, next) {
		if (of->slot != -1) {
			KASSERT(vc->vc_fonts[of->slot] == of);
			slot = of->slot;
			of->slot = -1;
			goto loadit;
		}
	}
	panic("vga_usefont");

loadit:
	vga_loadchars(&vc->hdl, slot, f->wsfont->firstchar,
	    f->wsfont->numchars, f->wsfont->fontheight, f->wsfont->data);
	f->slot = slot;
	vc->vc_fonts[slot] = f;

toend:
	TAILQ_REMOVE(&vc->vc_fontlist, f, next);
	TAILQ_INSERT_TAIL(&vc->vc_fontlist, f, next);
}

static void
vga_setfont(struct vga_config *vc, struct vgascreen *scr)
{
	int fontslot1, fontslot2;

	if (scr->fontset1)
		vga_usefont(vc, scr->fontset1);
	if (scr->fontset2)
		vga_usefont(vc, scr->fontset2);

	fontslot1 = (scr->fontset1 ? scr->fontset1->slot : 0);
	fontslot2 = (scr->fontset2 ? scr->fontset2->slot : fontslot1);
	if (vc->currentfontset1 != fontslot1 ||
	    vc->currentfontset2 != fontslot2) {
		vga_setfontset(&vc->hdl, fontslot1, fontslot2);
		vc->currentfontset1 = fontslot1;
		vc->currentfontset2 = fontslot2;
	}
}

static int
vga_show_screen(void *v, void *cookie, int waitok,
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
		    (void(*)(void *))vga_doswitch, vc);
		return (EAGAIN);
	}

	vga_doswitch(vc);
	return (0);
}

static void
vga_doswitch(struct vga_config *vc)
{
	struct vgascreen *scr, *oldscr;
	struct vga_handle *vh = &vc->hdl;
	const struct wsscreen_descr *type;

	scr = vc->wantedscreen;
	if (!scr) {
		printf("vga_doswitch: disappeared\n");
		(*vc->switchcb)(vc->switchcbarg, EIO, 0);
		return;
	}
	type = scr->pcs.type;
	oldscr = vc->active; /* can be NULL! */
#ifdef DIAGNOSTIC
	if (oldscr) {
		if (!oldscr->pcs.active)
			panic("vga_show_screen: not active");
		if (oldscr->pcs.type != vc->currenttype)
			panic("vga_show_screen: bad type");
	}
#endif
	if (scr == oldscr) {
		return;
	}
#ifdef DIAGNOSTIC
	if (scr->pcs.active)
		panic("vga_show_screen: active");
#endif

	if (oldscr) {
		const struct wsscreen_descr *oldtype = oldscr->pcs.type;

		oldscr->pcs.active = 0;
		bus_space_read_region_2(vh->vh_memt, vh->vh_memh,
		    oldscr->pcs.dispoffset, oldscr->pcs.mem,
		    oldtype->ncols * oldtype->nrows);
	}

	if (vc->currenttype != type) {
		vga_setscreentype(vh, type);
		vc->currenttype = type;
	}

	vga_setfont(vc, scr);
	vga_restore_palette(vc);

	scr->pcs.visibleoffset = scr->pcs.dispoffset = scr->mindispoffset;
	if (!oldscr || (scr->pcs.dispoffset != oldscr->pcs.dispoffset)) {
		vga_6845_write(vh, startadrh, scr->pcs.dispoffset >> 9);
		vga_6845_write(vh, startadrl, scr->pcs.dispoffset >> 1);
	}

	bus_space_write_region_2(vh->vh_memt, vh->vh_memh,
	    scr->pcs.dispoffset, scr->pcs.mem, type->ncols * type->nrows);
	scr->pcs.active = 1;

	vc->active = scr;

	pcdisplay_cursor(&scr->pcs, scr->pcs.cursoron,
	    scr->pcs.cursorrow, scr->pcs.cursorcol);

	vc->wantedscreen = 0;
	if (vc->switchcb)
		(*vc->switchcb)(vc->switchcbarg, 0, 0);
}

static int
vga_load_font(void *v, void *cookie, struct wsdisplay_font *data)
{
	struct vga_config *vc = v;
	struct vgascreen *scr = cookie;
	char *name2;
	int res;

	if (scr) {
		name2 = NULL;
		if (data->name) {
			name2 = strchr(data->name, ',');
			if (name2)
				*name2++ = '\0';
		}
		res = vga_selectfont(vc, scr, data->name, name2);
		if (!res && scr->pcs.active)
			vga_setfont(vc, scr);
		return (res);
	}

	return (0);
}

static int
vga_allocattr(void *id, int fg, int bg, int flags, long *attrp)
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
vga_copyrows(void *id, int srcrow, int dstrow, int nrows)
{
	struct vgascreen *scr = id;
	bus_space_tag_t memt = scr->pcs.hdl->ph_memt;
	bus_space_handle_t memh = scr->pcs.hdl->ph_memh;
	int ncols = scr->pcs.type->ncols;
	bus_size_t srcoff, dstoff;

	srcoff = srcrow * ncols + 0;
	dstoff = dstrow * ncols + 0;

	if (scr->pcs.active) {
		if (dstrow == 0 && (srcrow + nrows == scr->pcs.type->nrows)) {
#ifdef PCDISPLAY_SOFTCURSOR
			int cursoron = scr->pcs.cursoron;

			if (cursoron)
				pcdisplay_cursor(&scr->pcs, 0,
				    scr->pcs.cursorrow, scr->pcs.cursorcol);
#endif
			/* scroll up whole screen */
			if ((scr->pcs.dispoffset + srcrow * ncols * 2)
			    <= scr->maxdispoffset) {
				scr->pcs.dispoffset += srcrow * ncols * 2;
			} else {
				bus_space_copy_region_2(memt, memh,
				    scr->pcs.dispoffset + srcoff * 2,
				    memh, scr->mindispoffset, nrows * ncols);
				scr->pcs.dispoffset = scr->mindispoffset;
			}
			vga_6845_write(&scr->cfg->hdl, startadrh,
			    scr->pcs.dispoffset >> 9);
			vga_6845_write(&scr->cfg->hdl, startadrl,
			    scr->pcs.dispoffset >> 1);
#ifdef PCDISPLAY_SOFTCURSOR
			if (cursoron)
				pcdisplay_cursor(&scr->pcs, 1,
				    scr->pcs.cursorrow, scr->pcs.cursorcol);
#endif
		} else {
			bus_space_copy_region_2(memt, memh,
			    scr->pcs.dispoffset + srcoff * 2,
			    memh, scr->pcs.dispoffset + dstoff * 2,
			    nrows * ncols);
		}
	} else
		memcpy(&scr->pcs.mem[dstoff], &scr->pcs.mem[srcoff],
		    nrows * ncols * 2);
}

#ifdef WSCONS_SUPPORT_PCVTFONTS

#define NOTYET 0xffff
static const uint16_t pcvt_unichars[0xa0] = {
/* 0 */	_e006U, /* N/L control */
	NOTYET, NOTYET, NOTYET, NOTYET, NOTYET, NOTYET, NOTYET,
	NOTYET,
	0x2409, /* SYMBOL FOR HORIZONTAL TABULATION */
	0x240a, /* SYMBOL FOR LINE FEED */
	0x240b, /* SYMBOL FOR VERTICAL TABULATION */
	0x240c, /* SYMBOL FOR FORM FEED */
	0x240d, /* SYMBOL FOR CARRIAGE RETURN */
	NOTYET, NOTYET,
/* 1 */	NOTYET, NOTYET, NOTYET, NOTYET, NOTYET, NOTYET, NOTYET, NOTYET,
	NOTYET, NOTYET, NOTYET, NOTYET, NOTYET, NOTYET, NOTYET, NOTYET,
/* 2 */	NOTYET, NOTYET, NOTYET, NOTYET, NOTYET, NOTYET, NOTYET, NOTYET,
	NOTYET, NOTYET, NOTYET, NOTYET, NOTYET, NOTYET, NOTYET, NOTYET,
/* 3 */	NOTYET, NOTYET, NOTYET, NOTYET, NOTYET, NOTYET, NOTYET, NOTYET,
	NOTYET, NOTYET, NOTYET, NOTYET, NOTYET, NOTYET, NOTYET, NOTYET,
/* 4 */	0x03c1, /* GREEK SMALL LETTER RHO */
	0x03c8, /* GREEK SMALL LETTER PSI */
	0x2202, /* PARTIAL DIFFERENTIAL */
	0x03bb, /* GREEK SMALL LETTER LAMDA */
	0x03b9, /* GREEK SMALL LETTER IOTA */
	0x03b7, /* GREEK SMALL LETTER ETA */
	0x03b5, /* GREEK SMALL LETTER EPSILON */
	0x03c7, /* GREEK SMALL LETTER CHI */
	0x2228, /* LOGICAL OR */
	0x2227, /* LOGICAL AND */
	0x222a, /* UNION */
	0x2283, /* SUPERSET OF */
	0x2282, /* SUBSET OF */
	0x03a5, /* GREEK CAPITAL LETTER UPSILON */
	0x039e, /* GREEK CAPITAL LETTER XI */
	0x03a8, /* GREEK CAPITAL LETTER PSI */
/* 5 */	0x03a0, /* GREEK CAPITAL LETTER PI */
	0x21d2, /* RIGHTWARDS DOUBLE ARROW */
	0x21d4, /* LEFT RIGHT DOUBLE ARROW */
	0x039b, /* GREEK CAPITAL LETTER LAMDA */
	0x0398, /* GREEK CAPITAL LETTER THETA */
	0x2243, /* ASYMPTOTICALLY EQUAL TO */
	0x2207, /* NABLA */
	0x2206, /* INCREMENT */
	0x221d, /* PROPORTIONAL TO */
	0x2234, /* THEREFORE */
	0x222b, /* INTEGRAL */
	0x2215, /* DIVISION SLASH */
	0x2216, /* SET MINUS */
	_e00eU, /* angle? */
	_e00dU, /* inverted angle? */
	_e00bU, /* braceleftmid */
/* 6 */	_e00cU, /* bracerightmid */
	_e007U, /* bracelefttp */
	_e008U, /* braceleftbt */
	_e009U, /* bracerighttp */
	_e00aU, /* bracerightbt */
	0x221a, /* SQUARE ROOT */
	0x03c9, /* GREEK SMALL LETTER OMEGA */
	0x00a5, /* YEN SIGN */
	0x03be, /* GREEK SMALL LETTER XI */
	0x00fd, /* LATIN SMALL LETTER Y WITH ACUTE */
	0x00fe, /* LATIN SMALL LETTER THORN */
	0x00f0, /* LATIN SMALL LETTER ETH */
	0x00de, /* LATIN CAPITAL LETTER THORN */
	0x00dd, /* LATIN CAPITAL LETTER Y WITH ACUTE */
	0x00d7, /* MULTIPLICATION SIGN */
	0x00d0, /* LATIN CAPITAL LETTER ETH */
/* 7 */	0x00be, /* VULGAR FRACTION THREE QUARTERS */
	0x00b8, /* CEDILLA */
	0x00b4, /* ACUTE ACCENT */
	0x00af, /* MACRON */
	0x00ae, /* REGISTERED SIGN */
	0x00ad, /* SOFT HYPHEN */
	0x00ac, /* NOT SIGN */
	0x00a8, /* DIAERESIS */
	0x2260, /* NOT EQUAL TO */
	0x23bd, /* scan 9 */
	0x23bc, /* scan 7 */
	0x2500, /* scan 5 */
	0x23bb, /* scan 3 */
	0x23ba, /* scan 1 */
	0x03c5, /* GREEK SMALL LETTER UPSILON */
	0x00f8, /* LATIN SMALL LETTER O WITH STROKE */
/* 8 */	0x0153, /* LATIN SMALL LIGATURE OE */
	0x00f5, /* LATIN SMALL LETTER O WITH TILDE !!!doc bug */
	0x00e3, /* LATIN SMALL LETTER A WITH TILDE */
	0x0178, /* LATIN CAPITAL LETTER Y WITH DIAERESIS */
	0x00db, /* LATIN CAPITAL LETTER U WITH CIRCUMFLEX */
	0x00da, /* LATIN CAPITAL LETTER U WITH ACUTE */
	0x00d9, /* LATIN CAPITAL LETTER U WITH GRAVE */
	0x00d8, /* LATIN CAPITAL LETTER O WITH STROKE */
	0x0152, /* LATIN CAPITAL LIGATURE OE */
	0x00d5, /* LATIN CAPITAL LETTER O WITH TILDE */
	0x00d4, /* LATIN CAPITAL LETTER O WITH CIRCUMFLEX */
	0x00d3, /* LATIN CAPITAL LETTER O WITH ACUTE */
	0x00d2, /* LATIN CAPITAL LETTER O WITH GRAVE */
	0x00cf, /* LATIN CAPITAL LETTER I WITH DIAERESIS */
	0x00ce, /* LATIN CAPITAL LETTER I WITH CIRCUMFLEX */
	0x00cd, /* LATIN CAPITAL LETTER I WITH ACUTE */
/* 9 */	0x00cc, /* LATIN CAPITAL LETTER I WITH GRAVE */
	0x00cb, /* LATIN CAPITAL LETTER E WITH DIAERESIS */
	0x00ca, /* LATIN CAPITAL LETTER E WITH CIRCUMFLEX */
	0x00c8, /* LATIN CAPITAL LETTER E WITH GRAVE */
	0x00c3, /* LATIN CAPITAL LETTER A WITH TILDE */
	0x00c2, /* LATIN CAPITAL LETTER A WITH CIRCUMFLEX */
	0x00c1, /* LATIN CAPITAL LETTER A WITH ACUTE */
	0x00c0, /* LATIN CAPITAL LETTER A WITH GRAVE */
	0x00b9, /* SUPERSCRIPT ONE */
	0x00b7, /* MIDDLE DOT */
	0x03b6, /* GREEK SMALL LETTER ZETA */
	0x00b3, /* SUPERSCRIPT THREE */
	0x00a9, /* COPYRIGHT SIGN */
	0x00a4, /* CURRENCY SIGN */
	0x03ba, /* GREEK SMALL LETTER KAPPA */
	_e000U  /* mirrored question mark? */
};

static int vga_pcvt_mapchar(int, u_int *);

static int
vga_pcvt_mapchar(int uni, u_int *index)
{
	int i;

	for (i = 0; i < 0xa0; i++) /* 0xa0..0xff are reserved */
		if (uni == pcvt_unichars[i]) {
			*index = i;
			return (5);
		}
	*index = 0x99; /* middle dot */
	return (0);
}

#endif /* WSCONS_SUPPORT_PCVTFONTS */

#ifdef WSCONS_SUPPORT_ISO7FONTS

static int
vga_iso7_mapchar(int uni, u_int *index)
{

	/*
	 * U+0384 (GREEK TONOS) to
	 * U+03ce (GREEK SMALL LETTER OMEGA WITH TONOS)
	 * map directly to the iso-9 font
	 */
	if (uni >= 0x0384 && uni <= 0x03ce) {
		/* U+0384 is at offset 0xb4 in the font */
		*index = uni - 0x0384 + 0xb4;
		return (5);
	}

	/* XXX more chars in the iso-9 font */

	*index = 0xa4; /* shaded rectangle */
	return (0);
}

#endif /* WSCONS_SUPPORT_ISO7FONTS */

static const uint16_t iso2_unichars[0x60] = {
	0x00A0, 0x0104, 0x02D8, 0x0141, 0x00A4, 0x013D, 0x015A, 0x00A7,
	0x00A8, 0x0160, 0x015E, 0x0164, 0x0179, 0x00AD, 0x017D, 0x017B,
	0x00B0, 0x0105, 0x02DB, 0x0142, 0x00B4, 0x013E, 0x015B, 0x02C7,
	0x00B8, 0x0161, 0x015F, 0x0165, 0x017A, 0x02DD, 0x017E, 0x017C,
	0x0154, 0x00C1, 0x00C2, 0x0102, 0x00C4, 0x0139, 0x0106, 0x00C7,
	0x010C, 0x00C9, 0x0118, 0x00CB, 0x011A, 0x00CD, 0x00CE, 0x010E,
	0x0110, 0x0143, 0x0147, 0x00D3, 0x00D4, 0x0150, 0x00D6, 0x00D7,
	0x0158, 0x016E, 0x00DA, 0x0170, 0x00DC, 0x00DD, 0x0162, 0x00DF,
	0x0155, 0x00E1, 0x00E2, 0x0103, 0x00E4, 0x013A, 0x0107, 0x00E7,
	0x010D, 0x00E9, 0x0119, 0x00EB, 0x011B, 0x00ED, 0x00EE, 0x010F,
	0x0111, 0x0144, 0x0148, 0x00F3, 0x00F4, 0x0151, 0x00F6, 0x00F7,
	0x0159, 0x016F, 0x00FA, 0x0171, 0x00FC, 0x00FD, 0x0163, 0x02D9
};

static const uint16_t koi8_unichars[0x40] = {
	0x044E, 0x0430, 0x0431, 0x0446, 0x0434, 0x0435, 0x0444, 0x0433,
	0x0445, 0x0438, 0x0439, 0x043A, 0x043B, 0x043C, 0x043D, 0x043E,
	0x043F, 0x044F, 0x0440, 0x0441, 0x0442, 0x0443, 0x0436, 0x0432,
	0x044C, 0x044B, 0x0437, 0x0448, 0x044D, 0x0449, 0x0447, 0x044A,
	0x042E, 0x0410, 0x0411, 0x0426, 0x0414, 0x0415, 0x0424, 0x0413,
	0x0425, 0x0418, 0x0419, 0x041A, 0x041B, 0x041C, 0x041D, 0x041E,
	0x041F, 0x042F, 0x0420, 0x0421, 0x0422, 0x0423, 0x0416, 0x0412,
	0x042C, 0x042B, 0x0417, 0x0428, 0x042D, 0x0429, 0x0427, 0x042A
};

static int _vga_mapchar(void *, const struct egavga_font *, int, u_int *);

static int
_vga_mapchar(void *id, const struct egavga_font *font, int uni, u_int *index)
{

	switch (font->wsfont->encoding) {
	case WSDISPLAY_FONTENC_ISO:
		if (uni < 256) {
			*index = uni;
			return (5);
		} else {
			*index = ' ';
			return (0);
		}
	case WSDISPLAY_FONTENC_ISO2:
		if (uni < 0xa0) {
			*index = uni;
			return (5);
		} else {
			int i;
			for (i = 0; i < 0x60; i++) {
				if (uni == iso2_unichars[i]) {
					*index = i + 0xa0;
					return (5);
				}
			}
			*index = 0xa4; /* currency sign */
			return (0);
		}
	case WSDISPLAY_FONTENC_KOI8_R:
		if (uni < 0x80) {
			*index = uni;
			return (5);
		} else {
			int i;
			for (i = 0; i < 0x40; i++) {
				if (uni == koi8_unichars[i]) {
					*index = i + 0xc0;
					return (5);
				}
			}
			*index = 0x94; /* box */
			return (0);
		}
	case WSDISPLAY_FONTENC_IBM:
		return (pcdisplay_mapchar(id, uni, index));
#ifdef WSCONS_SUPPORT_PCVTFONTS
	case WSDISPLAY_FONTENC_PCVT:
		return (vga_pcvt_mapchar(uni, index));
#endif
#ifdef WSCONS_SUPPORT_ISO7FONTS
	case WSDISPLAY_FONTENC_ISO7:
		return (vga_iso7_mapchar(uni, index));
#endif
	default:
#ifdef VGAFONTDEBUG
		printf("_vga_mapchar: encoding=%d\n", font->wsfont->encoding);
#endif
		*index = ' ';
		return (0);
	}
}

static int
vga_mapchar(void *id, int uni, u_int *index)
{
	struct vgascreen *scr = id;
	u_int idx1, idx2;
	int res1, res2;

	res1 = 0;
	idx1 = ' '; /* space */
	if (scr->fontset1)
		res1 = _vga_mapchar(id, scr->fontset1, uni, &idx1);
	res2 = -1;
	if (scr->fontset2) {
		KASSERT(VGA_SCREEN_CANTWOFONTS(scr->pcs.type));
		res2 = _vga_mapchar(id, scr->fontset2, uni, &idx2);
	}
	if (res2 > res1) {
		*index = idx2 | 0x0800; /* attribute bit 3 */
		return (res2);
	}
	*index = idx1;
	return (res1);
}

#ifdef WSDISPLAY_SCROLLSUPPORT
static void
vga_scroll(void *v, void *cookie, int lines)
{
	struct vga_config *vc = v;
	struct vgascreen *scr = cookie;
	struct vga_handle *vh = &vc->hdl;

	if (lines == 0) {
		if (scr->pcs.visibleoffset == scr->pcs.dispoffset)
			return;

		scr->pcs.visibleoffset = scr->pcs.dispoffset;
	}
	else {
		int vga_scr_end;
		int margin = scr->pcs.type->ncols * 2;
		int ul, we, p, st;

		vga_scr_end = (scr->pcs.dispoffset + scr->pcs.type->ncols *
		    scr->pcs.type->nrows * 2);
		if (scr->vga_rollover > vga_scr_end + margin) {
			ul = vga_scr_end;
			we = scr->vga_rollover + scr->pcs.type->ncols * 2;
		} else {
			ul = 0;
			we = 0x8000;
		}
		p = (scr->pcs.visibleoffset - ul + we) % we + lines *
		    (scr->pcs.type->ncols * 2);
		st = (scr->pcs.dispoffset - ul + we) % we;
		if (p < margin)
			p = 0;
		if (p > st - margin)
			p = st;
		scr->pcs.visibleoffset = (p + ul) % we;
	}

	vga_6845_write(vh, startadrh, scr->pcs.visibleoffset >> 9);
	vga_6845_write(vh, startadrl, scr->pcs.visibleoffset >> 1);
}
#endif

static void
vga_putchar(void *c, int row, int col, u_int uc, long attr)
{

	pcdisplay_putchar(c, row, col, uc, attr);
}

#ifdef WSDISPLAY_CUSTOM_BORDER
static int
vga_getborder(struct vga_config *vc, u_int *valuep)
{
	struct vga_handle *vh = &vc->hdl;
	u_int idx;
	uint8_t value;

	if (vh->vh_mono)
		return ENODEV;

	value = _vga_attr_read(vh, VGA_ATC_OVERSCAN);
	for (idx = 0; idx < sizeof(fgansitopc); idx++) {
		if (fgansitopc[idx] == value) {
			*valuep = idx;
			return (0);
		}
	}
	return (EIO);
}

static int
vga_setborder(struct vga_config *vc, u_int value)
{
	struct vga_handle *vh = &vc->hdl;

	if (vh->vh_mono)
		return ENODEV;
	if (value >= sizeof(fgansitopc))
		return EINVAL;

	_vga_attr_write(vh, VGA_ATC_OVERSCAN, fgansitopc[value]);
	return (0);
}
#endif /* WSDISPLAY_CUSTOM_BORDER */

void
vga_resume(struct vga_softc *sc)
{
#ifdef VGA_RESET_ON_RESUME
	vga_initregs(&sc->sc_vc->hdl);
#endif
#ifdef PCDISPLAY_SOFTCURSOR
	/* Disable the hardware cursor */
	vga_6845_write(&sc->sc_vc->hdl, curstart, 0x20);
	vga_6845_write(&sc->sc_vc->hdl, curend, 0x00);
#endif
}

static void
vga_save_palette(struct vga_config *vc)
{
	struct vga_handle *vh = &vc->hdl;
	size_t i;
	uint8_t *palette = vc->palette;

	if (vh->vh_mono)
		return;

	vga_raw_write(vh, VGA_DAC_PELMASK, 0xff);
	vga_raw_write(vh, VGA_DAC_ADDRR, 0x00);
	for (i = 0; i < sizeof(vc->palette); i++)
		*palette++ = vga_raw_read(vh, VGA_DAC_PALETTE);

	vga_reset_state(vh);			/* reset flip/flop */
}

static void
vga_restore_palette(struct vga_config *vc)
{
	struct vga_handle *vh = &vc->hdl;
	size_t i;
	uint8_t *palette = vc->palette;

	if (vh->vh_mono)
		return;

	vga_raw_write(vh, VGA_DAC_PELMASK, 0xff);
	vga_raw_write(vh, VGA_DAC_ADDRW, 0x00);
	for (i = 0; i < sizeof(vc->palette); i++)
		vga_raw_write(vh, VGA_DAC_PALETTE, *palette++);

	vga_reset_state(vh);			/* reset flip/flop */
	vga_enable(vh);
}
