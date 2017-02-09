/*	$NetBSD: hpcfb.c,v 1.60 2015/04/07 01:24:32 ozaki-r Exp $	*/

/*-
 * Copyright (c) 1999
 *         Shin Takemura and PocketBSD Project. All rights reserved.
 * Copyright (c) 2000,2001
 *         SATO Kazumi. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the PocketBSD project
 *	and its contributors.
 * 4. Neither the name of the project nor the names of its contributors
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
 */

/*
 * jump scroll, scroll thread, multiscreen, virtual text vram
 * and hpcfb_emulops functions
 * written by SATO Kazumi.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: hpcfb.c,v 1.60 2015/04/07 01:24:32 ozaki-r Exp $");

#ifdef _KERNEL_OPT
#include "opt_hpcfb.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/signalvar.h>
#include <sys/proc.h>
#include <sys/kthread.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/ioctl.h>

#include <sys/bus.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>
#include <dev/wscons/wscons_callbacks.h>

#include <dev/wsfont/wsfont.h>
#include <dev/rasops/rasops.h>

#include <dev/hpc/hpcfbvar.h>
#include <dev/hpc/hpcfbio.h>

#include "bivideo.h"
#if NBIVIDEO > 0
#include <dev/hpc/bivideovar.h>
#endif

#ifdef FBDEBUG
int	hpcfb_debug = 0;
#define	DPRINTF(arg)	if (hpcfb_debug) printf arg
#else
#define	DPRINTF(arg)	do {} while (/* CONSTCOND */ 0)
#endif

#ifndef HPCFB_MAX_COLUMN
#define HPCFB_MAX_COLUMN 130
#endif /* HPCFB_MAX_COLUMN */
#ifndef HPCFB_MAX_ROW
#define HPCFB_MAX_ROW 80
#endif /* HPCFB_MAX_ROW */

/*
 * currently experimental
#define HPCFB_JUMP
*/

struct hpcfb_vchar {
	u_int c;
	long attr;
};

struct hpcfb_tvrow {
	int maxcol;
	int spacecol;
	struct hpcfb_vchar col[HPCFB_MAX_COLUMN];
};

struct hpcfb_devconfig {
	struct rasops_info	dc_rinfo;	/* rasops information */

	int		dc_blanked;	/* currently had video disabled */
	struct hpcfb_softc *dc_sc;
	int dc_rows;
	int dc_cols;
	struct hpcfb_tvrow *dc_tvram;
	int dc_curx;
	int dc_cury;
#ifdef HPCFB_JUMP
	int dc_min_row;
	int dc_max_row;
	int dc_scroll;
	struct callout dc_scroll_ch;
	int dc_scroll_src;
	int dc_scroll_dst;
	int dc_scroll_num;
#endif /* HPCFB_JUMP */
	volatile int dc_state;
#define HPCFB_DC_CURRENT		0x80000000
#define HPCFB_DC_DRAWING		0x01	/* drawing raster ops */
#define HPCFB_DC_TDRAWING		0x02	/* drawing tvram */
#define HPCFB_DC_SCROLLPENDING		0x04	/* scroll is pending */
#define HPCFB_DC_UPDATE			0x08	/* tvram update */
#define HPCFB_DC_SCRDELAY		0x10	/* scroll time but delay it */
#define HPCFB_DC_SCRTHREAD		0x20	/* in scroll thread or callout */
#define HPCFB_DC_UPDATEALL		0x40	/* need to redraw all */
#define HPCFB_DC_ABORT			0x80	/* abort redrawing */
#define	HPCFB_DC_SWITCHREQ		0x100	/* switch request exist */
	int	dc_memsize;
	u_char *dc_fbaddr;
};

#define IS_DRAWABLE(dc) \
	(((dc)->dc_state&HPCFB_DC_CURRENT)&& \
	 (((dc)->dc_state&(HPCFB_DC_DRAWING|HPCFB_DC_SWITCHREQ)) == 0))

#define HPCFB_MAX_SCREEN 5
#define HPCFB_MAX_JUMP 5

struct hpcfb_softc {
	device_t sc_dev;
	struct	hpcfb_devconfig *sc_dc;	/* device configuration */
	const struct hpcfb_accessops	*sc_accessops;
	void *sc_accessctx;
	device_t sc_wsdisplay;
	int sc_screen_resumed;
	int sc_polling;
	int sc_mapping;
	struct proc *sc_thread;
	void *sc_wantedscreen;
	void (*sc_switchcb)(void *, int, int);
	void *sc_switchcbarg;
	struct callout sc_switch_callout;
	int sc_nfbconf;
	struct hpcfb_fbconf *sc_fbconflist;
};

/*
 *  function prototypes
 */
int	hpcfbmatch(device_t, cfdata_t, void *);
void	hpcfbattach(device_t, device_t, void *);
int	hpcfbprint(void *, const char *);

int	hpcfb_ioctl(void *, void *, u_long, void *, int, struct lwp *);
paddr_t	hpcfb_mmap(void *, void *, off_t, int);

void	hpcfb_refresh_screen(struct hpcfb_softc *);
void	hpcfb_doswitch(struct hpcfb_softc *);

#ifdef HPCFB_JUMP
static void	hpcfb_thread(void *);
#endif /* HPCFB_JUMP */

static int	hpcfb_init(struct hpcfb_fbconf *, struct hpcfb_devconfig *);
static int	hpcfb_alloc_screen(void *, const struct wsscreen_descr *,
		    void **, int *, int *, long *);
static void	hpcfb_free_screen(void *, void *);
static int	hpcfb_show_screen(void *, void *, int,
		    void (*) (void *, int, int), void *);
static void     hpcfb_pollc(void *, int);
static void	hpcfb_cmap_reorder(struct hpcfb_fbconf *,
		    struct hpcfb_devconfig *);

static void	hpcfb_power(int, void *);
static bool	hpcfb_suspend(device_t, const pmf_qual_t *);
static bool	hpcfb_resume(device_t, const pmf_qual_t *);


void    hpcfb_cursor(void *, int, int, int);
int     hpcfb_mapchar(void *, int, unsigned int *);
void    hpcfb_putchar(void *, int, int, u_int, long);
void    hpcfb_copycols(void *, int, int, int, int);
void    hpcfb_erasecols(void *, int, int, int, long);
void    hpcfb_redraw(void *, int, int, int);
void    hpcfb_copyrows(void *, int, int, int);
void    hpcfb_eraserows(void *, int, int, long);
int     hpcfb_allocattr(void *, int, int, int, long *);
void    hpcfb_cursor_raw(void *, int, int, int);

#ifdef HPCFB_JUMP
void	hpcfb_update(void *);
void	hpcfb_do_scroll(void *);
void	hpcfb_check_update(void *);
#endif /* HPCFB_JUMP */

struct wsdisplay_emulops hpcfb_emulops = {
	.cursor		= hpcfb_cursor,
	.mapchar	= hpcfb_mapchar,
	.putchar	= hpcfb_putchar,
	.copycols	= hpcfb_copycols,
	.erasecols	= hpcfb_erasecols,
	.copyrows	= hpcfb_copyrows,
	.eraserows	= hpcfb_eraserows,
	.allocattr	= hpcfb_allocattr,
	.replaceattr	= NULL,
};

/*
 *  static variables
 */
CFATTACH_DECL_NEW(hpcfb, sizeof(struct hpcfb_softc),
    hpcfbmatch, hpcfbattach, NULL, NULL);

struct wsscreen_descr hpcfb_stdscreen = {
	.name		= "std",
	.textops	= &hpcfb_emulops, /* XXX */
	.capabilities	= WSSCREEN_REVERSE,
	/* XXX: ncols/nrows will be filled in -- shouldn't, they are global */
};

const struct wsscreen_descr *_hpcfb_scrlist[] = {
	&hpcfb_stdscreen,
	/* XXX other formats, graphics screen? */
};

struct wsscreen_list hpcfb_screenlist = {
	.nscreens = __arraycount(_hpcfb_scrlist),
	.screens = _hpcfb_scrlist,
};

struct wsdisplay_accessops hpcfb_accessops = {
	.ioctl		= hpcfb_ioctl,
	.mmap		= hpcfb_mmap,
	.alloc_screen	= hpcfb_alloc_screen,
	.free_screen	= hpcfb_free_screen,
	.show_screen	= hpcfb_show_screen,
	.load_font	= NULL,
	.pollc		= hpcfb_pollc,
	.scroll		= NULL,
};

void    hpcfb_tv_putchar(struct hpcfb_devconfig *, int, int, u_int, long);
void    hpcfb_tv_copycols(struct hpcfb_devconfig *, int, int, int, int);
void    hpcfb_tv_erasecols(struct hpcfb_devconfig *, int, int, int, long);
void    hpcfb_tv_copyrows(struct hpcfb_devconfig *, int, int, int);
void    hpcfb_tv_eraserows(struct hpcfb_devconfig *, int, int, long);

struct wsdisplay_emulops rasops_emul;

static int hpcfbconsole;
struct hpcfb_devconfig hpcfb_console_dc;
struct wsscreen_descr hpcfb_console_wsscreen;
struct hpcfb_tvrow hpcfb_console_tvram[HPCFB_MAX_ROW];

/*
 *  function bodies
 */

int
hpcfbmatch(device_t parent, cfdata_t match, void *aux)
{
	return (1);
}

void
hpcfbattach(device_t parent, device_t self, void *aux)
{
	struct hpcfb_softc *sc;
	struct hpcfb_attach_args *ha = aux;
	struct wsemuldisplaydev_attach_args wa;

	sc = device_private(self);
	sc->sc_dev = self;

	sc->sc_accessops = ha->ha_accessops;
	sc->sc_accessctx = ha->ha_accessctx;
	sc->sc_nfbconf = ha->ha_nfbconf;
	sc->sc_fbconflist = ha->ha_fbconflist;

	if (hpcfbconsole) {
		sc->sc_dc = &hpcfb_console_dc;
		sc->sc_dc->dc_rinfo.ri_flg &= ~RI_NO_AUTO;
		hpcfb_console_dc.dc_sc = sc;
		printf(": %dx%d pixels, %d colors, %dx%d chars",
		    sc->sc_dc->dc_rinfo.ri_width,sc->sc_dc->dc_rinfo.ri_height,
		    (1 << sc->sc_dc->dc_rinfo.ri_depth),
		    sc->sc_dc->dc_rinfo.ri_cols,sc->sc_dc->dc_rinfo.ri_rows);
		/* Set video chip dependent CLUT if any. */
		if (sc->sc_accessops->setclut)
			sc->sc_accessops->setclut(sc->sc_accessctx,
			    &hpcfb_console_dc.dc_rinfo);
	}
	printf("\n");

	sc->sc_polling = 0; /* XXX */
	sc->sc_mapping = 0; /* XXX */
	callout_init(&sc->sc_switch_callout, 0);

	wa.console = hpcfbconsole;
	wa.scrdata = &hpcfb_screenlist;
	wa.accessops = &hpcfb_accessops;
	wa.accesscookie = sc;

	sc->sc_wsdisplay = config_found(self, &wa, wsemuldisplaydevprint);

#ifdef HPCFB_JUMP
	/*
	 * Create a kernel thread to scroll,
	 */
	if (kthread_create(PRI_NONE, 0, NULL, hpcfb_thread, sc,
	    &sc->sc_thread, "%s", device_xname(sc->sc_dev)) != 0) {
		/*
		 * We were unable to create the HPCFB thread; bail out.
		 */
		sc->sc_thread = 0;
		aprint_error_dev(sc->sc_dev, "unable to create thread, kernel "
		    "hpcfb scroll support disabled\n");
	}
#endif /* HPCFB_JUMP */

	if (!pmf_device_register(self, hpcfb_suspend, hpcfb_resume))
		aprint_error_dev(self, "unable to establish power handler\n");
}

#ifdef HPCFB_JUMP
void
hpcfb_thread(void *arg)
{
	struct hpcfb_softc *sc = arg;

	/*
	 * Loop forever, doing a periodic check for update events.
	 */
	for (;;) {
		/* HPCFB_LOCK(sc); */
		sc->sc_dc->dc_state |= HPCFB_DC_SCRTHREAD;
		if (!sc->sc_mapping) /* draw only EMUL mode */
			hpcfb_update(sc->sc_dc);
		sc->sc_dc->dc_state &= ~HPCFB_DC_SCRTHREAD;
		/* APM_UNLOCK(sc); */
		(void) tsleep(sc, PWAIT, "hpcfb",  (8 * hz) / 7 / 10);
	}
}
#endif /* HPCFB_JUMP */

/* Print function (for parent devices). */
int
hpcfbprint(void *aux, const char *pnp)
{
	if (pnp)
		aprint_normal("hpcfb at %s", pnp);

	return (UNCONF);
}

int
hpcfb_cnattach(struct hpcfb_fbconf *fbconf)
{
#if NBIVIDEO > 0
	struct hpcfb_fbconf __fbconf;
#endif
	long defattr;

	DPRINTF(("%s(%d): hpcfb_cnattach()\n", __FILE__, __LINE__));
#if NBIVIDEO > 0
	if (fbconf == NULL) {
		memset(&__fbconf, 0, sizeof(struct hpcfb_fbconf));
		if (bivideo_getcnfb(&__fbconf) != 0)
			return (ENXIO);
		fbconf = &__fbconf;
	}
#endif /* NBIVIDEO > 0 */
	memset(&hpcfb_console_dc, 0, sizeof(struct hpcfb_devconfig));
	if (hpcfb_init(fbconf, &hpcfb_console_dc) != 0)
		return (ENXIO);
	hpcfb_console_dc.dc_state |= HPCFB_DC_CURRENT;

	hpcfb_console_dc.dc_tvram = hpcfb_console_tvram;
	/* clear screen */
	memset(hpcfb_console_tvram, 0, sizeof(hpcfb_console_tvram));
	hpcfb_redraw(&hpcfb_console_dc, 0, hpcfb_console_dc.dc_rows, 1);

	hpcfb_console_wsscreen = hpcfb_stdscreen;
	hpcfb_console_wsscreen.nrows = hpcfb_console_dc.dc_rows;
	hpcfb_console_wsscreen.ncols = hpcfb_console_dc.dc_cols;
	hpcfb_console_wsscreen.capabilities = hpcfb_console_dc.dc_rinfo.ri_caps;
	hpcfb_allocattr(&hpcfb_console_dc,
			WSCOL_WHITE, WSCOL_BLACK, 0, &defattr);
	wsdisplay_cnattach(&hpcfb_console_wsscreen, &hpcfb_console_dc,
	    0, 0, defattr);
	hpcfbconsole = 1;

	return (0);
}

int
hpcfb_init(struct hpcfb_fbconf *fbconf,	struct hpcfb_devconfig *dc)
{
	struct rasops_info *ri;
	vaddr_t fbaddr;

	fbaddr = (vaddr_t)fbconf->hf_baseaddr;
	dc->dc_fbaddr = (u_char *)fbaddr;

	/* init rasops */
	ri = &dc->dc_rinfo;
	memset(ri, 0, sizeof(struct rasops_info));
	ri->ri_depth = fbconf->hf_pixel_width;
	ri->ri_bits = (void *)fbaddr;
	ri->ri_width = fbconf->hf_width;
	ri->ri_height = fbconf->hf_height;
	ri->ri_stride = fbconf->hf_bytes_per_line;
#if 0
	ri->ri_flg = RI_FORCEMONO | RI_CURSOR;
#else
	ri->ri_flg = RI_CURSOR;
#endif
	if (dc == &hpcfb_console_dc)
		ri->ri_flg |= RI_NO_AUTO;

	switch (ri->ri_depth) {
	case 8:
		if (32 <= fbconf->hf_pack_width &&
		    (fbconf->hf_order_flags & HPCFB_REVORDER_BYTE) &&
		    (fbconf->hf_order_flags & HPCFB_REVORDER_WORD)) {
			ri->ri_flg |= RI_BSWAP;
		}
		break;
	default:
		if (fbconf->hf_order_flags & HPCFB_REVORDER_BYTE) {
#if BYTE_ORDER == BIG_ENDIAN
			ri->ri_flg |= RI_BSWAP;
#endif
		} else {
#if BYTE_ORDER == LITTLE_ENDIAN
			ri->ri_flg |= RI_BSWAP;
#endif
		}
		break;
	}

	if (fbconf->hf_class == HPCFB_CLASS_RGBCOLOR) {
		ri->ri_rnum = fbconf->hf_u.hf_rgb.hf_red_width;
		ri->ri_rpos = fbconf->hf_u.hf_rgb.hf_red_shift;
		ri->ri_gnum = fbconf->hf_u.hf_rgb.hf_green_width;
		ri->ri_gpos = fbconf->hf_u.hf_rgb.hf_green_shift;
		ri->ri_bnum = fbconf->hf_u.hf_rgb.hf_blue_width;
		ri->ri_bpos = fbconf->hf_u.hf_rgb.hf_blue_shift;
	}

	if (rasops_init(ri, HPCFB_MAX_ROW, HPCFB_MAX_COLUMN)) {
		aprint_error_dev(dc->dc_sc->sc_dev, "rasops_init() failed!");
		return -1;
	}

	/* over write color map of rasops */
	hpcfb_cmap_reorder (fbconf, dc);

	dc->dc_curx = -1;
	dc->dc_cury = -1;
	dc->dc_rows = dc->dc_rinfo.ri_rows;
	dc->dc_cols = dc->dc_rinfo.ri_cols;
#ifdef HPCFB_JUMP
	dc->dc_max_row = 0;
	dc->dc_min_row = dc->dc_rows;
	dc->dc_scroll = 0;
	callout_init(&dc->dc_scroll_ch, 0);
#endif /* HPCFB_JUMP */
	dc->dc_memsize = ri->ri_stride * ri->ri_height;
	/* hook rasops in hpcfb_ops */
	rasops_emul = ri->ri_ops; /* struct copy */
	ri->ri_ops = hpcfb_emulops; /* struct copy */

	return (0);
}

static void
hpcfb_cmap_reorder(struct hpcfb_fbconf *fbconf, struct hpcfb_devconfig *dc)
{
	struct rasops_info *ri = &dc->dc_rinfo;
	int reverse = fbconf->hf_access_flags & HPCFB_ACCESS_REVERSE;
	int *cmap = ri->ri_devcmap;
	int i, j, bg, fg, tmp;

	/*
	 * Set forground and background so that the screen
	 * looks black on white.
	 * Normally, black = 00 and white = ff.
	 * HPCFB_ACCESS_REVERSE means black = ff and white = 00.
	 */
	switch (fbconf->hf_pixel_width) {
	case 1:
		/* FALLTHROUGH */
	case 2:
		/* FALLTHROUGH */
	case 4:
		if (reverse) {
			bg = 0;
			fg = ~0;
		} else {
			bg = ~0;
			fg = 0;
		}
		/* for gray-scale LCD, hi-contrast color map */
		cmap[0] = bg;
		for (i = 1; i < 16; i++)
			cmap[i] = fg;
		break;
	case 8:
		/* FALLTHROUGH */
	case 16:
		if (reverse) {
			for (i = 0, j = 15; i < 8; i++, j--) {
				tmp = cmap[i];
				cmap[i] = cmap[j];
				cmap[j] = tmp;
			}
		}
		break;
	}
}

int
hpcfb_ioctl(void *v, void *vs, u_long cmd, void *data, int flag,
	struct lwp *l)
{
	struct hpcfb_softc *sc = v;
	struct hpcfb_devconfig *dc = sc->sc_dc;
	struct wsdisplay_fbinfo *wdf;

	DPRINTF(("hpcfb_ioctl(cmd=0x%lx)\n", cmd));
	switch (cmd) {
	case WSKBDIO_BELL:
		return (0);
		break;

	case WSDISPLAYIO_GTYPE:
		*(u_int *)data = WSDISPLAY_TYPE_HPCFB;
		return (0);

	case WSDISPLAYIO_GINFO:
		wdf = (void *)data;
		wdf->height = dc->dc_rinfo.ri_height;
		wdf->width = dc->dc_rinfo.ri_width;
		wdf->depth = dc->dc_rinfo.ri_depth;
		wdf->cmsize = 256;	/* XXXX */
		return (0);

	case WSDISPLAYIO_LINEBYTES:
		*(u_int *)data = dc->dc_rinfo.ri_stride;
		return 0;

	case WSDISPLAYIO_SMODE:
		if (*(int *)data == WSDISPLAYIO_MODE_EMUL){
			if (sc->sc_mapping){
				sc->sc_mapping = 0;
				if (dc->dc_state&HPCFB_DC_DRAWING)
					dc->dc_state &= ~HPCFB_DC_ABORT;
#ifdef HPCFB_FORCE_REDRAW
				hpcfb_refresh_screen(sc);
#else
				dc->dc_state |= HPCFB_DC_UPDATEALL;
#endif
			}
		} else {
			if (!sc->sc_mapping) {
				sc->sc_mapping = 1;
				dc->dc_state |= HPCFB_DC_ABORT;
			}
			sc->sc_mapping = 1;
		}
		if (sc && sc->sc_accessops->iodone)
			(*sc->sc_accessops->iodone)(sc->sc_accessctx);
		return (0);

	case WSDISPLAYIO_GETCMAP:
	case WSDISPLAYIO_PUTCMAP:
	case WSDISPLAYIO_SVIDEO:
	case WSDISPLAYIO_GVIDEO:
	case WSDISPLAYIO_GETPARAM:
	case WSDISPLAYIO_SETPARAM:
	case HPCFBIO_GCONF:
	case HPCFBIO_SCONF:
	case HPCFBIO_GDSPCONF:
	case HPCFBIO_SDSPCONF:
	case HPCFBIO_GOP:
	case HPCFBIO_SOP:
		return ((*sc->sc_accessops->ioctl)(sc->sc_accessctx,
		    cmd, data, flag, l));

	default:
		if (IOCGROUP(cmd) != 't')
			DPRINTF(("%s(%d): hpcfb_ioctl(%lx, %lx) grp=%c num=%ld\n",
			    __FILE__, __LINE__,
			    cmd, (u_long)data, (char)IOCGROUP(cmd), cmd&0xff));
		break;
	}

	return (EPASSTHROUGH); /* Inappropriate ioctl for device */
}

paddr_t
hpcfb_mmap(void *v, void *vs, off_t offset, int prot)
{
	struct hpcfb_softc *sc = v;

	return ((*sc->sc_accessops->mmap)(sc->sc_accessctx, offset, prot));
}

static void
hpcfb_power(int why, void *arg)
{
	struct hpcfb_softc *sc = arg;

	if (sc->sc_dc == NULL)
		return;	/* You have no screen yet. */

	switch (why) {
	case PWR_STANDBY:
		break;
	case PWR_SOFTSUSPEND: {
		struct wsdisplay_softc *wsc = device_private(sc->sc_wsdisplay);

		sc->sc_screen_resumed = wsdisplay_getactivescreen(wsc);

		if (wsdisplay_switch(sc->sc_wsdisplay,
		    WSDISPLAY_NULLSCREEN, 1 /* waitok */) == 0) {
			wsscreen_switchwait(wsc, WSDISPLAY_NULLSCREEN);
		} else {
			sc->sc_screen_resumed = WSDISPLAY_NULLSCREEN;
		}

		sc->sc_dc->dc_state &= ~HPCFB_DC_CURRENT;
		break;
	    }
	case PWR_SOFTRESUME:
		sc->sc_dc->dc_state |= HPCFB_DC_CURRENT;
		if (sc->sc_screen_resumed != WSDISPLAY_NULLSCREEN)
			wsdisplay_switch(sc->sc_wsdisplay,
			    sc->sc_screen_resumed, 1 /* waitok */);
		break;
	}
}

static bool
hpcfb_suspend(device_t self, const pmf_qual_t *qual)
{
	struct hpcfb_softc *sc = device_private(self);

	hpcfb_power(PWR_SOFTSUSPEND, sc);
	return true;
}

static bool
hpcfb_resume(device_t self, const pmf_qual_t *qual)
{
	struct hpcfb_softc *sc = device_private(self);

	hpcfb_power(PWR_SOFTRESUME, sc);
	return true;
}

void
hpcfb_refresh_screen(struct hpcfb_softc *sc)
{
	struct hpcfb_devconfig *dc = sc->sc_dc;
	int x, y;

	DPRINTF(("hpcfb_refres_screen()\n"));
	if (dc == NULL)
		return;

#ifdef HPCFB_JUMP
	if (dc->dc_state&HPCFB_DC_SCROLLPENDING) {
		dc->dc_state &= ~HPCFB_DC_SCROLLPENDING;
		dc->dc_state &= ~HPCFB_DC_UPDATE;
		callout_stop(&dc->dc_scroll_ch);
	}
#endif /* HPCFB_JUMP */
	/*
	 * refresh screen
	 */
	dc->dc_state &= ~HPCFB_DC_UPDATEALL;
	x = dc->dc_curx;
	y = dc->dc_cury;
	if (0 <= x && 0 <= y)
		hpcfb_cursor_raw(dc, 0,  y, x); /* disable cursor */
	/* redraw all text */
	hpcfb_redraw(dc, 0, dc->dc_rows, 1);
	if (0 <= x && 0 <= y)
		hpcfb_cursor_raw(dc, 1,  y, x); /* enable cursor */
}

static int
hpcfb_alloc_screen(void *v, const struct wsscreen_descr *type,
		   void **cookiep, int *curxp, int *curyp, long *attrp)
{
	struct hpcfb_softc *sc = v;
	struct hpcfb_devconfig *dc;

	DPRINTF(("%s(%d): hpcfb_alloc_screen()\n", __FILE__, __LINE__));

	dc = malloc(sizeof(*dc), M_DEVBUF, M_WAITOK|M_ZERO);
	if (dc == NULL)
		return ENOMEM;

	dc->dc_sc = sc;
	if (hpcfb_init(&sc->sc_fbconflist[0], dc) != 0) {
		free(dc, M_DEVBUF);
		return EINVAL;
	}
	if (sc->sc_accessops->font) {
		sc->sc_accessops->font(sc->sc_accessctx,
		    dc->dc_rinfo.ri_font);
	}
	/* Set video chip dependent CLUT if any. */
	if (sc->sc_accessops->setclut)
		sc->sc_accessops->setclut(sc->sc_accessctx, &dc->dc_rinfo);
	printf("hpcfb: %dx%d pixels, %d colors, %dx%d chars\n",
	    dc->dc_rinfo.ri_width, dc->dc_rinfo.ri_height,
	    (1 << dc->dc_rinfo.ri_depth),
	    dc->dc_rinfo.ri_cols, dc->dc_rinfo.ri_rows);

	/*
	 * XXX, wsdisplay won't reffer the information in wsscreen_descr
	 * structure until alloc_screen will be called, at least, under
	 * current implementation...
	 */
	hpcfb_stdscreen.nrows = dc->dc_rows;
        hpcfb_stdscreen.ncols = dc->dc_cols;
	hpcfb_stdscreen.capabilities = dc->dc_rinfo.ri_caps;

	dc->dc_fbaddr = dc->dc_rinfo.ri_bits;
	dc->dc_rows = dc->dc_rinfo.ri_rows;
	dc->dc_cols = dc->dc_rinfo.ri_cols;
	dc->dc_memsize = dc->dc_rinfo.ri_stride * dc->dc_rinfo.ri_height;

	dc->dc_curx = -1;
	dc->dc_cury = -1;
	dc->dc_tvram = malloc(sizeof(struct hpcfb_tvrow)*dc->dc_rows,
	    M_DEVBUF, M_WAITOK|M_ZERO);
	if (dc->dc_tvram == NULL){
		free(dc, M_DEVBUF);
		return (ENOMEM);
	}

	*curxp = 0;
	*curyp = 0;
	*cookiep = dc;
	hpcfb_allocattr(*cookiep, WSCOL_WHITE, WSCOL_BLACK, 0, attrp);
	DPRINTF(("%s(%d): hpcfb_alloc_screen(): %p\n",
	    __FILE__, __LINE__, dc));

	return (0);
}

static void
hpcfb_free_screen(void *v, void *cookie)
{
	struct hpcfb_devconfig *dc = cookie;

	DPRINTF(("%s(%d): hpcfb_free_screen(%p)\n",
	    __FILE__, __LINE__, cookie));
#ifdef DIAGNOSTIC
	if (dc == &hpcfb_console_dc)
		panic("hpcfb_free_screen: console");
#endif
	free(dc->dc_tvram, M_DEVBUF);
	free(dc, M_DEVBUF);
}

static int
hpcfb_show_screen(void *v, void *cookie, int waitok,
    void (*cb)(void *, int, int), void *cbarg)
{
	struct hpcfb_softc *sc = v;
	struct hpcfb_devconfig *dc = (struct hpcfb_devconfig *)cookie;
	struct hpcfb_devconfig *odc;

	DPRINTF(("%s(%d): hpcfb_show_screen(%p)\n",
	    __FILE__, __LINE__, dc));

	odc = sc->sc_dc;

	if (dc == NULL || odc == dc) {
		hpcfb_refresh_screen(sc);
		return (0);
	}

	if (odc != NULL) {
		odc->dc_state |= HPCFB_DC_SWITCHREQ;

		if ((odc->dc_state&HPCFB_DC_DRAWING) != 0) {
			odc->dc_state |= HPCFB_DC_ABORT;
		}
	}

	sc->sc_wantedscreen = cookie;
	sc->sc_switchcb = cb;
	sc->sc_switchcbarg = cbarg;
	if (cb) {
		callout_reset(&sc->sc_switch_callout, 0,
		    (void(*)(void *))hpcfb_doswitch, sc);
		return (EAGAIN);
	}

	hpcfb_doswitch(sc);
	return (0);
}

void
hpcfb_doswitch(struct hpcfb_softc *sc)
{
	struct hpcfb_devconfig *dc;
	struct hpcfb_devconfig *odc;

	DPRINTF(("hpcfb_doswitch()\n"));
	odc = sc->sc_dc;
	dc = sc->sc_wantedscreen;

	if (!dc) {
		(*sc->sc_switchcb)(sc->sc_switchcbarg, EIO, 0);
		odc->dc_state &= ~HPCFB_DC_SWITCHREQ;
		return;
	}

	if (odc == dc) {
		odc->dc_state &= ~HPCFB_DC_SWITCHREQ;
		return;
	}

	if (odc) {
#ifdef HPCFB_JUMP
		odc->dc_state |= HPCFB_DC_ABORT;
#endif /* HPCFB_JUMP */

		if (odc->dc_curx >= 0 && odc->dc_cury >= 0)
			hpcfb_cursor_raw(odc, 0,  odc->dc_cury, odc->dc_curx);
		/* disable cursor */
		/* disable old screen */
		odc->dc_state &= ~HPCFB_DC_CURRENT;
		/* XXX, This is too dangerous.
		odc->dc_rinfo.ri_bits = NULL;
		*/
	}
	/* switch screen to new one */
	dc->dc_state |= HPCFB_DC_CURRENT;
	dc->dc_state &= ~HPCFB_DC_ABORT;
	dc->dc_rinfo.ri_bits = dc->dc_fbaddr;
	sc->sc_dc = dc;

	/* redraw screen image */
	hpcfb_refresh_screen(sc);

	sc->sc_wantedscreen = NULL;
	if (sc->sc_switchcb)
		(*sc->sc_switchcb)(sc->sc_switchcbarg, 0, 0);

	if (odc != NULL)
		odc->dc_state &= ~HPCFB_DC_SWITCHREQ;
	dc->dc_state &= ~HPCFB_DC_SWITCHREQ;
	return;
}

static void
hpcfb_pollc(void *v, int on)
{
	struct hpcfb_softc *sc = v;

	if (sc == NULL)
		return;
	sc->sc_polling = on;
	if (sc->sc_accessops->iodone)
		(*sc->sc_accessops->iodone)(sc->sc_accessctx);
	if (on) {
		hpcfb_refresh_screen(sc);
		if (sc->sc_accessops->iodone)
			(*sc->sc_accessops->iodone)(sc->sc_accessctx);
	}

	return;
}

/*
 * cursor
 */
void
hpcfb_cursor(void *cookie, int on, int row, int col)
{
	struct hpcfb_devconfig *dc = (struct hpcfb_devconfig *)cookie;

	if (on) {
		dc->dc_curx = col;
		dc->dc_cury = row;
	} else {
		dc->dc_curx = -1;
		dc->dc_cury = -1;
	}

	hpcfb_cursor_raw(cookie, on, row, col);
}

void
hpcfb_cursor_raw(void *cookie, int on, int row, int col)
{
	struct hpcfb_devconfig *dc = (struct hpcfb_devconfig *)cookie;
	struct hpcfb_softc *sc = dc->dc_sc;
	struct rasops_info *ri = &dc->dc_rinfo;
	int curwidth, curheight;
	int xoff, yoff;

#ifdef HPCFB_JUMP
	if (dc->dc_state&HPCFB_DC_SCROLLPENDING) {
		dc->dc_state |= HPCFB_DC_UPDATE;
		return;
	}
#endif /* HPCFB_JUMP */
	if (!IS_DRAWABLE(dc)) {
		return;
	}

	if (ri->ri_bits == NULL)
		return;

	dc->dc_state |= HPCFB_DC_DRAWING;
	if (sc && sc->sc_accessops->cursor) {
		xoff = col * ri->ri_font->fontwidth;
		yoff = row * ri->ri_font->fontheight;
		curheight = ri->ri_font->fontheight;
		curwidth = ri->ri_font->fontwidth;
		(*sc->sc_accessops->cursor)(sc->sc_accessctx,
		    on, xoff, yoff, curwidth, curheight);
	} else
		rasops_emul.cursor(ri, on, row, col);
	dc->dc_state &= ~HPCFB_DC_DRAWING;
}

/*
 * mapchar
 */
int
hpcfb_mapchar(void *cookie, int c, unsigned int *cp)
{
	struct hpcfb_devconfig *dc = (struct hpcfb_devconfig *)cookie;
	struct rasops_info *ri = &dc->dc_rinfo;

	return (rasops_emul.mapchar(ri, c, cp));
}

/*
 * putchar
 */
void
hpcfb_tv_putchar(struct hpcfb_devconfig *dc, int row, int col, u_int uc,
    long attr)
{
	struct hpcfb_tvrow *vscn = dc->dc_tvram;
	struct hpcfb_vchar *vc = &vscn[row].col[col];
	struct hpcfb_vchar *vcb;

	if (vscn == 0)
		return;

	dc->dc_state |= HPCFB_DC_TDRAWING;
#ifdef HPCFB_JUMP
	if (row < dc->dc_min_row)
		dc->dc_min_row = row;
	if (row > dc->dc_max_row)
		dc->dc_max_row = row;

#endif /* HPCFB_JUMP */
	if (vscn[row].maxcol +1 == col)
		vscn[row].maxcol = col;
	else if (vscn[row].maxcol < col) {
		vcb =  &vscn[row].col[vscn[row].maxcol+1];
		memset(vcb, 0,
		    sizeof(struct hpcfb_vchar)*(col-vscn[row].maxcol-1));
		vscn[row].maxcol = col;
	}
	vc->c = uc;
	vc->attr = attr;
	dc->dc_state &= ~HPCFB_DC_TDRAWING;
#ifdef HPCFB_JUMP
	hpcfb_check_update(dc);
#endif /* HPCFB_JUMP */
}

void
hpcfb_putchar(void *cookie, int row, int col, u_int uc, long attr)
{
	struct hpcfb_devconfig *dc = (struct hpcfb_devconfig *)cookie;
	struct hpcfb_softc *sc = dc->dc_sc;
	struct rasops_info *ri = &dc->dc_rinfo;
	int xoff;
	int yoff;
	int fclr, uclr;
	struct wsdisplay_font *font;

	hpcfb_tv_putchar(dc, row, col, uc, attr);
#ifdef HPCFB_JUMP
	if (dc->dc_state&HPCFB_DC_SCROLLPENDING) {
		dc->dc_state |= HPCFB_DC_UPDATE;
		return;
	}
#endif /* HPCFB_JUMP */

	if (!IS_DRAWABLE(dc)) {
		return;
	}

	if (ri->ri_bits == NULL)
		return;

	dc->dc_state |= HPCFB_DC_DRAWING;
	if (sc && sc->sc_accessops->putchar
	    && (dc->dc_state&HPCFB_DC_CURRENT)) {
		font = ri->ri_font;
		yoff = row * ri->ri_font->fontheight;
		xoff =  col * ri->ri_font->fontwidth;
		fclr = ri->ri_devcmap[((u_int)attr >> 24) & 15];
		uclr = ri->ri_devcmap[((u_int)attr >> 16) & 15];

		(*sc->sc_accessops->putchar)(sc->sc_accessctx,
		    xoff, yoff, font, fclr, uclr, uc, attr);
	} else
		rasops_emul.putchar(ri, row, col, uc, attr);
	dc->dc_state &= ~HPCFB_DC_DRAWING;
#ifdef HPCFB_JUMP
	hpcfb_check_update(dc);
#endif /* HPCFB_JUMP */
}

/*
 * copycols
 */
void
hpcfb_tv_copycols(struct hpcfb_devconfig *dc, int row, int srccol, int dstcol,
    int ncols)
{
	struct hpcfb_tvrow *vscn = dc->dc_tvram;
	struct hpcfb_vchar *svc = &vscn[row].col[srccol];
	struct hpcfb_vchar *dvc = &vscn[row].col[dstcol];

	if (vscn == 0)
		return;

	dc->dc_state |= HPCFB_DC_TDRAWING;
#ifdef HPCFB_JUMP
	if (row < dc->dc_min_row)
		dc->dc_min_row = row;
	if (row > dc->dc_max_row)
		dc->dc_max_row = row;
#endif /* HPCFB_JUMP */

	memcpy(dvc, svc, ncols*sizeof(struct hpcfb_vchar));
	if (vscn[row].maxcol < srccol+ncols-1)
		vscn[row].maxcol = srccol+ncols-1;
	if (vscn[row].maxcol < dstcol+ncols-1)
		vscn[row].maxcol = dstcol+ncols-1;
	dc->dc_state &= ~HPCFB_DC_TDRAWING;
#ifdef HPCFB_JUMP
	hpcfb_check_update(dc);
#endif /* HPCFB_JUMP */
}

void
hpcfb_copycols(void *cookie, int row, int srccol, int dstcol, int ncols)
{
	struct hpcfb_devconfig *dc = (struct hpcfb_devconfig *)cookie;
	struct hpcfb_softc *sc = dc->dc_sc;
	struct rasops_info *ri = &dc->dc_rinfo;
	int srcxoff,dstxoff;
	int srcyoff,dstyoff;
	int height, width;

	hpcfb_tv_copycols(dc, row, srccol, dstcol, ncols);
#ifdef HPCFB_JUMP
	if (dc->dc_state&HPCFB_DC_SCROLLPENDING) {
		dc->dc_state |= HPCFB_DC_UPDATE;
		return;
	}
#endif /* HPCFB_JUMP */
	if (!IS_DRAWABLE(dc)) {
		return;
	}

	if (ri->ri_bits == NULL)
		return;

	dc->dc_state |= HPCFB_DC_DRAWING;
	if (sc && sc->sc_accessops->bitblit
	    && (dc->dc_state&HPCFB_DC_CURRENT)) {
		srcxoff = srccol * ri->ri_font->fontwidth;
		srcyoff = row * ri->ri_font->fontheight;
		dstxoff = dstcol * ri->ri_font->fontwidth;
		dstyoff = row * ri->ri_font->fontheight;
		width = ncols * ri->ri_font->fontwidth;
		height = ri->ri_font->fontheight;
		(*sc->sc_accessops->bitblit)(sc->sc_accessctx,
		    srcxoff, srcyoff, dstxoff, dstyoff, height, width);
	} else
		rasops_emul.copycols(ri, row, srccol, dstcol, ncols);
	dc->dc_state &= ~HPCFB_DC_DRAWING;
#ifdef HPCFB_JUMP
	hpcfb_check_update(dc);
#endif /* HPCFB_JUMP */
}


/*
 * erasecols
 */
void
hpcfb_tv_erasecols(struct hpcfb_devconfig *dc,
		   int row, int startcol, int ncols, long attr)
{
	struct hpcfb_tvrow *vscn = dc->dc_tvram;

	if (vscn == 0)
		return;

	dc->dc_state |= HPCFB_DC_TDRAWING;
#ifdef HPCFB_JUMP
	if (row < dc->dc_min_row)
		dc->dc_min_row = row;
	if (row > dc->dc_max_row)
		dc->dc_max_row = row;
#endif /* HPCFB_JUMP */

	vscn[row].maxcol = startcol-1;
	if (vscn[row].spacecol < startcol+ncols-1)
		vscn[row].spacecol = startcol+ncols-1;
	dc->dc_state &= ~HPCFB_DC_TDRAWING;
#ifdef HPCFB_JUMP
	hpcfb_check_update(dc);
#endif /* HPCFB_JUMP */
}

void
hpcfb_erasecols(void *cookie, int row, int startcol, int ncols, long attr)
{
	struct hpcfb_devconfig *dc = (struct hpcfb_devconfig *)cookie;
	struct hpcfb_softc *sc = dc->dc_sc;
	struct rasops_info *ri = &dc->dc_rinfo;
	int xoff, yoff;
	int width, height;

	hpcfb_tv_erasecols(dc, row, startcol, ncols, attr);
#ifdef HPCFB_JUMP
	if (dc->dc_state&HPCFB_DC_SCROLLPENDING) {
		dc->dc_state |= HPCFB_DC_UPDATE;
		return;
	}
#endif /* HPCFB_JUMP */
	if (!IS_DRAWABLE(dc)) {
		return;
	}

	if (ri->ri_bits == NULL)
		return;

	dc->dc_state |= HPCFB_DC_DRAWING;
	if (sc && sc->sc_accessops->erase
	    && (dc->dc_state&HPCFB_DC_CURRENT)) {
		xoff = startcol * ri->ri_font->fontwidth;
		yoff = row * ri->ri_font->fontheight;
		width = ncols * ri->ri_font->fontwidth;
		height = ri->ri_font->fontheight;
		(*sc->sc_accessops->erase)(sc->sc_accessctx,
		    xoff, yoff, height, width, attr);
	} else
		rasops_emul.erasecols(ri, row, startcol, ncols, attr);
	dc->dc_state &= ~HPCFB_DC_DRAWING;
#ifdef HPCFB_JUMP
	hpcfb_check_update(dc);
#endif /* HPCFB_JUMP */
}

/*
 * Copy rows.
 */
void
hpcfb_tv_copyrows(struct hpcfb_devconfig *dc, int src, int dst, int num)
{
	struct hpcfb_tvrow *vscn = dc->dc_tvram;
	struct hpcfb_tvrow *svc = &vscn[src];
	struct hpcfb_tvrow *dvc = &vscn[dst];
	int i;
	int d;

	if (vscn == 0)
		return;

	dc->dc_state |= HPCFB_DC_TDRAWING;
#ifdef HPCFB_JUMP
	if (dst < dc->dc_min_row)
		dc->dc_min_row = dst;
	if (dst + num > dc->dc_max_row)
		dc->dc_max_row = dst + num -1;
#endif /* HPCFB_JUMP */

	if (svc > dvc)
		d = 1;
	else if (svc < dvc) {
		svc += num-1;
		dvc += num-1;
		d = -1;
	} else  {
		dc->dc_state &= ~HPCFB_DC_TDRAWING;
#ifdef HPCFB_JUMP
		hpcfb_check_update(dc);
#endif /* HPCFB_JUMP */
		return;
	}

	for (i = 0; i < num; i++) {
		memcpy(&dvc->col[0], &svc->col[0], sizeof(struct hpcfb_vchar)*(svc->maxcol+1));
		if (svc->maxcol < dvc->maxcol && dvc->spacecol < dvc->maxcol)
			dvc->spacecol = dvc->maxcol;
		dvc->maxcol = svc->maxcol;
		svc+=d;
		dvc+=d;
	}
	dc->dc_state &= ~HPCFB_DC_TDRAWING;
#ifdef HPCFB_JUMP
	hpcfb_check_update(dc);
#endif /* HPCFB_JUMP */
}

void
hpcfb_redraw(void *cookie, int row, int num, int all)
{
	struct hpcfb_devconfig *dc = (struct hpcfb_devconfig *)cookie;
	struct rasops_info *ri = &dc->dc_rinfo;
	int cols;
	struct hpcfb_tvrow *vscn = dc->dc_tvram;
	struct hpcfb_vchar *svc;
	int i, j;

#ifdef HPCFB_JUMP
	if (dc->dc_state&HPCFB_DC_SCROLLPENDING) {
		dc->dc_state |= HPCFB_DC_UPDATE;
		return;
	}
#endif /* HPCFB_JUMP */
	if (dc->dc_sc != NULL
	    && !dc->dc_sc->sc_polling
	    && dc->dc_sc->sc_mapping)
		return;

	dc->dc_state &= ~HPCFB_DC_ABORT;

	if (vscn == 0)
		return;

	if (!IS_DRAWABLE(dc)) {
		return;
	}

	if (ri->ri_bits == NULL)
		return;

	dc->dc_state |= HPCFB_DC_DRAWING;
	dc->dc_state |= HPCFB_DC_TDRAWING;
	for (i = 0; i < num; i++) {
		if (dc->dc_state&HPCFB_DC_ABORT)
			break;
		if ((dc->dc_state&HPCFB_DC_CURRENT) == 0)
			break;
		cols = vscn[row+i].maxcol;
		for (j = 0; j <= cols; j++) {
			if (dc->dc_state&HPCFB_DC_ABORT)
				continue;
			if ((dc->dc_state&HPCFB_DC_CURRENT) == 0)
				continue;
			svc = &vscn[row+i].col[j];
			rasops_emul.putchar(ri, row + i, j, svc->c, svc->attr);
		}
		if (all)
			cols = dc->dc_cols-1;
		else
			cols = vscn[row+i].spacecol;
		for (; j <= cols; j++) {
			if (dc->dc_state&HPCFB_DC_ABORT)
				continue;
			if ((dc->dc_state&HPCFB_DC_CURRENT) == 0)
				continue;
			rasops_emul.putchar(ri, row + i, j, ' ', 0);
		}
		vscn[row+i].spacecol = 0;
	}
	if (dc->dc_state&HPCFB_DC_ABORT)
		dc->dc_state &= ~HPCFB_DC_ABORT;
	dc->dc_state &= ~HPCFB_DC_DRAWING;
	dc->dc_state &= ~HPCFB_DC_TDRAWING;
#ifdef HPCFB_JUMP
	hpcfb_check_update(dc);
#endif /* HPCFB_JUMP */
}

#ifdef HPCFB_JUMP
void
hpcfb_update(void *v)
{
	struct hpcfb_devconfig *dc = (struct hpcfb_devconfig *)v;

	/* callout_stop(&dc->dc_scroll_ch); */
	dc->dc_state &= ~HPCFB_DC_SCROLLPENDING;
	if (dc->dc_curx > 0 && dc->dc_cury > 0)
		hpcfb_cursor_raw(dc, 0,  dc->dc_cury, dc->dc_curx);
	if ((dc->dc_state&HPCFB_DC_UPDATEALL)) {
		hpcfb_redraw(dc, 0, dc->dc_rows, 1);
		dc->dc_state &= ~(HPCFB_DC_UPDATE|HPCFB_DC_UPDATEALL);
	} else if ((dc->dc_state&HPCFB_DC_UPDATE)) {
		hpcfb_redraw(dc, dc->dc_min_row,
		    dc->dc_max_row - dc->dc_min_row, 0);
		dc->dc_state &= ~HPCFB_DC_UPDATE;
	} else {
		hpcfb_redraw(dc, dc->dc_scroll_dst, dc->dc_scroll_num, 0);
	}
	if (dc->dc_curx > 0 && dc->dc_cury > 0)
		hpcfb_cursor_raw(dc, 1,  dc->dc_cury, dc->dc_curx);
}

void
hpcfb_do_scroll(void *v)
{
	struct hpcfb_devconfig *dc = (struct hpcfb_devconfig *)v;

	dc->dc_state |= HPCFB_DC_SCRTHREAD;
	if (dc->dc_state&(HPCFB_DC_DRAWING|HPCFB_DC_TDRAWING))
		dc->dc_state |= HPCFB_DC_SCRDELAY;
	else if (dc->dc_sc != NULL && dc->dc_sc->sc_thread)
		wakeup(dc->dc_sc);
	else if (dc->dc_sc != NULL && !dc->dc_sc->sc_mapping) {
		/* draw only EMUL mode */
		hpcfb_update(v);
	}
	dc->dc_state &= ~HPCFB_DC_SCRTHREAD;
}

void
hpcfb_check_update(void *v)
{
	struct hpcfb_devconfig *dc = (struct hpcfb_devconfig *)v;

	if (dc->dc_sc != NULL
	    && dc->dc_sc->sc_polling
	    && (dc->dc_state&HPCFB_DC_SCROLLPENDING)){
		callout_stop(&dc->dc_scroll_ch);
		dc->dc_state &= ~HPCFB_DC_SCRDELAY;
		hpcfb_update(v);
	}
	else if (dc->dc_state&HPCFB_DC_SCRDELAY){
		dc->dc_state &= ~HPCFB_DC_SCRDELAY;
		hpcfb_update(v);
	} else if (dc->dc_state&HPCFB_DC_UPDATEALL){
		dc->dc_state &= ~HPCFB_DC_UPDATEALL;
		hpcfb_update(v);
	}
}
#endif /* HPCFB_JUMP */

void
hpcfb_copyrows(void *cookie, int src, int dst, int num)
{
	struct hpcfb_devconfig *dc = (struct hpcfb_devconfig *)cookie;
	struct rasops_info *ri = &dc->dc_rinfo;
	struct hpcfb_softc *sc = dc->dc_sc;
	int srcyoff, dstyoff;
	int width, height;

	hpcfb_tv_copyrows(cookie, src, dst, num);

	if (!IS_DRAWABLE(dc)) {
		return;
	}

	if (ri->ri_bits == NULL)
		return;

	if (sc && sc->sc_accessops->bitblit
	    && (dc->dc_state&HPCFB_DC_CURRENT)) {
		dc->dc_state |= HPCFB_DC_DRAWING;
		srcyoff = src * ri->ri_font->fontheight;
		dstyoff = dst * ri->ri_font->fontheight;
		width = dc->dc_cols * ri->ri_font->fontwidth;
		height = num * ri->ri_font->fontheight;
		(*sc->sc_accessops->bitblit)(sc->sc_accessctx,
		    0, srcyoff, 0, dstyoff, height, width);
		dc->dc_state &= ~HPCFB_DC_DRAWING;
	}
	else {
#ifdef HPCFB_JUMP
		if (sc && sc->sc_polling) {
			hpcfb_check_update(dc);
		} else if ((dc->dc_state&HPCFB_DC_SCROLLPENDING) == 0) {
			dc->dc_state |= HPCFB_DC_SCROLLPENDING;
			dc->dc_scroll = 1;
			dc->dc_scroll_src = src;
			dc->dc_scroll_dst = dst;
			dc->dc_scroll_num = num;
			callout_reset(&dc->dc_scroll_ch, hz/100, &hpcfb_do_scroll, dc);
			return;
		} else if (dc->dc_scroll++ < dc->dc_rows/HPCFB_MAX_JUMP) {
			dc->dc_state |= HPCFB_DC_UPDATE;
			return;
		} else {
			dc->dc_state &= ~HPCFB_DC_SCROLLPENDING;
			callout_stop(&dc->dc_scroll_ch);
		}
		if (dc->dc_state&HPCFB_DC_UPDATE) {
			dc->dc_state &= ~HPCFB_DC_UPDATE;
			hpcfb_redraw(cookie, dc->dc_min_row,
			    dc->dc_max_row - dc->dc_min_row, 0);
			dc->dc_max_row = 0;
			dc->dc_min_row = dc->dc_rows;
			if (dc->dc_curx > 0 && dc->dc_cury > 0)
				hpcfb_cursor(dc, 1,  dc->dc_cury, dc->dc_curx);
			return;
		}
#endif /* HPCFB_JUMP */
		hpcfb_redraw(cookie, dst, num, 0);
	}
#ifdef HPCFB_JUMP
	hpcfb_check_update(dc);
#endif /* HPCFB_JUMP */
}

/*
 * eraserows
 */
void
hpcfb_tv_eraserows(struct hpcfb_devconfig *dc,
		   int row, int nrow, long attr)
{
	struct hpcfb_tvrow *vscn = dc->dc_tvram;
	int cols;
	int i;

	if (vscn == 0)
		return;

	dc->dc_state |= HPCFB_DC_TDRAWING;
	dc->dc_state &= ~HPCFB_DC_TDRAWING;
#ifdef HPCFB_JUMP
	if (row < dc->dc_min_row)
		dc->dc_min_row = row;
	if (row + nrow > dc->dc_max_row)
		dc->dc_max_row = row + nrow;
#endif /* HPCFB_JUMP */

	for (i = 0; i < nrow; i++) {
		cols = vscn[row+i].maxcol;
		if (vscn[row+i].spacecol < cols)
			vscn[row+i].spacecol = cols;
		vscn[row+i].maxcol = -1;
	}
#ifdef HPCFB_JUMP
	hpcfb_check_update(dc);
#endif /* HPCFB_JUMP */
}

void
hpcfb_eraserows(void *cookie, int row, int nrow, long attr)
{
	struct hpcfb_devconfig *dc = (struct hpcfb_devconfig *)cookie;
	struct hpcfb_softc *sc = dc->dc_sc;
	struct rasops_info *ri = &dc->dc_rinfo;
	int yoff;
	int width;
	int height;

	hpcfb_tv_eraserows(dc, row, nrow, attr);
#ifdef HPCFB_JUMP
	if (dc->dc_state&HPCFB_DC_SCROLLPENDING) {
		dc->dc_state |= HPCFB_DC_UPDATE;
		return;
	}
#endif /* HPCFB_JUMP */
	if (!IS_DRAWABLE(dc)) {
		return;
	}

	if (ri->ri_bits == NULL)
		return;

	dc->dc_state |= HPCFB_DC_DRAWING;
	if (sc && sc->sc_accessops->erase
	    && (dc->dc_state&HPCFB_DC_CURRENT)) {
		yoff = row * ri->ri_font->fontheight;
		width = dc->dc_cols * ri->ri_font->fontwidth;
		height = nrow * ri->ri_font->fontheight;
		(*sc->sc_accessops->erase)(sc->sc_accessctx,
		    0, yoff, height, width, attr);
	} else
		rasops_emul.eraserows(ri, row, nrow, attr);
	dc->dc_state &= ~HPCFB_DC_DRAWING;
#ifdef HPCFB_JUMP
	hpcfb_check_update(dc);
#endif /* HPCFB_JUMP */
}

/*
 * allocattr
 */
int
hpcfb_allocattr(void *cookie, int fg, int bg, int flags, long *attrp)
{
	struct hpcfb_devconfig *dc = (struct hpcfb_devconfig *)cookie;
	struct rasops_info *ri = &dc->dc_rinfo;

	return (rasops_emul.allocattr(ri, fg, bg, flags, attrp));
}
