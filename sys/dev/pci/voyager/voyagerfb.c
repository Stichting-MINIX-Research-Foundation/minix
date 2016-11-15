/*	$NetBSD: voyagerfb.c,v 1.27 2014/03/11 08:19:45 mrg Exp $	*/

/*
 * Copyright (c) 2009, 2011 Michael Lorenz
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
 * A console driver for Silicon Motion SM502 / Voyager GX  graphics controllers
 * tested on GDIUM only so far
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: voyagerfb.c,v 1.27 2014/03/11 08:19:45 mrg Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/lwp.h>
#include <sys/kauth.h>

#include <dev/videomode/videomode.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>
#include <dev/pci/pciio.h>
#include <dev/ic/sm502reg.h>

#include <dev/wscons/wsdisplayvar.h>
#include <dev/wscons/wsconsio.h>
#include <dev/wsfont/wsfont.h>
#include <dev/rasops/rasops.h>
#include <dev/wscons/wsdisplay_vconsvar.h>
#include <dev/pci/wsdisplay_pci.h>

#include <dev/i2c/i2cvar.h>
#include <dev/pci/voyagervar.h>
#include <dev/wscons/wsdisplay_glyphcachevar.h>

#include "opt_voyagerfb.h"

#ifdef VOYAGERFB_DEBUG
#define DPRINTF aprint_error
#else
#define DPRINTF while (0) printf
#endif

/* XXX these are gdium-specific */
#define GPIO_BACKLIGHT	0x20000000

struct voyagerfb_softc {
	device_t sc_dev;

	pci_chipset_tag_t sc_pc;
	pcitag_t sc_pcitag;
	bus_space_tag_t sc_memt;

	bus_space_handle_t sc_fbh;
	bus_space_handle_t sc_regh;
	bus_addr_t sc_fb, sc_reg;
	bus_size_t sc_fbsize, sc_regsize;

	int sc_width, sc_height, sc_depth, sc_stride;
	int sc_locked;
	void *sc_fbaddr;
	struct vcons_screen sc_console_screen;
	struct wsscreen_descr sc_defaultscreen_descr;
	const struct wsscreen_descr *sc_screens[1];
	struct wsscreen_list sc_screenlist;
	struct vcons_data vd;
	uint8_t *sc_dataport;
	int sc_mode;
	int sc_bl_on, sc_bl_level;
	void *sc_gpio_cookie;

	/* cursor stuff */
	int sc_cur_x;
	int sc_cur_y;
	int sc_hot_x;
	int sc_hot_y;
	uint32_t sc_cursor_addr;
	uint32_t *sc_cursor;
 
	/* colour map */
	u_char sc_cmap_red[256];
	u_char sc_cmap_green[256];
	u_char sc_cmap_blue[256];

	glyphcache sc_gc;
};

static int	voyagerfb_match(device_t, cfdata_t, void *);
static void	voyagerfb_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(voyagerfb, sizeof(struct voyagerfb_softc),
    voyagerfb_match, voyagerfb_attach, NULL, NULL);

extern const u_char rasops_cmap[768];

static int	voyagerfb_ioctl(void *, void *, u_long, void *, int,
			     struct lwp *);
static paddr_t	voyagerfb_mmap(void *, void *, off_t, int);
static void	voyagerfb_init_screen(void *, struct vcons_screen *, int,
		 long *);

static int	voyagerfb_putcmap(struct voyagerfb_softc *,
		 struct wsdisplay_cmap *);
static int 	voyagerfb_getcmap(struct voyagerfb_softc *,
		 struct wsdisplay_cmap *);
static void	voyagerfb_restore_palette(struct voyagerfb_softc *);
static int 	voyagerfb_putpalreg(struct voyagerfb_softc *, int, uint8_t,
			    uint8_t, uint8_t);

static void	voyagerfb_init(struct voyagerfb_softc *);

static void	voyagerfb_rectfill(struct voyagerfb_softc *, int, int, int, int,
			    uint32_t);
static void	voyagerfb_bitblt(void *, int, int, int, int,
			    int, int, int);

static void	voyagerfb_cursor(void *, int, int, int);
static void	voyagerfb_putchar_mono(void *, int, int, u_int, long);
static void	voyagerfb_putchar_aa32(void *, int, int, u_int, long);
static void	voyagerfb_putchar_aa8(void *, int, int, u_int, long);
static void	voyagerfb_copycols(void *, int, int, int, int);
static void	voyagerfb_erasecols(void *, int, int, int, long);
static void	voyagerfb_copyrows(void *, int, int, int);
static void	voyagerfb_eraserows(void *, int, int, long);

static int	voyagerfb_set_curpos(struct voyagerfb_softc *, int, int);
static int	voyagerfb_gcursor(struct voyagerfb_softc *,
		 struct wsdisplay_cursor *);
static int	voyagerfb_scursor(struct voyagerfb_softc *,
		 struct wsdisplay_cursor *);

struct wsdisplay_accessops voyagerfb_accessops = {
	voyagerfb_ioctl,
	voyagerfb_mmap,
	NULL,	/* alloc_screen */
	NULL,	/* free_screen */
	NULL,	/* show_screen */
	NULL, 	/* load_font */
	NULL,	/* pollc */
	NULL	/* scroll */
};

static void	voyagerfb_setup_backlight(struct voyagerfb_softc *);
static void	voyagerfb_brightness_up(device_t);
static void	voyagerfb_brightness_down(device_t);
/* set backlight level */
static void	voyagerfb_set_backlight(struct voyagerfb_softc *, int);
/* turn backlight on and off without messing with the level */
static void	voyagerfb_switch_backlight(struct voyagerfb_softc *, int);

/* wait for FIFO empty so we can feed it another command */
static inline void
voyagerfb_ready(struct voyagerfb_softc *sc)
{
	do {} while ((bus_space_read_4(sc->sc_memt, sc->sc_regh, 
	    SM502_SYSTEM_CTRL) & SM502_SYSCTL_FIFO_EMPTY) == 0);
}

/* wait for the drawing engine to be idle */
static inline void
voyagerfb_wait(struct voyagerfb_softc *sc)
{
	do {} while ((bus_space_read_4(sc->sc_memt, sc->sc_regh, 
	    SM502_SYSTEM_CTRL) & SM502_SYSCTL_ENGINE_BUSY) != 0);
}

static int
voyagerfb_match(device_t parent, cfdata_t match, void *aux)
{
	struct voyager_attach_args *vaa = (struct voyager_attach_args *)aux;

	if (strcmp(vaa->vaa_name, "voyagerfb") == 0) return 100;
	return 0;
}

static void
voyagerfb_attach(device_t parent, device_t self, void *aux)
{
	struct voyagerfb_softc	*sc = device_private(self);
	struct voyager_attach_args	*vaa = aux;
	struct rasops_info	*ri;
	struct wsemuldisplaydev_attach_args aa;
	prop_dictionary_t	dict;
	unsigned long		defattr;
	uint32_t		reg;
	bool			is_console;
	int i, j;
	uint8_t			cmap[768];

	sc->sc_pc = vaa->vaa_pc;
	sc->sc_pcitag = vaa->vaa_pcitag;
	sc->sc_memt = vaa->vaa_tag;
	sc->sc_dev = self;

	aprint_normal("\n");

	dict = device_properties(self);
	prop_dictionary_get_bool(dict, "is_console", &is_console);

	sc->sc_fb = vaa->vaa_mem_pa;
	sc->sc_fbh = vaa->vaa_memh;
	sc->sc_fbaddr = bus_space_vaddr(sc->sc_memt, sc->sc_fbh);

	sc->sc_reg = vaa->vaa_reg_pa;
	sc->sc_regh = vaa->vaa_regh;
	sc->sc_regsize = 2 * 1024 * 1024;
	sc->sc_dataport = bus_space_vaddr(sc->sc_memt, sc->sc_regh);
	sc->sc_dataport += SM502_DATAPORT;

	sc->sc_gpio_cookie = device_private(parent);

	reg = bus_space_read_4(sc->sc_memt, sc->sc_regh, SM502_DRAM_CONTROL);
	switch(reg & 0x0000e000) {
		case SM502_MEM_2M:
			sc->sc_fbsize = 2 * 1024 * 1024;
			break;
		case SM502_MEM_4M:
			sc->sc_fbsize = 4 * 1024 * 1024;
			break;
		case SM502_MEM_8M:
			sc->sc_fbsize = 8 * 1024 * 1024;
			break;
		case SM502_MEM_16M:
			sc->sc_fbsize = 16 * 1024 * 1024;
			break;
		case SM502_MEM_32M:
			sc->sc_fbsize = 32 * 1024 * 1024;
			break;
		case SM502_MEM_64M:
			sc->sc_fbsize = 64 * 1024 * 1024;
			break;
	}

	sc->sc_width = (bus_space_read_4(sc->sc_memt, sc->sc_regh, 	
		SM502_PANEL_FB_WIDTH) & SM502_FBW_WIN_WIDTH_MASK) >> 16;
	sc->sc_height = (bus_space_read_4(sc->sc_memt, sc->sc_regh, 	
		SM502_PANEL_FB_HEIGHT) & SM502_FBH_WIN_HEIGHT_MASK) >> 16;

#ifdef VOYAGERFB_DEPTH_32
	sc->sc_depth = 32;
#else
	sc->sc_depth = 8;
#endif

	printf("%s: %d x %d, %d bit, stride %d\n", device_xname(self), 
		sc->sc_width, sc->sc_height, sc->sc_depth, sc->sc_stride);

	/*
	 * XXX yeah, casting the fb address to uint32_t is formally wrong
	 * but as far as I know there are no SM502 with 64bit BARs
	 */
	aprint_normal("%s: %d MB video memory at 0x%08x\n", device_xname(self),
	    (int)(sc->sc_fbsize >> 20), (uint32_t)sc->sc_fb);

	/* init engine here */
	voyagerfb_init(sc);

	sc->sc_defaultscreen_descr = (struct wsscreen_descr){
		"default",
		0, 0,
		NULL,
		8, 16,
		WSSCREEN_WSCOLORS | WSSCREEN_HILIT,
		NULL
	};
	sc->sc_screens[0] = &sc->sc_defaultscreen_descr;
	sc->sc_screenlist = (struct wsscreen_list){1, sc->sc_screens};
	sc->sc_mode = WSDISPLAYIO_MODE_EMUL;
	sc->sc_locked = 0;

	vcons_init(&sc->vd, sc, &sc->sc_defaultscreen_descr,
	    &voyagerfb_accessops);
	sc->vd.init_screen = voyagerfb_init_screen;

	/* backlight control */
	voyagerfb_setup_backlight(sc);

	ri = &sc->sc_console_screen.scr_ri;

	sc->sc_gc.gc_bitblt = voyagerfb_bitblt;
	sc->sc_gc.gc_blitcookie = sc;
	sc->sc_gc.gc_rop = ROP_COPY;
	if (is_console) {
		vcons_init_screen(&sc->vd, &sc->sc_console_screen, 1,
		    &defattr);
		sc->sc_console_screen.scr_flags |= VCONS_SCREEN_IS_STATIC;

		sc->sc_defaultscreen_descr.textops = &ri->ri_ops;
		sc->sc_defaultscreen_descr.capabilities = ri->ri_caps;
		sc->sc_defaultscreen_descr.nrows = ri->ri_rows;
		sc->sc_defaultscreen_descr.ncols = ri->ri_cols;
	} else {
		if (sc->sc_console_screen.scr_ri.ri_rows == 0) {
			/* do some minimal setup to avoid weirdness later */
			vcons_init_screen(&sc->vd, &sc->sc_console_screen, 1,
			    &defattr);
		} else
			(*ri->ri_ops.allocattr)(ri, 0, 0, 0, &defattr);
	}
	glyphcache_init(&sc->sc_gc, sc->sc_height,
			(sc->sc_fbsize / sc->sc_stride) - sc->sc_height,
			sc->sc_width,
			ri->ri_font->fontwidth,
			ri->ri_font->fontheight,
			defattr);
	if (is_console)
		wsdisplay_cnattach(&sc->sc_defaultscreen_descr, ri, 0, 0,
		    defattr);

	rasops_get_cmap(ri, cmap, sizeof(cmap));
	j = 0;
	if (sc->sc_depth <= 8) {
		for (i = 0; i < 256; i++) {

			sc->sc_cmap_red[i] = cmap[j];
			sc->sc_cmap_green[i] = cmap[j + 1];
			sc->sc_cmap_blue[i] = cmap[j + 2];
			voyagerfb_putpalreg(sc, i, cmap[j], cmap[j + 1],
			    cmap[j + 2]);
			j += 3;
		}
	}

	voyagerfb_rectfill(sc, 0, 0, sc->sc_width, sc->sc_height,
	    ri->ri_devcmap[(defattr >> 16) & 0xff]);

	if (is_console)
		vcons_replay_msgbuf(&sc->sc_console_screen);

	aa.console = is_console;
	aa.scrdata = &sc->sc_screenlist;
	aa.accessops = &voyagerfb_accessops;
	aa.accesscookie = &sc->vd;

	config_found(sc->sc_dev, &aa, wsemuldisplaydevprint);
}

static int
voyagerfb_ioctl(void *v, void *vs, u_long cmd, void *data, int flag,
	struct lwp *l)
{
	struct vcons_data *vd = v;
	struct voyagerfb_softc *sc = vd->cookie;
	struct wsdisplay_fbinfo *wdf;
	struct vcons_screen *ms = vd->active;
	struct wsdisplay_param  *param;

	switch (cmd) {
	case WSDISPLAYIO_GTYPE:
		*(u_int *)data = WSDISPLAY_TYPE_PCIMISC;
		return 0;

	/* PCI config read/write pass through. */
	case PCI_IOC_CFGREAD:
	case PCI_IOC_CFGWRITE:
		return pci_devioctl(sc->sc_pc, sc->sc_pcitag,
		    cmd, data, flag, l);

	case WSDISPLAYIO_GET_BUSID:
		return wsdisplayio_busid_pci(device_parent(sc->sc_dev),
		    sc->sc_pc, sc->sc_pcitag, data);

	case WSDISPLAYIO_GINFO:
		if (ms == NULL)
			return ENODEV;
		wdf = (void *)data;
		wdf->height = ms->scr_ri.ri_height;
		wdf->width = ms->scr_ri.ri_width;
		wdf->depth = 32;
		wdf->cmsize = 256;
		return 0;

	case WSDISPLAYIO_GETCMAP:
		return voyagerfb_getcmap(sc,
		    (struct wsdisplay_cmap *)data);

	case WSDISPLAYIO_PUTCMAP:
		return voyagerfb_putcmap(sc,
		    (struct wsdisplay_cmap *)data);

	case WSDISPLAYIO_LINEBYTES:
		*(u_int *)data = sc->sc_stride;
		return 0;

	case WSDISPLAYIO_SMODE: {
		int new_mode = *(int*)data;
		if (new_mode != sc->sc_mode) {
			sc->sc_mode = new_mode;
			if(new_mode == WSDISPLAYIO_MODE_EMUL) {
#ifdef VOYAGERFB_DEPTH_32
				sc->sc_depth = 32;
#else
				sc->sc_depth = 8;
#endif
				glyphcache_wipe(&sc->sc_gc);
				voyagerfb_init(sc);
				voyagerfb_restore_palette(sc);
				vcons_redraw_screen(ms);
			} else {
				sc->sc_depth = 32;
				voyagerfb_init(sc);
			}
		}
		}
		return 0;

	case WSDISPLAYIO_GVIDEO:
		*(int *)data = sc->sc_bl_on ? WSDISPLAYIO_VIDEO_ON :
					      WSDISPLAYIO_VIDEO_OFF;
		return 0;

	case WSDISPLAYIO_SVIDEO: {
			int new_bl = *(int *)data;

			voyagerfb_switch_backlight(sc,  new_bl);
		}
		return 0;

	case WSDISPLAYIO_GETPARAM:
		param = (struct wsdisplay_param *)data;
		switch (param->param) {
		case WSDISPLAYIO_PARAM_BRIGHTNESS:
			param->min = 0;
			param->max = 255;
			param->curval = sc->sc_bl_level;
			return 0;
		case WSDISPLAYIO_PARAM_BACKLIGHT:
			param->min = 0;
			param->max = 1;
			param->curval = sc->sc_bl_on;
			return 0;
		}
		return EPASSTHROUGH;

	case WSDISPLAYIO_SETPARAM:
		param = (struct wsdisplay_param *)data;
		switch (param->param) {
		case WSDISPLAYIO_PARAM_BRIGHTNESS:
			voyagerfb_set_backlight(sc, param->curval);
			return 0;
		case WSDISPLAYIO_PARAM_BACKLIGHT:
			voyagerfb_switch_backlight(sc,  param->curval);
			return 0;
		}
		return EPASSTHROUGH;

	case WSDISPLAYIO_GCURPOS:
		{
			struct wsdisplay_curpos *pos;

			pos = (struct wsdisplay_curpos *)data;
			pos->x = sc->sc_cur_x;
			pos->y = sc->sc_cur_y;
		}
		return 0;

	case WSDISPLAYIO_SCURPOS:
		{
			struct wsdisplay_curpos *pos;

			pos = (struct wsdisplay_curpos *)data;
			voyagerfb_set_curpos(sc, pos->x, pos->y);
		}
		return 0;

	case WSDISPLAYIO_GCURMAX:
		{
			struct wsdisplay_curpos *pos;

			pos = (struct wsdisplay_curpos *)data;
			pos->x = 64;
			pos->y = 64;
		}
		return 0;

	case WSDISPLAYIO_GCURSOR:
		{
			struct wsdisplay_cursor *cu;

			cu = (struct wsdisplay_cursor *)data;
			return voyagerfb_gcursor(sc, cu);
		}

	case WSDISPLAYIO_SCURSOR:
		{
			struct wsdisplay_cursor *cu;

			cu = (struct wsdisplay_cursor *)data;
			return voyagerfb_scursor(sc, cu);
		}
	}
	return EPASSTHROUGH;
}

static paddr_t
voyagerfb_mmap(void *v, void *vs, off_t offset, int prot)
{
	struct vcons_data *vd = v;
	struct voyagerfb_softc *sc = vd->cookie;
	paddr_t pa;

	/* 'regular' framebuffer mmap()ing */
	if (offset < sc->sc_fbsize) {
		pa = bus_space_mmap(sc->sc_memt, sc->sc_fb + offset, 0, prot,
		    BUS_SPACE_MAP_LINEAR | BUS_SPACE_MAP_PREFETCHABLE);
		return pa;
	}

	/*
	 * restrict all other mappings to processes with privileges
	 */
	if (kauth_authorize_machdep(kauth_cred_get(),
	    KAUTH_MACHDEP_UNMANAGEDMEM, NULL, NULL, NULL, NULL) != 0) {
		aprint_normal("%s: mmap() rejected.\n",
		    device_xname(sc->sc_dev));
		return -1;
	}

	if ((offset >= sc->sc_fb) && (offset < (sc->sc_fb + sc->sc_fbsize))) {
		pa = bus_space_mmap(sc->sc_memt, offset, 0, prot,
		    BUS_SPACE_MAP_LINEAR | BUS_SPACE_MAP_PREFETCHABLE);
		return pa;
	}

	if ((offset >= sc->sc_reg) && 
	    (offset < (sc->sc_reg + sc->sc_regsize))) {
		pa = bus_space_mmap(sc->sc_memt, offset, 0, prot, 0);
		return pa;
	}

	return -1;
}

static void
voyagerfb_init_screen(void *cookie, struct vcons_screen *scr,
    int existing, long *defattr)
{
	struct voyagerfb_softc *sc = cookie;
	struct rasops_info *ri = &scr->scr_ri;

	ri->ri_depth = sc->sc_depth;
	ri->ri_width = sc->sc_width;
	ri->ri_height = sc->sc_height;
	ri->ri_stride = sc->sc_stride;
	ri->ri_flg = RI_CENTER | RI_FULLCLEAR;

	ri->ri_bits = (char *)sc->sc_fbaddr;

	if (existing) {
		ri->ri_flg |= RI_CLEAR;
	}

	if (sc->sc_depth == 8) {
		ri->ri_flg |= RI_8BIT_IS_RGB;
#ifdef VOYAGERFB_ANTIALIAS
		ri->ri_flg |= RI_ENABLE_ALPHA;
#endif
	}
	if (sc->sc_depth == 32) {
#ifdef VOYAGERFB_ANTIALIAS
		ri->ri_flg |= RI_ENABLE_ALPHA;
#endif
		/* we always run in RGB */
		ri->ri_rnum = 8;
		ri->ri_gnum = 8;
		ri->ri_bnum = 8;
		ri->ri_rpos = 16;
		ri->ri_gpos = 8;
		ri->ri_bpos = 0;
	}

	rasops_init(ri, 0, 0);
	ri->ri_caps = WSSCREEN_WSCOLORS;

	rasops_reconfig(ri, sc->sc_height / ri->ri_font->fontheight,
		    sc->sc_width / ri->ri_font->fontwidth);

	ri->ri_hw = scr;
	ri->ri_ops.copyrows = voyagerfb_copyrows;
	ri->ri_ops.copycols = voyagerfb_copycols;
	ri->ri_ops.eraserows = voyagerfb_eraserows;
	ri->ri_ops.erasecols = voyagerfb_erasecols;
	ri->ri_ops.cursor = voyagerfb_cursor;
	if (FONT_IS_ALPHA(ri->ri_font)) {
	        switch (sc->sc_depth) {
	                case 32:
                		ri->ri_ops.putchar = voyagerfb_putchar_aa32;
                		break;
                        case 8:
                                ri->ri_ops.putchar = voyagerfb_putchar_aa8;
                                break;
                        default:
                                printf("alpha font at %d!?\n", sc->sc_depth);
                }
	} else
		ri->ri_ops.putchar = voyagerfb_putchar_mono;
}

static int
voyagerfb_putcmap(struct voyagerfb_softc *sc, struct wsdisplay_cmap *cm)
{
	u_char *r, *g, *b;
	u_int index = cm->index;
	u_int count = cm->count;
	int i, error;
	u_char rbuf[256], gbuf[256], bbuf[256];

#ifdef VOYAGERFB_DEBUG
	aprint_debug("putcmap: %d %d\n",index, count);
#endif
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

	memcpy(&sc->sc_cmap_red[index], &rbuf[index], count);
	memcpy(&sc->sc_cmap_green[index], &gbuf[index], count);
	memcpy(&sc->sc_cmap_blue[index], &bbuf[index], count);

	r = &sc->sc_cmap_red[index];
	g = &sc->sc_cmap_green[index];
	b = &sc->sc_cmap_blue[index];

	for (i = 0; i < count; i++) {
		voyagerfb_putpalreg(sc, index, *r, *g, *b);
		index++;
		r++, g++, b++;
	}
	return 0;
}

static int
voyagerfb_getcmap(struct voyagerfb_softc *sc, struct wsdisplay_cmap *cm)
{
	u_int index = cm->index;
	u_int count = cm->count;
	int error;

	if (index >= 255 || count > 256 || index + count > 256)
		return EINVAL;

	error = copyout(&sc->sc_cmap_red[index],   cm->red,   count);
	if (error)
		return error;
	error = copyout(&sc->sc_cmap_green[index], cm->green, count);
	if (error)
		return error;
	error = copyout(&sc->sc_cmap_blue[index],  cm->blue,  count);
	if (error)
		return error;

	return 0;
}

static void
voyagerfb_restore_palette(struct voyagerfb_softc *sc)
{
	int i;

	for (i = 0; i < 256; i++) {
		voyagerfb_putpalreg(sc, i, sc->sc_cmap_red[i],
		    sc->sc_cmap_green[i], sc->sc_cmap_blue[i]);
	}
}

static int
voyagerfb_putpalreg(struct voyagerfb_softc *sc, int idx, uint8_t r,
    uint8_t g, uint8_t b)
{
	uint32_t reg;

	reg = (r << 16) | (g << 8) | b;
	/* XXX we should probably write the CRT palette too */
	bus_space_write_4(sc->sc_memt, sc->sc_regh,
	    SM502_PALETTE_PANEL + (idx << 2), reg);
	return 0;
}

static void
voyagerfb_init(struct voyagerfb_softc *sc)
{
	int reg;

	voyagerfb_wait(sc);
	/* disable colour compare */
	bus_space_write_4(sc->sc_memt, sc->sc_regh, SM502_COLOR_COMP_MASK, 0);
	/* allow writes to all planes */
	bus_space_write_4(sc->sc_memt, sc->sc_regh, SM502_PLANEMASK,
	    0xffffffff);
	/* disable clipping */
	bus_space_write_4(sc->sc_memt, sc->sc_regh, SM502_CLIP_TOP_LEFT, 0);
	/* source and destination in local memory, no offset */
	bus_space_write_4(sc->sc_memt, sc->sc_regh, SM502_SRC_BASE, 0);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, SM502_DST_BASE, 0);
	/* pitch is screen stride */
	bus_space_write_4(sc->sc_memt, sc->sc_regh, SM502_PITCH,
	    sc->sc_width | (sc->sc_width << 16));
	/* window is screen width */
	bus_space_write_4(sc->sc_memt, sc->sc_regh, SM502_WINDOW_WIDTH,
	    sc->sc_width | (sc->sc_width << 16));
	reg = bus_space_read_4(sc->sc_memt, sc->sc_regh, SM502_PANEL_DISP_CTRL);
	reg &= ~SM502_PDC_DEPTH_MASK;
	
	switch (sc->sc_depth) {
		case 8:
			bus_space_write_4(sc->sc_memt, sc->sc_regh, 
			    SM502_STRETCH, SM502_STRETCH_8BIT);
			sc->sc_stride = sc->sc_width;
			reg |= SM502_PDC_8BIT;
			break;
		case 16:
			bus_space_write_4(sc->sc_memt, sc->sc_regh, 
			    SM502_STRETCH, SM502_STRETCH_16BIT);
			sc->sc_stride = sc->sc_width << 1;
			reg |= SM502_PDC_16BIT;
			break;
		case 24:
		case 32:
			bus_space_write_4(sc->sc_memt, sc->sc_regh, 
			    SM502_STRETCH, SM502_STRETCH_32BIT);
			sc->sc_stride = sc->sc_width << 2;
			reg |= SM502_PDC_32BIT;
			break;
	}
	bus_space_write_4(sc->sc_memt, sc->sc_regh, SM502_PANEL_FB_OFFSET,
	    (sc->sc_stride << 16) | sc->sc_stride);

	/* clear the screen... */
	voyagerfb_rectfill(sc, 0, 0, sc->sc_width, sc->sc_height, 0);

	/* ...and then switch colour depth. For aesthetic reasons. */
	bus_space_write_4(sc->sc_memt, sc->sc_regh, SM502_PANEL_DISP_CTRL,
	    reg);

	/* put the cursor at the end of video memory */
	sc->sc_cursor_addr = 16 * 1024 * 1024 - 16 * 64;	/* XXX */
	DPRINTF("%s: %08x\n", __func__, sc->sc_cursor_addr); 
	sc->sc_cursor = (uint32_t *)((uint8_t *)bus_space_vaddr(sc->sc_memt,
			 sc->sc_fbh) + sc->sc_cursor_addr);
#ifdef VOYAGERFB_DEBUG
	bus_space_write_4(sc->sc_memt, sc->sc_regh, SM502_PANEL_CRSR_XY,
							 0x00100010);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, SM502_PANEL_CRSR_COL12,
							 0x0000ffff);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, SM502_PANEL_CRSR_COL3,
							 0x0000f800);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, SM502_PANEL_CRSR_ADDR,
	    SM502_CRSR_ENABLE | sc->sc_cursor_addr);
	sc->sc_cursor[0] = 0x00000000;
	sc->sc_cursor[1] = 0x00000000;
	sc->sc_cursor[2] = 0xffffffff;
	sc->sc_cursor[3] = 0xffffffff;
	sc->sc_cursor[4] = 0xaaaaaaaa;
	sc->sc_cursor[5] = 0xaaaaaaaa;
	sc->sc_cursor[6] = 0xffffffff;
	sc->sc_cursor[7] = 0x00000000;
#else
	bus_space_write_4(sc->sc_memt, sc->sc_regh, SM502_PANEL_CRSR_ADDR,
	    sc->sc_cursor_addr);
#endif
}

static void
voyagerfb_rectfill(struct voyagerfb_softc *sc, int x, int y, int wi, int he,
     uint32_t colour)
{

	voyagerfb_ready(sc);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, SM502_CONTROL,
	    ROP_COPY |
	    SM502_CTRL_USE_ROP2 |
	    SM502_CTRL_CMD_RECTFILL |
	    SM502_CTRL_QUICKSTART_E);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, SM502_FOREGROUND,
	    colour);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, SM502_DST,
	    (x << 16) | y);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, SM502_DIMENSION,
	    (wi << 16) | he);
}

static void
voyagerfb_bitblt(void *cookie, int xs, int ys, int xd, int yd,
    int wi, int he, int rop)
{
	struct voyagerfb_softc *sc = cookie;
	uint32_t cmd;

	cmd = (rop & 0xf) | SM502_CTRL_USE_ROP2 | SM502_CTRL_CMD_BITBLT |
	      SM502_CTRL_QUICKSTART_E;

	voyagerfb_ready(sc);

	if (xd <= xs) {
		/* left to right */
	} else {
		/*
		 * According to the manual this flag reverses only the blitter's
		 * X direction. At least on my Gdium it also reverses the Y
		 * direction
		 */ 
		cmd |= SM502_CTRL_R_TO_L;
		xs += wi - 1;
		xd += wi - 1;
		ys += he - 1;
		yd += he - 1;
	}
	
	bus_space_write_4(sc->sc_memt, sc->sc_regh, SM502_CONTROL, cmd);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, SM502_SRC,
	    (xs << 16) | ys);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, SM502_DST,
	    (xd << 16) | yd);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, SM502_DIMENSION,
	    (wi << 16) | he);
}

static void
voyagerfb_cursor(void *cookie, int on, int row, int col)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;
	struct voyagerfb_softc *sc = scr->scr_cookie;
	int x, y, wi, he;
	
	wi = ri->ri_font->fontwidth;
	he = ri->ri_font->fontheight;
	
	if (sc->sc_mode == WSDISPLAYIO_MODE_EMUL) {
		x = ri->ri_ccol * wi + ri->ri_xorigin;
		y = ri->ri_crow * he + ri->ri_yorigin;
		if (ri->ri_flg & RI_CURSOR) {
			voyagerfb_bitblt(sc, x, y, x, y, wi, he, ROP_INVERT);
			ri->ri_flg &= ~RI_CURSOR;
		}
		ri->ri_crow = row;
		ri->ri_ccol = col;
		if (on) {
			x = ri->ri_ccol * wi + ri->ri_xorigin;
			y = ri->ri_crow * he + ri->ri_yorigin;
			voyagerfb_bitblt(sc, x, y, x, y, wi, he, ROP_INVERT);
			ri->ri_flg |= RI_CURSOR;
		}
	} else {
		scr->scr_ri.ri_crow = row;
		scr->scr_ri.ri_ccol = col;
		scr->scr_ri.ri_flg &= ~RI_CURSOR;
	}

}

static inline void
voyagerfb_feed8(struct voyagerfb_softc *sc, uint8_t *data, int len)
{
	uint32_t *port = (uint32_t *)sc->sc_dataport;
	int i;

	for (i = 0; i < ((len + 3) & 0xfffc); i++) {
		*port = *data;
		data++;
	}
}

static inline void
voyagerfb_feed16(struct voyagerfb_softc *sc, uint16_t *data, int len)
{
	uint32_t *port = (uint32_t *)sc->sc_dataport;
	int i;

	len = len << 1;
	for (i = 0; i < ((len + 1) & 0xfffe); i++) {
		*port = *data;
		data++;
	}
}

static void
voyagerfb_putchar_mono(void *cookie, int row, int col, u_int c, long attr)
{
	struct rasops_info *ri = cookie;
	struct wsdisplay_font *font = PICK_FONT(ri, c);
	struct vcons_screen *scr = ri->ri_hw;
	struct voyagerfb_softc *sc = scr->scr_cookie;
	uint32_t cmd;
	int fg, bg;
	uint8_t *data;
	int x, y, wi, he;

	if (sc->sc_mode != WSDISPLAYIO_MODE_EMUL)
		return;
		
	if (!CHAR_IN_FONT(c, font))
		return;

	wi = font->fontwidth;
	he = font->fontheight;

	bg = ri->ri_devcmap[(attr >> 16) & 0x0f];
	fg = ri->ri_devcmap[(attr >> 24) & 0x0f];
	x = ri->ri_xorigin + col * wi;
	y = ri->ri_yorigin + row * he;
	if (c == 0x20) {
		voyagerfb_rectfill(sc, x, y, wi, he, bg);
		return;
	}

	data = WSFONT_GLYPH(c, font);

	cmd = ROP_COPY |
	      SM502_CTRL_USE_ROP2 |
	      SM502_CTRL_CMD_HOSTWRT |
	      SM502_CTRL_HOSTBLT_MONO |
	      SM502_CTRL_QUICKSTART_E | 
	      SM502_CTRL_MONO_PACK_32BIT;
	voyagerfb_ready(sc);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, SM502_CONTROL, cmd);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, SM502_FOREGROUND, fg);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, SM502_BACKGROUND, bg);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, SM502_SRC, 0);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, SM502_DST, (x << 16) | y);
	bus_space_write_4(sc->sc_memt, sc->sc_regh,
	    SM502_DIMENSION, (wi << 16) | he);
	/* now feed the data, padded to 32bit */
	switch (ri->ri_font->stride) {
		case 1:
			voyagerfb_feed8(sc, data, ri->ri_fontscale);
			break;
		case 2:
			voyagerfb_feed16(sc, (uint16_t *)data,
			    ri->ri_fontscale);
			break;		
	}	
}

static void
voyagerfb_putchar_aa32(void *cookie, int row, int col, u_int c, long attr)
{
	struct rasops_info *ri = cookie;
	struct wsdisplay_font *font = PICK_FONT(ri, c);
	struct vcons_screen *scr = ri->ri_hw;
	struct voyagerfb_softc *sc = scr->scr_cookie;
	uint32_t cmd;
	int fg, bg;
	uint8_t *data;
	int x, y, wi, he;
	int i, j, r, g, b, aval, pad;
	int rf, gf, bf, rb, gb, bb;
	uint32_t pixel;
	int rv;

	if (sc->sc_mode != WSDISPLAYIO_MODE_EMUL)
		return;
		
	if (!CHAR_IN_FONT(c, font))
		return;

	wi = font->fontwidth;
	he = font->fontheight;

	bg = ri->ri_devcmap[(attr >> 16) & 0x0f];
	fg = ri->ri_devcmap[(attr >> 24) & 0x0f];
	x = ri->ri_xorigin + col * wi;
	y = ri->ri_yorigin + row * he;
	if (c == 0x20) {
		voyagerfb_rectfill(sc, x, y, wi, he, bg);
		return;
	}

	data = WSFONT_GLYPH(c, font);
	/*
	 * we can't accelerate the actual alpha blending but
	 * we can at least use a host blit to go through the
	 * pipeline instead of having to sync the engine
	 */

	rv = glyphcache_try(&sc->sc_gc, c, x, y, attr);
	if (rv == GC_OK)
		return;

	cmd = ROP_COPY |
	      SM502_CTRL_USE_ROP2 |
	      SM502_CTRL_CMD_HOSTWRT |
	      SM502_CTRL_QUICKSTART_E;
	voyagerfb_ready(sc);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, SM502_CONTROL, cmd);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, SM502_SRC, 0);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, SM502_DST, (x << 16) | y);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, SM502_DIMENSION,
	    (wi << 16) | he);
	rf = (fg >> 16) & 0xff;
	rb = (bg >> 16) & 0xff;
	gf = (fg >> 8) & 0xff;
	gb = (bg >> 8) & 0xff;
	bf =  fg & 0xff;
	bb =  bg & 0xff;
	pad = wi & 1;
	for (i = 0; i < he; i++) {
		for (j = 0; j < wi; j++) {
			aval = *data;
			data++;
			if (aval == 0) {
				pixel = bg;
			} else if (aval == 255) {
				pixel = fg;
			} else {
				r = aval * rf + (255 - aval) * rb;
				g = aval * gf + (255 - aval) * gb;
				b = aval * bf + (255 - aval) * bb;
				pixel = (r & 0xff00) << 8 |
				        (g & 0xff00) |
				        (b & 0xff00) >> 8;
			}
			bus_space_write_4(sc->sc_memt, sc->sc_regh,
			    SM502_DATAPORT, pixel);
		}
		if (pad)
			bus_space_write_4(sc->sc_memt, sc->sc_regh,
			    SM502_DATAPORT, 0);
	}
	if (rv == GC_ADD) {
		glyphcache_add(&sc->sc_gc, c, x, y);
	}
}

static void
voyagerfb_putchar_aa8(void *cookie, int row, int col, u_int c, long attr)
{
	struct rasops_info *ri = cookie;
	struct wsdisplay_font *font = PICK_FONT(ri, c);
	struct vcons_screen *scr = ri->ri_hw;
	struct voyagerfb_softc *sc = scr->scr_cookie;
	uint32_t cmd;
	int bg;
	uint8_t *data;
	int x, y, wi, he;
	int i, j, r, g, b, aval, pad;
	int r1, g1, b1, r0, g0, b0, fgo, bgo;
	uint32_t pixel = 0, latch = 0, bg8, fg8;
	int rv;

	if (sc->sc_mode != WSDISPLAYIO_MODE_EMUL)
		return;
		
	if (!CHAR_IN_FONT(c, font))
		return;

	wi = font->fontwidth;
	he = font->fontheight;

	bg = ri->ri_devcmap[(attr >> 16) & 0x0f];
	x = ri->ri_xorigin + col * wi;
	y = ri->ri_yorigin + row * he;
	if (c == 0x20) {
		voyagerfb_rectfill(sc, x, y, wi, he, bg);
		return;
	}

	data = WSFONT_GLYPH(c, font);
	/*
	 * we can't accelerate the actual alpha blending but
	 * we can at least use a host blit to go through the
	 * pipeline instead of having to sync the engine
	 */

	rv = glyphcache_try(&sc->sc_gc, c, x, y, attr);
	if (rv == GC_OK)
		return;

	cmd = ROP_COPY |
	      SM502_CTRL_USE_ROP2 |
	      SM502_CTRL_CMD_HOSTWRT |
	      SM502_CTRL_QUICKSTART_E;
	voyagerfb_ready(sc);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, SM502_CONTROL, cmd);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, SM502_SRC, 0);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, SM502_DST, (x << 16) | y);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, SM502_DIMENSION,
	    (wi << 16) | he);

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

	pad = wi & 4;
	for (i = 0; i < he; i++) {
		for (j = 0; j < wi; j++) {
			aval = *data;
			data++;
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
				bus_space_write_4(sc->sc_memt, sc->sc_regh,
				    SM502_DATAPORT, be32toh(latch));
				latch = 0;
			}
		}
		/* if we have pixels left in latch write them out */
		if ((j & 3) != 0) {
			latch = latch << ((4 - (i & 3)) << 3);	
			bus_space_write_4(sc->sc_memt, sc->sc_regh,
			    SM502_DATAPORT, be32toh(latch));
		}
		if (pad)
			bus_space_write_4(sc->sc_memt, sc->sc_regh,
			    SM502_DATAPORT, 0);
	}
	if (rv == GC_ADD) {
		glyphcache_add(&sc->sc_gc, c, x, y);
	}
}

static void
voyagerfb_copycols(void *cookie, int row, int srccol, int dstcol, int ncols)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;
	struct voyagerfb_softc *sc = scr->scr_cookie;
	int32_t xs, xd, y, width, height;
	
	if ((sc->sc_locked == 0) && (sc->sc_mode == WSDISPLAYIO_MODE_EMUL)) {
		xs = ri->ri_xorigin + ri->ri_font->fontwidth * srccol;
		xd = ri->ri_xorigin + ri->ri_font->fontwidth * dstcol;
		y = ri->ri_yorigin + ri->ri_font->fontheight * row;
		width = ri->ri_font->fontwidth * ncols;
		height = ri->ri_font->fontheight;
		voyagerfb_bitblt(sc, xs, y, xd, y, width, height, ROP_COPY);
	}
}

static void
voyagerfb_erasecols(void *cookie, int row, int startcol, int ncols,
     long fillattr)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;
	struct voyagerfb_softc *sc = scr->scr_cookie;
	int32_t x, y, width, height, fg, bg, ul;
	
	if ((sc->sc_locked == 0) && (sc->sc_mode == WSDISPLAYIO_MODE_EMUL)) {
		x = ri->ri_xorigin + ri->ri_font->fontwidth * startcol;
		y = ri->ri_yorigin + ri->ri_font->fontheight * row;
		width = ri->ri_font->fontwidth * ncols;
		height = ri->ri_font->fontheight;
		rasops_unpack_attr(fillattr, &fg, &bg, &ul);

		voyagerfb_rectfill(sc, x, y, width, height, ri->ri_devcmap[bg]);
	}
}

static void
voyagerfb_copyrows(void *cookie, int srcrow, int dstrow, int nrows)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;
	struct voyagerfb_softc *sc = scr->scr_cookie;
	int32_t x, ys, yd, width, height;
	int i;

	if ((sc->sc_locked == 0) && (sc->sc_mode == WSDISPLAYIO_MODE_EMUL)) {
		x = ri->ri_xorigin;
		ys = ri->ri_yorigin + ri->ri_font->fontheight * srcrow;
		yd = ri->ri_yorigin + ri->ri_font->fontheight * dstrow;
		width = ri->ri_emuwidth;
		height = ri->ri_font->fontheight * nrows;
		if ((nrows > 1) && (dstrow > srcrow)) {
			/*
			 * the blitter can't do bottom-up copies so we have
			 * to copy line by line here
			 * should probably use a command sequence
			 */
			ys += (height - ri->ri_font->fontheight);
			yd += (height - ri->ri_font->fontheight);
			for (i = 0; i < nrows; i++) {
				voyagerfb_bitblt(sc, x, ys, x, yd, width, 
				    ri->ri_font->fontheight, ROP_COPY);
				ys -= ri->ri_font->fontheight;
				yd -= ri->ri_font->fontheight;
			}
		} else
			voyagerfb_bitblt(sc, x, ys, x, yd, width, height, 
			    ROP_COPY);
	}
}

static void
voyagerfb_eraserows(void *cookie, int row, int nrows, long fillattr)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;
	struct voyagerfb_softc *sc = scr->scr_cookie;
	int32_t x, y, width, height, fg, bg, ul;
	
	if ((sc->sc_locked == 0) && (sc->sc_mode == WSDISPLAYIO_MODE_EMUL)) {
		x = ri->ri_xorigin;
		y = ri->ri_yorigin + ri->ri_font->fontheight * row;
		width = ri->ri_emuwidth;
		height = ri->ri_font->fontheight * nrows;
		rasops_unpack_attr(fillattr, &fg, &bg, &ul);

		voyagerfb_rectfill(sc, x, y, width, height, ri->ri_devcmap[bg]);
	}
}

/* backlight control */
static void
voyagerfb_setup_backlight(struct voyagerfb_softc *sc)
{
	/* switch the pin to gpio mode if it isn't already */
	voyager_control_gpio(sc->sc_gpio_cookie, ~GPIO_BACKLIGHT, 0);
	/* turn it on */
	voyager_write_gpio(sc->sc_gpio_cookie, 0xffffffff, GPIO_BACKLIGHT);
	sc->sc_bl_on = 1;
	sc->sc_bl_level = 255;
	pmf_event_register(sc->sc_dev, PMFE_DISPLAY_BRIGHTNESS_UP,
	    voyagerfb_brightness_up, TRUE);
	pmf_event_register(sc->sc_dev, PMFE_DISPLAY_BRIGHTNESS_DOWN,
	    voyagerfb_brightness_down, TRUE);
}

static void
voyagerfb_set_backlight(struct voyagerfb_softc *sc, int level)
{

	/*
	 * should we do nothing when backlight is off, should we just store the
	 * level and use it when turning back on or should we just flip sc_bl_on
	 * and turn the backlight on?
	 * For now turn it on so a crashed screensaver can't get the user stuck
	 * with a dark screen as long as hotkeys work
	 */
	if (level > 255) level = 255;
	if (level < 0) level = 0;
	if (level == sc->sc_bl_level)
		return;
	sc->sc_bl_level = level;
	if (sc->sc_bl_on == 0)
		sc->sc_bl_on = 1;
	/* and here we would actually muck with the hardware */
	if ((level == 0) || (level == 255)) {
		/* in these cases bypass the PWM and use the gpio */
		voyager_control_gpio(sc->sc_gpio_cookie, ~GPIO_BACKLIGHT, 0);
		if (level == 0) {
			voyager_write_gpio(sc->sc_gpio_cookie,
			    ~GPIO_BACKLIGHT, 0);
		} else {
			voyager_write_gpio(sc->sc_gpio_cookie,
			    0xffffffff, GPIO_BACKLIGHT);
		}
	} else {
		uint32_t pwm;

		pwm = voyager_set_pwm(20000, level * 1000 / 256);
		pwm |= SM502_PWM_ENABLE;
		bus_space_write_4(sc->sc_memt, sc->sc_regh, SM502_PWM0, pwm);

		/* let the PWM take over */
		voyager_control_gpio(sc->sc_gpio_cookie,
		    0xffffffff, GPIO_BACKLIGHT);
	}
}

static void
voyagerfb_switch_backlight(struct voyagerfb_softc *sc, int on)
{

	if (on == sc->sc_bl_on)
		return;
	sc->sc_bl_on = on;
	if (on) {
		int level = sc->sc_bl_level;

		sc->sc_bl_level = -1;
		voyagerfb_set_backlight(sc, level);
	} else {
		voyager_control_gpio(sc->sc_gpio_cookie, ~GPIO_BACKLIGHT, 0);
		voyager_write_gpio(sc->sc_gpio_cookie, ~GPIO_BACKLIGHT, 0);
	}
}
	

static void
voyagerfb_brightness_up(device_t dev)
{
	struct voyagerfb_softc *sc = device_private(dev);

	voyagerfb_set_backlight(sc, sc->sc_bl_level + 8);
}

static void
voyagerfb_brightness_down(device_t dev)
{
	struct voyagerfb_softc *sc = device_private(dev);

	voyagerfb_set_backlight(sc, sc->sc_bl_level - 8);
}

static int
voyagerfb_set_curpos(struct voyagerfb_softc *sc, int x, int y)
{
	uint32_t val;
	int xx, yy;

	sc->sc_cur_x = x;
	sc->sc_cur_y = y;

	xx = x - sc->sc_hot_x;
	yy = y - sc->sc_hot_y;
	
	if (xx < 0) xx = abs(xx) | 0x800;
	if (yy < 0) yy = abs(yy) | 0x800;
	
	val = (xx & 0xffff) | (yy << 16);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, SM502_PANEL_CRSR_XY, val);

	return 0;
}

static int
voyagerfb_gcursor(struct voyagerfb_softc *sc, struct wsdisplay_cursor *cur)
{
	/* do nothing for now */
	return 0;
}

static int
voyagerfb_scursor(struct voyagerfb_softc *sc, struct wsdisplay_cursor *cur)
{
	if (cur->which & WSDISPLAY_CURSOR_DOCUR) {

		bus_space_write_4(sc->sc_memt, sc->sc_regh,
		    SM502_PANEL_CRSR_ADDR,
		    sc->sc_cursor_addr | (cur->enable ? SM502_CRSR_ENABLE : 0));
		DPRINTF("%s: %08x\n", __func__, sc->sc_cursor_addr);
	}
	if (cur->which & WSDISPLAY_CURSOR_DOHOT) {

		sc->sc_hot_x = cur->hot.x;
		sc->sc_hot_y = cur->hot.y;
	}
	if (cur->which & WSDISPLAY_CURSOR_DOPOS) {

		voyagerfb_set_curpos(sc, cur->pos.x, cur->pos.y);
	}
	if (cur->which & WSDISPLAY_CURSOR_DOCMAP) {
		int i, idx;
		uint32_t val;
	
		for (i = 0; i < cur->cmap.count; i++) {
			val = ((cur->cmap.red[i] & 0xf8) << 8) |
			      ((cur->cmap.green[i] & 0xfc) << 3) |
			      ((cur->cmap.blue[i] & 0xf8) >> 3);
			idx = i + cur->cmap.index;
			bus_space_write_2(sc->sc_memt, sc->sc_regh,
			    SM502_PANEL_CRSR_COL12 + (idx << 1),
			    val);
			/*
			 * if userland doesn't try to set the 3rd colour we
			 * assume it expects an X11-style 2 colour cursor
			 * X should be our main user anyway
			 */
			if ((idx == 1) && 
			   ((cur->cmap.count + cur->cmap.index) < 3)) {
				bus_space_write_2(sc->sc_memt, sc->sc_regh,
				    SM502_PANEL_CRSR_COL3,
				    val);
			}
			DPRINTF("%s: %d %04x\n", __func__, i + cur->cmap.index,
			    val);
		}
	}
	if (cur->which & WSDISPLAY_CURSOR_DOSHAPE) {

		int i, j, cnt = 0;
		uint32_t latch = 0, omask;
		uint8_t imask;
		DPRINTF("%s: %d %d\n", __func__, cur->size.x, cur->size.y);
		for (i = 0; i < 256; i++) {
			omask = 0x00000001;
			imask = 0x01;
			cur->image[cnt] &= cur->mask[cnt];
			for (j = 0; j < 8; j++) {
				if (cur->mask[cnt] & imask)
					latch |= omask;
				omask <<= 1;
				if (cur->image[cnt] & imask)
					latch |= omask;
				omask <<= 1;
				imask <<= 1;
			}
			cnt++;
			imask = 0x01;
			cur->image[cnt] &= cur->mask[cnt];
			for (j = 0; j < 8; j++) {
				if (cur->mask[cnt] & imask)
					latch |= omask;
				omask <<= 1;
				if (cur->image[cnt] & imask)
					latch |= omask;
				omask <<= 1;
				imask <<= 1;
			}
			cnt++;
			sc->sc_cursor[i] = latch;
			latch = 0;
		}				
	}
	return 0;
}
