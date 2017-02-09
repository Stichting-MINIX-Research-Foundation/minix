/*	$NetBSD: zx.c,v 1.39 2012/01/11 16:08:57 macallan Exp $	*/

/*
 *  Copyright (c) 2002 The NetBSD Foundation, Inc.
 *  All rights reserved.
 *
 *  This code is derived from software contributed to The NetBSD Foundation
 *  by Andrew Doran.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 *  ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 *  TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 *  PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 *  BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Driver for the Sun ZX display adapter.  This would be called 'leo', but
 * NetBSD/amiga already has a driver by that name.  The XFree86 and Linux
 * drivers were used as "living documentation" when writing this; thanks
 * to the authors.
 *
 * Issues (which can be solved with wscons, happily enough):
 *
 * o There is lots of unnecessary mucking about rasops in here, primarily
 *   to appease the sparc fb code.
 *
 * o RASTERCONSOLE is required.  X needs the board set up correctly, and
 *   that's difficult to reconcile with using the PROM for output.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: zx.c,v 1.39 2012/01/11 16:08:57 macallan Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>
#include <sys/mman.h>
#include <sys/tty.h>
#include <sys/conf.h>
#include <sys/syslog.h>
#include <sys/buf.h>
#ifdef DEBUG
/* for log(9) in zxioctl() */
#include <sys/lwp.h>
#include <sys/proc.h>
#endif

#include <sys/bus.h>
#include <machine/autoconf.h>

#include <dev/sun/fbio.h>
#include <dev/sun/fbvar.h>

#include "wsdisplay.h"
#if NWSDISPLAY > 0
#include <dev/wscons/wsconsio.h>
#include <dev/wsfont/wsfont.h>
#include <dev/rasops/rasops.h>
#include <dev/wscons/wsdisplay_vconsvar.h>

#include "opt_wsemul.h"
#endif

#include <dev/sbus/zxreg.h>
#include <dev/sbus/zxvar.h>
#include <dev/sbus/sbusvar.h>

#include <dev/wscons/wsconsio.h>

#include "ioconf.h"

#if (NWSDISPLAY == 0) && !defined(RASTERCONSOLE)
#error Sorry, this driver needs WSCONS or RASTERCONSOLE
#endif

#if (NWSDISPLAY > 0) && defined(RASTERCONSOLE)
#error Sorry, RASTERCONSOLE and WSCONS are mutually exclusive
#endif

#define	ZX_STD_ROP	(ZX_ROP_NEW | ZX_ATTR_WE_ENABLE | \
    ZX_ATTR_OE_ENABLE | ZX_ATTR_FORCE_WID)

static void	zx_attach(device_t, device_t, void *);
static int	zx_match(device_t, cfdata_t, void *);

static void	zx_blank(device_t);
static int	zx_cmap_put(struct zx_softc *);
static void	zx_copyrect(struct zx_softc *, int, int, int, int, int, int);
static int	zx_cross_loadwid(struct zx_softc *, u_int, u_int, u_int);
static int	zx_cross_wait(struct zx_softc *);
static void	zx_fillrect(struct zx_softc *, int, int, int, int, uint32_t, int);
static int	zx_intr(void *);
static void	zx_reset(struct zx_softc *);
static void	zx_unblank(device_t);

static void	zx_cursor_blank(struct zx_softc *);
static void	zx_cursor_color(struct zx_softc *);
static void	zx_cursor_move(struct zx_softc *);
static void	zx_cursor_set(struct zx_softc *);
static void	zx_cursor_unblank(struct zx_softc *);

static void	zx_copycols(void *, int, int, int, int);
static void	zx_copyrows(void *, int, int, int);
static void	zx_do_cursor(void *, int, int, int);
static void	zx_erasecols(void *, int, int, int, long);
static void	zx_eraserows(void *, int, int, long);
static void	zx_putchar(void *, int, int, u_int, long);

struct zx_mmo {
	off_t	mo_va;
	off_t	mo_pa;
	off_t	mo_size;
} static const zx_mmo[] = {
	{ ZX_FB0_VOFF,		ZX_OFF_SS0,		0x00800000 },
	{ ZX_LC0_VOFF,		ZX_OFF_LC_SS0_USR,	0x00001000 },
	{ ZX_LD0_VOFF,		ZX_OFF_LD_SS0,		0x00001000 },
	{ ZX_LX0_CURSOR_VOFF,	ZX_OFF_LX_CURSOR,	0x00001000 },
	{ ZX_FB1_VOFF,		ZX_OFF_SS1,		0x00800000 },
	{ ZX_LC1_VOFF,		ZX_OFF_LC_SS1_USR,	0x00001000 },
	{ ZX_LD1_VOFF,		ZX_OFF_LD_SS1,		0x00001000 },
	{ ZX_LX_KRN_VOFF,	ZX_OFF_LX_CROSS,	0x00001000 },
	{ ZX_LC0_KRN_VOFF,	ZX_OFF_LC_SS0_KRN,	0x00001000 },
	{ ZX_LC1_KRN_VOFF,	ZX_OFF_LC_SS1_KRN,	0x00001000 },
	{ ZX_LD_GBL_VOFF,	ZX_OFF_LD_GBL,		0x00001000 },
};

CFATTACH_DECL_NEW(zx, sizeof(struct zx_softc),
    zx_match, zx_attach, NULL, NULL);

static dev_type_open(zxopen);
static dev_type_close(zxclose);
static dev_type_ioctl(zxioctl);
static dev_type_mmap(zxmmap);

static struct fbdriver zx_fbdriver = {
	zx_unblank, zxopen, zxclose, zxioctl, nopoll, zxmmap
};

#if NWSDISPLAY > 0
struct wsscreen_descr zx_defaultscreen = {
	"std",
	0, 0,	/* will be filled in -- XXX shouldn't, it's global */
		/* doesn't matter - you can't really have more than one leo */
	NULL,		/* textops */
	8, 16,	/* font width/height */
	WSSCREEN_WSCOLORS,	/* capabilities */
	NULL	/* modecookie */
};

static int 	zx_ioctl(void *, void *, u_long, void *, int, struct lwp *);
static paddr_t	zx_mmap(void *, void *, off_t, int);
static void	zx_init_screen(void *, struct vcons_screen *, int, long *);

static int	zx_putcmap(struct zx_softc *, struct wsdisplay_cmap *);
static int	zx_getcmap(struct zx_softc *, struct wsdisplay_cmap *);

struct wsdisplay_accessops zx_accessops = {
	zx_ioctl,
	zx_mmap,
	NULL,	/* alloc_screen */
	NULL,	/* free_screen */
	NULL,	/* show_screen */
	NULL, 	/* load_font */
	NULL,	/* pollc */
	NULL	/* scroll */
};

const struct wsscreen_descr *_zx_scrlist[] = {
	&zx_defaultscreen
};

struct wsscreen_list zx_screenlist = {
	sizeof(_zx_scrlist) / sizeof(struct wsscreen_descr *),
	_zx_scrlist
};


extern const u_char rasops_cmap[768];

static struct vcons_screen zx_console_screen;
#endif /* NWSDISPLAY > 0 */

static int
zx_match(device_t parent, cfdata_t cf, void *aux)
{
	struct sbus_attach_args *sa;

	sa = (struct sbus_attach_args *)aux;

	return (strcmp(sa->sa_name, "SUNW,leo") == 0);
}

static void
zx_attach(device_t parent, device_t self, void *args)
{
	struct zx_softc *sc;
	struct sbus_attach_args *sa;
	bus_space_handle_t bh;
	bus_space_tag_t bt;
	struct fbdevice *fb;
#if NWSDISPLAY > 0
	struct wsemuldisplaydev_attach_args aa;
	struct rasops_info *ri = &zx_console_screen.scr_ri;
	unsigned long defattr;
#endif
	int isconsole, width, height;

	sc = device_private(self);
	sc->sc_dv = self;

	sa = args;
	fb = &sc->sc_fb;
	bt = sa->sa_bustag;
	sc->sc_bt = bt;
	sc->sc_paddr = sbus_bus_addr(bt, sa->sa_slot, sa->sa_offset);

	if (sbus_bus_map(bt, sa->sa_slot, sa->sa_offset + ZX_OFF_SS0,
	    0x800000, BUS_SPACE_MAP_LINEAR | BUS_SPACE_MAP_LARGE, &bh) != 0) {
		aprint_error_dev(self, "can't map bits\n");
		return;
	}
	fb->fb_pixels = (void *)bus_space_vaddr(bt, bh);
	sc->sc_pixels = (uint32_t *)fb->fb_pixels;

	if (sbus_bus_map(bt, sa->sa_slot, sa->sa_offset + ZX_OFF_LC_SS0_USR,
	    PAGE_SIZE, BUS_SPACE_MAP_LINEAR, &bh) != 0) {
		aprint_error_dev(self, "can't map zc\n");
		return;
	}

	sc->sc_bhzc = bh;

	if (sbus_bus_map(bt, sa->sa_slot, sa->sa_offset + ZX_OFF_LD_SS0,
	    PAGE_SIZE, BUS_SPACE_MAP_LINEAR, &bh) != 0) {
		aprint_error_dev(self, "can't map ld/ss0\n");
		return;
	}
	sc->sc_bhzdss0 = bh;

	if (sbus_bus_map(bt, sa->sa_slot, sa->sa_offset + ZX_OFF_LD_SS1,
	    PAGE_SIZE, BUS_SPACE_MAP_LINEAR, &bh) != 0) {
		aprint_error_dev(self, "can't map ld/ss1\n");
		return;
	}
	sc->sc_bhzdss1 = bh;

	if (sbus_bus_map(bt, sa->sa_slot, sa->sa_offset + ZX_OFF_LX_CROSS,
	    PAGE_SIZE, BUS_SPACE_MAP_LINEAR, &bh) != 0) {
		aprint_error_dev(self, "can't map zx\n");
		return;
	}
	sc->sc_bhzx = bh;

	if (sbus_bus_map(bt, sa->sa_slot, sa->sa_offset + ZX_OFF_LX_CURSOR,
	    PAGE_SIZE, BUS_SPACE_MAP_LINEAR, &bh) != 0) {
		aprint_error_dev(self, "can't map zcu\n");
		return;
	}
	sc->sc_bhzcu = bh;

	fb->fb_driver = &zx_fbdriver;
	fb->fb_device = sc->sc_dv;
	fb->fb_flags = device_cfdata(sc->sc_dv)->cf_flags & FB_USERMASK;
	fb->fb_pfour = NULL;
	fb->fb_linebytes = prom_getpropint(sa->sa_node, "linebytes", 8192);

	width = prom_getpropint(sa->sa_node, "width", 1280);
	height = prom_getpropint(sa->sa_node, "height", 1024);
	fb_setsize_obp(fb, 32, width, height, sa->sa_node);

	fb->fb_type.fb_cmsize = 256;
	fb->fb_type.fb_depth = 32;
	fb->fb_type.fb_size = fb->fb_type.fb_height * fb->fb_linebytes;
	fb->fb_type.fb_type = FBTYPE_SUNLEO;

	printf(": %d x %d", fb->fb_type.fb_width, fb->fb_type.fb_height);
	isconsole = fb_is_console(sa->sa_node);
	if (isconsole)
		printf(" (console)");
	printf("\n");

	if (sa->sa_nintr != 0)
		bus_intr_establish(bt, sa->sa_pri, IPL_NONE, zx_intr, sc);

	sc->sc_cmap = malloc(768, M_DEVBUF, M_NOWAIT);
	zx_reset(sc);

#if NWSDISPLAY > 0
	sc->sc_width = fb->fb_type.fb_width;
	sc->sc_stride = 8192; /* 32 bit */
	sc->sc_height = fb->fb_type.fb_height;

	/* setup rasops and so on for wsdisplay */
	wsfont_init();
	sc->sc_mode = WSDISPLAYIO_MODE_EMUL;
	sc->sc_bg = WS_DEFAULT_BG;

	vcons_init(&sc->vd, sc, &zx_defaultscreen, &zx_accessops);
	sc->vd.init_screen = zx_init_screen;

	if (isconsole) {
		/* we mess with zx_console_screen only once */
		vcons_init_screen(&sc->vd, &zx_console_screen, 1,
		    &defattr);
		zx_console_screen.scr_flags |= VCONS_SCREEN_IS_STATIC;
		
		zx_defaultscreen.textops = &ri->ri_ops;
		zx_defaultscreen.capabilities = WSSCREEN_WSCOLORS;
		zx_defaultscreen.nrows = ri->ri_rows;
		zx_defaultscreen.ncols = ri->ri_cols;
		zx_fillrect(sc, 0, 0, width, height,
		     ri->ri_devcmap[defattr >> 16], ZX_STD_ROP);
		wsdisplay_cnattach(&zx_defaultscreen, ri, 0, 0, defattr);
		vcons_replay_msgbuf(&zx_console_screen);
	} else {
		/* 
		 * we're not the console so we just clear the screen and don't 
		 * set up any sort of text display
		 */
		if (zx_defaultscreen.textops == NULL) {
			/* 
			 * ugly, but...
			 * we want the console settings to win, so we only
			 * touch anything when we find an untouched screen
			 * definition. In this case we fill it from fb to
			 * avoid problems in case no zx is the console
			 */
			ri = &sc->sc_fb.fb_rinfo;
			zx_defaultscreen.textops = &ri->ri_ops;
			zx_defaultscreen.capabilities = ri->ri_caps;
			zx_defaultscreen.nrows = ri->ri_rows;
			zx_defaultscreen.ncols = ri->ri_cols;
		}
	}

	aa.scrdata = &zx_screenlist;
	aa.console = isconsole;
	aa.accessops = &zx_accessops;
	aa.accesscookie = &sc->vd;
	config_found(sc->sc_dv, &aa, wsemuldisplaydevprint);
#endif
	fb_attach(&sc->sc_fb, isconsole);
}

static int
zxopen(dev_t dev, int flags, int mode, struct lwp *l)
{

	if (device_lookup(&zx_cd, minor(dev)) == NULL)
		return (ENXIO);
	return (0);
}

static int
zxclose(dev_t dev, int flags, int mode, struct lwp *l)
{
	struct zx_softc *sc;

	sc = device_lookup_private(&zx_cd, minor(dev));

	zx_reset(sc);
	zx_cursor_blank(sc);
	return (0);
}

static int
zxioctl(dev_t dev, u_long cmd, void *data, int flags, struct lwp *l)
{
	struct zx_softc *sc;
	struct fbcmap *cm;
	struct fbcursor *cu;
	uint32_t curbits[2][32];
	int rv, v, count, i;

	sc = device_lookup_private(&zx_cd, minor(dev));

	switch (cmd) {
	case FBIOGTYPE:
		*(struct fbtype *)data = sc->sc_fb.fb_type;
		break;

	case FBIOGATTR:
#define fba ((struct fbgattr *)data)
		fba->real_type = sc->sc_fb.fb_type.fb_type;
		fba->owner = 0;		/* XXX ??? */
		fba->fbtype = sc->sc_fb.fb_type;
		fba->sattr.flags = 0;
		fba->sattr.emu_type = sc->sc_fb.fb_type.fb_type;
		fba->sattr.dev_specific[0] = -1;
		fba->emu_types[0] = sc->sc_fb.fb_type.fb_type;
		fba->emu_types[1] = -1;
		fba->emu_types[2] = -1;
#undef fba
		break;

	case FBIOGVIDEO:
		*(int *)data = ((sc->sc_flags & ZX_BLANKED) != 0);
		break;

	case FBIOSVIDEO:
		if (*(int *)data)
			zx_unblank(sc->sc_dv);
		else
			zx_blank(sc->sc_dv);
		break;

	case FBIOGETCMAP:
		cm = (struct fbcmap *)data;
		if (cm->index > 256 || cm->count > 256 - cm->index)
			return (EINVAL);
		rv = copyout(sc->sc_cmap + cm->index, cm->red, cm->count);
		if (rv == 0)
			rv = copyout(sc->sc_cmap + 256 + cm->index, cm->green,
			    cm->count);
		if (rv == 0)
			rv = copyout(sc->sc_cmap + 512 + cm->index, cm->blue,
			    cm->count);
		return (rv);

	case FBIOPUTCMAP:
		cm = (struct fbcmap *)data;
		if (cm->index > 256 || cm->count > 256 - cm->index)
			return (EINVAL);
		rv = copyin(cm->red, sc->sc_cmap + cm->index, cm->count);
		if (rv == 0)
			rv = copyin(cm->green, sc->sc_cmap + 256 + cm->index,
			    cm->count);
		if (rv == 0)
			rv = copyin(cm->blue, sc->sc_cmap + 512 + cm->index,
			    cm->count);
		zx_cmap_put(sc);
		return (rv);

	case FBIOGCURPOS:
		*(struct fbcurpos *)data = sc->sc_curpos;
		break;

	case FBIOSCURPOS:
		sc->sc_curpos = *(struct fbcurpos *)data;
		zx_cursor_move(sc);
		break;

	case FBIOGCURMAX:
		((struct fbcurpos *)data)->x = 32;
		((struct fbcurpos *)data)->y = 32;
		break;

	case FBIOSCURSOR:
		cu = (struct fbcursor *)data;
		v = cu->set;

		if ((v & FB_CUR_SETSHAPE) != 0) {
			if ((u_int)cu->size.x > 32 || (u_int)cu->size.y > 32)
				return (EINVAL);
			count = cu->size.y * 4;
			rv = copyin(cu->mask, curbits[0], count);
			if (rv)
				return rv;
			rv = copyin(cu->image, curbits[1], count);
			if (rv)
				return rv;
		}
		if ((v & FB_CUR_SETCUR) != 0) {
			if (cu->enable)
				zx_cursor_unblank(sc);
			else
				zx_cursor_blank(sc);
		}
		if ((v & (FB_CUR_SETPOS | FB_CUR_SETHOT)) != 0) {
			if ((v & FB_CUR_SETPOS) != 0)
				sc->sc_curpos = cu->pos;
			if ((v & FB_CUR_SETHOT) != 0)
				sc->sc_curhot = cu->hot;
			zx_cursor_move(sc);
		}
		if ((v & FB_CUR_SETCMAP) != 0) {
			if (cu->cmap.index > 2 ||
			    cu->cmap.count > 2 - cu->cmap.index)
				return (EINVAL);
			for (i = 0; i < cu->cmap.count; i++) {
				if ((v = fubyte(&cu->cmap.red[i])) < 0)
					return (EFAULT);
				sc->sc_curcmap[i + cu->cmap.index + 0] = v;
				if ((v = fubyte(&cu->cmap.green[i])) < 0)
					return (EFAULT);
				sc->sc_curcmap[i + cu->cmap.index + 2] = v;
				if ((v = fubyte(&cu->cmap.blue[i])) < 0)
					return (EFAULT);
				sc->sc_curcmap[i + cu->cmap.index + 4] = v;
			}
			zx_cursor_color(sc);
		}
		if ((v & FB_CUR_SETSHAPE) != 0) {
			sc->sc_cursize = cu->size;
			count = cu->size.y * 4;
			memset(sc->sc_curbits, 0, sizeof(sc->sc_curbits));
			memcpy(sc->sc_curbits[0], curbits[0], count);
			memcpy(sc->sc_curbits[1], curbits[1], count);
			zx_cursor_set(sc);
		}
		break;

	case FBIOGCURSOR:
		cu = (struct fbcursor *)data;

		cu->set = FB_CUR_SETALL;
		cu->enable = ((sc->sc_flags & ZX_CURSOR) != 0);
		cu->pos = sc->sc_curpos;
		cu->hot = sc->sc_curhot;
		cu->size = sc->sc_cursize;

		if (cu->image != NULL) {
			count = sc->sc_cursize.y * 4;
			rv = copyout(sc->sc_curbits[1], cu->image, count);
			if (rv)
				return (rv);
			rv = copyout(sc->sc_curbits[0], cu->mask, count);
			if (rv)
				return (rv);
		}
		if (cu->cmap.red != NULL) {
			if (cu->cmap.index > 2 ||
			    cu->cmap.count > 2 - cu->cmap.index)
				return (EINVAL);
			for (i = 0; i < cu->cmap.count; i++) {
				v = sc->sc_curcmap[i + cu->cmap.index + 0];
				if (subyte(&cu->cmap.red[i], v))
					return (EFAULT);
				v = sc->sc_curcmap[i + cu->cmap.index + 2];
				if (subyte(&cu->cmap.green[i], v))
					return (EFAULT);
				v = sc->sc_curcmap[i + cu->cmap.index + 4];
				if (subyte(&cu->cmap.blue[i], v))
					return (EFAULT);
			}
		} else {
			cu->cmap.index = 0;
			cu->cmap.count = 2;
		}
		break;

	default:
#ifdef DEBUG
		log(LOG_NOTICE, "zxioctl(0x%lx) (%s[%d])\n", cmd,
		    l->l_proc->p_comm, l->l_proc->p_pid);
#endif
		return (ENOTTY);
	}

	return (0);
}

static int
zx_intr(void *cookie)
{

	return (1);
}

static void
zx_reset(struct zx_softc *sc)
{
	struct fbtype *fbt;
	u_int i;

	fbt = &sc->sc_fb.fb_type;

	zx_cross_loadwid(sc, ZX_WID_DBL_8, 0, 0x2c0);
	zx_cross_loadwid(sc, ZX_WID_DBL_8, 1, 0x30);
	zx_cross_loadwid(sc, ZX_WID_DBL_8, 2, 0x20);
	zx_cross_loadwid(sc, ZX_WID_DBL_24, 1, 0x30);

	i = bus_space_read_4(sc->sc_bt, sc->sc_bhzdss1, zd_misc);
	i |= ZX_SS1_MISC_ENABLE;
	bus_space_write_4(sc->sc_bt, sc->sc_bhzdss1, zd_misc, i);

	bus_space_write_4(sc->sc_bt, sc->sc_bhzdss0, zd_wid, 1);
	bus_space_write_4(sc->sc_bt, sc->sc_bhzdss0, zd_widclip, 0);
	bus_space_write_4(sc->sc_bt, sc->sc_bhzdss0, zd_wmask, 0xffff);
	bus_space_write_4(sc->sc_bt, sc->sc_bhzdss0, zd_vclipmin, 0);
	bus_space_write_4(sc->sc_bt, sc->sc_bhzdss0, zd_vclipmax,
	    (fbt->fb_width - 1) | ((fbt->fb_height - 1) << 16));
	bus_space_write_4(sc->sc_bt, sc->sc_bhzdss0, zd_fg, 0);
	bus_space_write_4(sc->sc_bt, sc->sc_bhzdss0, zd_planemask, 0xffffffff);
	bus_space_write_4(sc->sc_bt, sc->sc_bhzdss0, zd_rop, ZX_STD_ROP);

	bus_space_write_4(sc->sc_bt, sc->sc_bhzc, zc_extent,
	    (fbt->fb_width - 1) | ((fbt->fb_height - 1) << 11));
	bus_space_write_4(sc->sc_bt, sc->sc_bhzc, zc_addrspace,
	    ZX_ADDRSPC_FONT_OBGR);
	bus_space_write_4(sc->sc_bt, sc->sc_bhzc, zc_fontt, 0);

	for (i = 0; i < 256; i++) {
		sc->sc_cmap[i] = rasops_cmap[i * 3];
		sc->sc_cmap[i + 256] = rasops_cmap[i * 3 + 1];
		sc->sc_cmap[i + 512] = rasops_cmap[i * 3 + 2];
	}

	zx_cmap_put(sc);
}

static int
zx_cross_wait(struct zx_softc *sc)
{
	int i;

	for (i = 300000; i != 0; i--) {
		if ((bus_space_read_4(sc->sc_bt, sc->sc_bhzx, zx_csr) &
		    ZX_CROSS_CSR_PROGRESS) == 0)
			break;
		DELAY(1);
	}

	if (i == 0)
		printf("zx_cross_wait: timed out\n");

	return (i);
}

static int
zx_cross_loadwid(struct zx_softc *sc, u_int type, u_int index, u_int value)
{
	u_int tmp = 0;

	bus_space_write_4(sc->sc_bt, sc->sc_bhzx, zx_type, ZX_CROSS_TYPE_WID);

	if (zx_cross_wait(sc))
		return (1);

	if (type == ZX_WID_DBL_8)
		tmp = (index & 0x0f) + 0x40;
	else if (type == ZX_WID_DBL_24)
		tmp = index & 0x3f;

	bus_space_write_4(sc->sc_bt, sc->sc_bhzx, zx_type, 0x5800 + tmp);
	bus_space_write_4(sc->sc_bt, sc->sc_bhzx, zx_value, value);
	bus_space_write_4(sc->sc_bt, sc->sc_bhzx, zx_type, ZX_CROSS_TYPE_WID);
	bus_space_write_4(sc->sc_bt, sc->sc_bhzx, zx_csr,
	    ZX_CROSS_CSR_UNK | ZX_CROSS_CSR_UNK2);

	return (0);
}

static int
zx_cmap_put(struct zx_softc *sc)
{
	const u_char *b;
	u_int i, t;

	bus_space_write_4(sc->sc_bt, sc->sc_bhzx, zx_type, ZX_CROSS_TYPE_CLUT0);

	zx_cross_wait(sc);

	bus_space_write_4(sc->sc_bt, sc->sc_bhzx, zx_type,
	    ZX_CROSS_TYPE_CLUTDATA);

	for (i = 0, b = sc->sc_cmap; i < 256; i++) {
		t = b[i];
		t |= b[i + 256] << 8;
		t |= b[i + 512] << 16;
		bus_space_write_4(sc->sc_bt, sc->sc_bhzx, zx_value, t);
	}

	bus_space_write_4(sc->sc_bt, sc->sc_bhzx, zx_type, ZX_CROSS_TYPE_CLUT0);
	i = bus_space_read_4(sc->sc_bt, sc->sc_bhzx, zx_csr);
	i = i | ZX_CROSS_CSR_UNK | ZX_CROSS_CSR_UNK2;
	bus_space_write_4(sc->sc_bt, sc->sc_bhzx, zx_csr, i);
	return (0);
}

static void
zx_cursor_move(struct zx_softc *sc)
{
	int sx, sy, x, y;

	x = sc->sc_curpos.x - sc->sc_curhot.x;
	y = sc->sc_curpos.y - sc->sc_curhot.y;

	if (x < 0) {
		sx = min(-x, 32);
		x = 0;
	} else
		sx = 0;

	if (y < 0) {
		sy = min(-y, 32);
		y = 0;
	} else
		sy = 0;

	if (sx != sc->sc_shiftx || sy != sc->sc_shifty) {
		sc->sc_shiftx = sx;
		sc->sc_shifty = sy;
		zx_cursor_set(sc);
	}

	bus_space_write_4(sc->sc_bt, sc->sc_bhzcu, zcu_sxy,
	    ((y & 0x7ff) << 11) | (x & 0x7ff));
	bus_space_write_4(sc->sc_bt, sc->sc_bhzcu, zcu_misc,
	    bus_space_read_4(sc->sc_bt, sc->sc_bhzcu, zcu_misc) | 0x30);

	/* XXX Necessary? */
	bus_space_write_4(sc->sc_bt, sc->sc_bhzcu, zcu_misc,
	    bus_space_read_4(sc->sc_bt, sc->sc_bhzcu, zcu_misc) | 0x80);
}

static void
zx_cursor_set(struct zx_softc *sc)
{
	int i, j, data;

	if ((sc->sc_flags & ZX_CURSOR) != 0)
		bus_space_write_4(sc->sc_bt, sc->sc_bhzcu, zcu_misc,
		    bus_space_read_4(sc->sc_bt, sc->sc_bhzcu, zcu_misc) &
		    ~0x80);

	for (j = 0; j < 2; j++) {
		bus_space_write_4(sc->sc_bt, sc->sc_bhzcu, zcu_type, 0x20 << j);

		for (i = sc->sc_shifty; i < 32; i++) {
			data = sc->sc_curbits[j][i];
			bus_space_write_4(sc->sc_bt, sc->sc_bhzcu, zcu_data,
			    data >> sc->sc_shiftx);
		}
		for (i = sc->sc_shifty; i != 0; i--)
			bus_space_write_4(sc->sc_bt, sc->sc_bhzcu, zcu_data, 0);
	}

	if ((sc->sc_flags & ZX_CURSOR) != 0)
		bus_space_write_4(sc->sc_bt, sc->sc_bhzcu, zcu_misc,
		    bus_space_read_4(sc->sc_bt, sc->sc_bhzcu, zcu_misc) | 0x80);
}

static void
zx_cursor_blank(struct zx_softc *sc)
{

	sc->sc_flags &= ~ZX_CURSOR;
	bus_space_write_4(sc->sc_bt, sc->sc_bhzcu, zcu_misc,
	    bus_space_read_4(sc->sc_bt, sc->sc_bhzcu, zcu_misc) & ~0x80);
}

static void
zx_cursor_unblank(struct zx_softc *sc)
{

	sc->sc_flags |= ZX_CURSOR;
	bus_space_write_4(sc->sc_bt, sc->sc_bhzcu, zcu_misc,
	    bus_space_read_4(sc->sc_bt, sc->sc_bhzcu, zcu_misc) | 0x80);
}

static void
zx_cursor_color(struct zx_softc *sc)
{
	uint8_t tmp;

	bus_space_write_4(sc->sc_bt, sc->sc_bhzcu, zcu_type, 0x50);

	tmp = sc->sc_curcmap[0] | (sc->sc_curcmap[2] << 8) |
	    (sc->sc_curcmap[4] << 16);
	bus_space_write_4(sc->sc_bt, sc->sc_bhzcu, zcu_data, tmp);

	tmp = sc->sc_curcmap[1] | (sc->sc_curcmap[3] << 8) |
	    (sc->sc_curcmap[5] << 16);
	bus_space_write_4(sc->sc_bt, sc->sc_bhzcu, zcu_data, sc->sc_curcmap[1]);

	bus_space_write_4(sc->sc_bt, sc->sc_bhzcu, zcu_misc,
	    bus_space_read_4(sc->sc_bt, sc->sc_bhzcu, zcu_misc) | 0x03);
}

static void
zx_blank(device_t dv)
{
	struct zx_softc *sc;

	sc = device_private(dv);

	if ((sc->sc_flags & ZX_BLANKED) != 0)
		return;
	sc->sc_flags |= ZX_BLANKED;

	bus_space_write_4(sc->sc_bt, sc->sc_bhzx, zx_type, ZX_CROSS_TYPE_VIDEO);
	bus_space_write_4(sc->sc_bt, sc->sc_bhzx, zx_csr,
	    bus_space_read_4(sc->sc_bt, sc->sc_bhzx, zx_csr) &
	    ~ZX_CROSS_CSR_ENABLE);
}

static void
zx_unblank(device_t dv)
{
	struct zx_softc *sc;

	sc = device_private(dv);

	if ((sc->sc_flags & ZX_BLANKED) == 0)
		return;
	sc->sc_flags &= ~ZX_BLANKED;

	bus_space_write_4(sc->sc_bt, sc->sc_bhzx, zx_type, ZX_CROSS_TYPE_VIDEO);
	bus_space_write_4(sc->sc_bt, sc->sc_bhzx, zx_csr,
	    bus_space_read_4(sc->sc_bt, sc->sc_bhzx, zx_csr) |
	    ZX_CROSS_CSR_ENABLE);
}

static paddr_t
zxmmap(dev_t dev, off_t off, int prot)
{
	struct zx_softc *sc;
	const struct zx_mmo *mm, *mmmax;

	sc = device_lookup_private(&zx_cd, minor(dev));
	off = trunc_page(off);
	mm = zx_mmo;
	mmmax = mm + sizeof(zx_mmo) / sizeof(zx_mmo[0]);

	for (; mm < mmmax; mm++)
		if (off >= mm->mo_va && off < mm->mo_va + mm->mo_size) {
			off = off - mm->mo_va + mm->mo_pa;
			return (bus_space_mmap(sc->sc_bt, sc->sc_paddr,
			    off, prot, BUS_SPACE_MAP_LINEAR));
		}

	return (-1);
}

static void
zx_fillrect(struct zx_softc *sc, int x, int y, int w, int h, uint32_t bg,
	    int rop)
{


	while ((bus_space_read_4(sc->sc_bt, sc->sc_bhzc, zc_csr) &
	    ZX_CSR_BLT_BUSY) != 0)
		;

	bus_space_write_4(sc->sc_bt, sc->sc_bhzdss0, zd_rop, rop);
	bus_space_write_4(sc->sc_bt, sc->sc_bhzdss0, zd_fg, bg);
	bus_space_write_4(sc->sc_bt, sc->sc_bhzc, zc_extent,
	    (w - 1) | ((h - 1) << 11));
	bus_space_write_4(sc->sc_bt, sc->sc_bhzc, zc_fill,
	    x | (y << 11) | 0x80000000);
}

static void
zx_copyrect(struct zx_softc *sc, int sx, int sy, int dx, int dy, int w,
	    int h)
{
	uint32_t dir;

	w -= 1;
	h -= 1;

	if (sy < dy || sx < dx) {
		dir = 0x80000000;
		sx += w;
		sy += h;
		dx += w;
		dy += h;
	} else
		dir = 0;

	while ((bus_space_read_4(sc->sc_bt, sc->sc_bhzc, zc_csr) &
	    ZX_CSR_BLT_BUSY) != 0)
		;

	bus_space_write_4(sc->sc_bt, sc->sc_bhzdss0, zd_rop, ZX_STD_ROP);
	bus_space_write_4(sc->sc_bt, sc->sc_bhzc, zc_extent,
	    w | (h << 11) | dir);
	bus_space_write_4(sc->sc_bt, sc->sc_bhzc, zc_src, sx | (sy << 11));
	bus_space_write_4(sc->sc_bt, sc->sc_bhzc, zc_copy, dx | (dy << 11));
}

static void
zx_do_cursor(void *cookie, int on, int row, int col)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;
	struct zx_softc *sc = scr->scr_cookie;
	int x, y, wi, he;

	wi = ri->ri_font->fontwidth;
	he = ri->ri_font->fontheight;

	if (ri->ri_flg & RI_CURSOR) {
		x = ri->ri_ccol * wi + ri->ri_xorigin;
		y = ri->ri_crow * he + ri->ri_yorigin;
		zx_fillrect(sc, x, y, wi, he, 0xff000000,
		  ZX_ROP_NEW_XOR_OLD | ZX_ATTR_WE_ENABLE | ZX_ATTR_OE_ENABLE |
		  ZX_ATTR_FORCE_WID);
		ri->ri_flg &= ~RI_CURSOR;
	}

	ri->ri_crow = row;
	ri->ri_ccol = col;

	if (on)
	{
		x = ri->ri_ccol * wi + ri->ri_xorigin;
		y = ri->ri_crow * he + ri->ri_yorigin;
		zx_fillrect(sc, x, y, wi, he, 0xff000000,
		  ZX_ROP_NEW_XOR_OLD | ZX_ATTR_WE_ENABLE | ZX_ATTR_OE_ENABLE |
		  ZX_ATTR_FORCE_WID);
		ri->ri_flg |= RI_CURSOR;
	}
}

static void
zx_erasecols(void *cookie, int row, int startcol, int ncols, long attr)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;
	struct zx_softc *sc = scr->scr_cookie;
	int32_t x, y, width, height, bg;

	x = ri->ri_xorigin + ri->ri_font->fontwidth * startcol;
	y = ri->ri_yorigin + ri->ri_font->fontheight * row;
	width = ri->ri_font->fontwidth * ncols;
	height = ri->ri_font->fontheight;
	bg = ((uint32_t)ri->ri_devcmap[(attr >> 16) & 0xff]) << 24;
	zx_fillrect(sc, x, y, width, height, bg, ZX_STD_ROP);
}

static void
zx_eraserows(void *cookie, int row, int nrows, long attr)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;
	struct zx_softc *sc = scr->scr_cookie;
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
	bg = ((uint32_t)ri->ri_devcmap[(attr >> 16) & 0xff]) << 24;
	zx_fillrect(sc, x, y, width, height, bg, ZX_STD_ROP);
}

static void
zx_copyrows(void *cookie, int srcrow, int dstrow, int nrows)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;
	struct zx_softc *sc = scr->scr_cookie;
	int32_t x, ys, yd, width, height;

	x = ri->ri_xorigin;
	ys = ri->ri_yorigin + ri->ri_font->fontheight * srcrow;
	yd = ri->ri_yorigin + ri->ri_font->fontheight * dstrow;
	width = ri->ri_emuwidth;
	height = ri->ri_font->fontheight * nrows;
	zx_copyrect(sc, x, ys, x, yd, width, height);
}

static void
zx_copycols(void *cookie, int row, int srccol, int dstcol, int ncols)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;
	struct zx_softc *sc = scr->scr_cookie;
	int32_t xs, xd, y, width, height;

	xs = ri->ri_xorigin + ri->ri_font->fontwidth * srccol;
	xd = ri->ri_xorigin + ri->ri_font->fontwidth * dstcol;
	y = ri->ri_yorigin + ri->ri_font->fontheight * row;
	width = ri->ri_font->fontwidth * ncols;
	height = ri->ri_font->fontheight;
	zx_copyrect(sc, xs, y, xd, y, width, height);
}

static void
zx_putchar(void *cookie, int row, int col, u_int uc, long attr)
{
	struct rasops_info *ri = cookie;
	struct wsdisplay_font *font = PICK_FONT(ri, uc);
	struct vcons_screen *scr = ri->ri_hw;
	struct zx_softc *sc = scr->scr_cookie;
	volatile uint32_t *dp;
	uint8_t *fb;
	int fs, i, ul;
	uint32_t fg, bg;
	
	rasops_unpack_attr(attr, &fg, &bg, &ul);
	bg = ((uint32_t)ri->ri_devcmap[bg]) << 24;
	fg = ((uint32_t)ri->ri_devcmap[fg]) << 24;
	if (uc == ' ') {
		int x, y;

		x = ri->ri_xorigin + font->fontwidth * col;
		y = ri->ri_yorigin + font->fontheight * row;
		zx_fillrect(sc, x, y, font->fontwidth,
			    font->fontheight, bg, ZX_STD_ROP);
		return;
	}

	dp = (volatile uint32_t *)sc->sc_pixels +
	    ((row * font->fontheight + ri->ri_yorigin) << 11) +
	    (col * font->fontwidth + ri->ri_xorigin);
	fb = (uint8_t *)font->data + (uc - font->firstchar) *
	    ri->ri_fontscale;
	fs = font->stride;

	while ((bus_space_read_4(sc->sc_bt, sc->sc_bhzc, zc_csr) &
	    ZX_CSR_BLT_BUSY) != 0)
		;

	bus_space_write_4(sc->sc_bt, sc->sc_bhzdss0, zd_rop, ZX_STD_ROP);
	bus_space_write_4(sc->sc_bt, sc->sc_bhzdss0, zd_fg, fg);
	bus_space_write_4(sc->sc_bt, sc->sc_bhzdss0, zd_bg, bg);
	bus_space_write_4(sc->sc_bt, sc->sc_bhzc, zc_fontmsk,
	    0xffffffff << (32 - font->fontwidth));

	if (font->fontwidth <= 8) {
		for (i = font->fontheight; i != 0; i--, dp += 2048) {
			*dp = *fb << 24;
			fb += fs;
		}
	} else {
		for (i = font->fontheight; i != 0; i--, dp += 2048) {
			*dp = *((uint16_t *)fb) << 16;
			fb += fs;
		}
	}

	if (ul) {
		dp -= 4096;
		*dp = 0xffffffff;
	}
}

#if NWSDISPLAY > 0
static int
zx_ioctl(void *v, void *vs, u_long cmd, void *data, int flag,
	struct lwp *l)
{
	/* we'll probably need to add more stuff here */
	struct vcons_data *vd = v;
	struct zx_softc *sc = vd->cookie;
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
			return zx_getcmap(sc, 
			    (struct wsdisplay_cmap *)data);
		case WSDISPLAYIO_PUTCMAP:
			return zx_putcmap(sc, 
			    (struct wsdisplay_cmap *)data);

		case WSDISPLAYIO_SMODE:
			{
				int new_mode = *(int*)data;
				if (new_mode != sc->sc_mode)
				{
					sc->sc_mode = new_mode;
					if(new_mode == WSDISPLAYIO_MODE_EMUL)
					{
						zx_reset(sc);
						vcons_redraw_screen(ms);
					}
				}
			}
	}
	return EPASSTHROUGH;
}

static paddr_t
zx_mmap(void *v, void *vs, off_t offset, int prot)
{
	/* I'm not at all sure this is the right thing to do */
	return zxmmap(0, offset, prot); /* assume minor dev 0 for now */
}

static int
zx_putcmap(struct zx_softc *sc, struct wsdisplay_cmap *cm)
{
	u_int index = cm->index;
	u_int count = cm->count;
	int error,i;
	if (index >= 256 || count > 256 || index + count > 256)
		return EINVAL;

	for (i = 0; i < count; i++)
	{
		error = copyin(&cm->red[i],
		    &sc->sc_cmap[index + i], 1);
		if (error)
			return error;
		error = copyin(&cm->green[i],
		    &sc->sc_cmap[index + i + 256], 1);
		if (error)
			return error;
		error = copyin(&cm->blue[i],
		    &sc->sc_cmap[index + i + 512], 1);
		if (error)
			return error;
	}
	zx_cmap_put(sc);

	return 0;
}

static int
zx_getcmap(struct zx_softc *sc, struct wsdisplay_cmap *cm)
{
	u_int index = cm->index;
	u_int count = cm->count;
	int error,i;

	if (index >= 256 || count > 256 || index + count > 256)
		return EINVAL;

	for (i = 0; i < count; i++)
	{
		error = copyout(&sc->sc_cmap[index + i],
		    &cm->red[i], 1);
		if (error)
			return error;
		error = copyout(&sc->sc_cmap[index + i + 256],
		    &cm->green[i], 1);
		if (error)
			return error;
		error = copyout(&sc->sc_cmap[index + i + 256],
		    &cm->blue[i], 1);
		if (error)
			return error;
	}

	return 0;
}

static void
zx_init_screen(void *cookie, struct vcons_screen *scr,
    int existing, long *defattr)
{
	struct zx_softc *sc = cookie;
	struct rasops_info *ri = &scr->scr_ri;

	ri->ri_depth = 8; /*sc->sc_fb.fb_type.fb_depth = 32;*/
	ri->ri_width = sc->sc_width;
	ri->ri_height = sc->sc_height;
	ri->ri_stride = sc->sc_stride;
	ri->ri_flg = RI_CENTER;

	ri->ri_bits = (void *)sc->sc_pixels;
	
	rasops_init(ri, 0, 0);
	ri->ri_caps = WSSCREEN_WSCOLORS | WSSCREEN_REVERSE;
	rasops_reconfig(ri, sc->sc_height / ri->ri_font->fontheight,
		    sc->sc_width / ri->ri_font->fontwidth);

	ri->ri_hw = scr;

	ri->ri_ops.cursor = zx_do_cursor;
	ri->ri_ops.copycols = zx_copycols;
	ri->ri_ops.copyrows = zx_copyrows;
	ri->ri_ops.erasecols = zx_erasecols;
	ri->ri_ops.eraserows = zx_eraserows;
	ri->ri_ops.putchar = zx_putchar;
}

#endif /* NWSDISPLAY > 0 */
