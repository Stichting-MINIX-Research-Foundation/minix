/*	$NetBSD: tcx.c,v 1.56 2014/07/25 08:10:38 dholland Exp $ */

/*
 *  Copyright (c) 1996, 1998, 2009 The NetBSD Foundation, Inc.
 *  All rights reserved.
 *
 *  This code is derived from software contributed to The NetBSD Foundation
 *  by Paul Kranenburg and Michael Lorenz.
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
 * color display (TCX) driver.
 *
 * Does not handle interrupts, even though they can occur.
 *
 * XXX should defer colormap updates to vertical retrace interrupts
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: tcx.c,v 1.56 2014/07/25 08:10:38 dholland Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>
#include <sys/mman.h>
#include <sys/tty.h>
#include <sys/conf.h>

#ifdef DEBUG
#include <sys/proc.h>
#include <sys/syslog.h>
#endif

#include <sys/bus.h>
#include <machine/autoconf.h>

#include <dev/sun/fbio.h>
#include <dev/sun/fbvar.h>
#include <dev/sun/btreg.h>
#include <dev/sun/btvar.h>

#include <dev/sbus/sbusvar.h>
#include <dev/sbus/tcxreg.h>

#include <dev/wscons/wsdisplayvar.h>
#include <dev/wscons/wsconsio.h>
#include <dev/wsfont/wsfont.h>
#include <dev/rasops/rasops.h>

#include <dev/wscons/wsdisplay_vconsvar.h>

#include "opt_wsemul.h"

#include "ioconf.h"

/* per-display variables */
struct tcx_softc {
	device_t	sc_dev;		/* base device */
	struct fbdevice	sc_fb;		/* frame buffer device */
	bus_space_tag_t	sc_bustag;
	struct openprom_addr sc_physaddr[TCX_NREG];/* phys addr of h/w */

	bus_space_handle_t sc_bt;	/* Brooktree registers */
	bus_space_handle_t sc_thc;	/* THC registers */
	uint8_t 	*sc_fbaddr;	/* framebuffer */
	uint64_t 	*sc_rblit;	/* blitspace */
	uint64_t 	*sc_rstip;	/* stipple space */

	short		sc_8bit;	/* true if 8-bit hardware */
	short		sc_blanked;	/* true if blanked */
	uint32_t	sc_fbsize;	/* size of the 8bit fb */
	u_char		sc_cmap_red[256];
	u_char		sc_cmap_green[256];
	u_char		sc_cmap_blue[256];
	int 		sc_mode, sc_bg;
	int		sc_cursor_x, sc_cursor_y;
	int		sc_hotspot_x, sc_hotspot_y;
	struct vcons_data vd;
};

static struct vcons_screen tcx_console_screen;

extern const u_char rasops_cmap[768];

struct wsscreen_descr tcx_defscreendesc = {
	"default",
	0, 0,
	NULL,
	8, 16,
	WSSCREEN_WSCOLORS,
};

const struct wsscreen_descr *_tcx_scrlist[] = {
	&tcx_defscreendesc,
	/* XXX other formats, graphics screen? */
};

struct wsscreen_list tcx_screenlist = {
	sizeof(_tcx_scrlist) / sizeof(struct wsscreen_descr *),
	_tcx_scrlist
};

/* autoconfiguration driver */
static void	tcxattach(device_t, device_t, void *);
static int	tcxmatch(device_t, cfdata_t, void *);
static void	tcx_unblank(device_t);

CFATTACH_DECL_NEW(tcx, sizeof(struct tcx_softc),
    tcxmatch, tcxattach, NULL, NULL);

dev_type_open(tcxopen);
dev_type_close(tcxclose);
dev_type_ioctl(tcxioctl);
dev_type_mmap(tcxmmap);

const struct cdevsw tcx_cdevsw = {
	.d_open = tcxopen,
	.d_close = tcxclose,
	.d_read = noread,
	.d_write = nowrite,
	.d_ioctl = tcxioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = nopoll,
	.d_mmap = tcxmmap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = 0
};

/* frame buffer generic driver */
static struct fbdriver tcx_fbdriver = {
	tcx_unblank, tcxopen, tcxclose, tcxioctl, nopoll, tcxmmap,
	nokqfilter
};

static void tcx_reset(struct tcx_softc *);
static void tcx_loadcmap(struct tcx_softc *, int, int);

static int	tcx_ioctl(void *, void *, u_long, void *, int, struct lwp *);
static paddr_t	tcx_mmap(void *, void *, off_t, int);

static void	tcx_init_cmap(struct tcx_softc *);
static void	tcx_init_screen(void *, struct vcons_screen *, int, long *);
static void	tcx_clearscreen(struct tcx_softc *, int);
static void	tcx_copyrows(void *, int, int, int);
static void	tcx_eraserows(void *, int, int, long);
static void	tcx_putchar(void *, int, int, u_int, long);
static void	tcx_set_video(struct tcx_softc *, int);
static int	tcx_do_cursor(struct tcx_softc *, struct wsdisplay_cursor *);
static void	tcx_set_cursor(struct tcx_softc *);

struct wsdisplay_accessops tcx_accessops = {
	tcx_ioctl,
	tcx_mmap,
	NULL,	/* vcons_alloc_screen */
	NULL,	/* vcons_free_screen */
	NULL,	/* vcons_show_screen */
	NULL,	/* load_font */
	NULL,	/* polls */
	NULL,	/* scroll */
};

#define OBPNAME	"SUNW,tcx"

/*
 * Match a tcx.
 */
int
tcxmatch(device_t parent, cfdata_t cf, void *aux)
{
	struct sbus_attach_args *sa = aux;

	if (strcmp(sa->sa_name, OBPNAME) == 0)
		return 100;	/* beat genfb */
	return 0;
}

/*
 * Attach a display.
 */
void
tcxattach(device_t parent, device_t self, void *args)
{
	struct tcx_softc *sc = device_private(self);
	struct sbus_attach_args *sa = args;
	struct wsemuldisplaydev_attach_args aa;
	struct rasops_info *ri;
	unsigned long defattr;
	int node;
	struct fbdevice *fb = &sc->sc_fb;
	bus_space_handle_t bh;
	int isconsole;
	uint32_t confreg;

	sc->sc_dev = self;
	sc->sc_bustag = sa->sa_bustag;
	node = sa->sa_node;

	sc->sc_cursor_x = 0x7fff;
	sc->sc_cursor_y = 0x7fff;
	sc->sc_hotspot_x = 0;
	sc->sc_hotspot_y = 0;

	fb->fb_driver = &tcx_fbdriver;
	fb->fb_device = sc->sc_dev;
	/* Mask out invalid flags from the user. */
	fb->fb_flags = device_cfdata(sc->sc_dev)->cf_flags & FB_USERMASK;
	/*
	 * The onboard framebuffer on the SS4 supports only 8-bit mode;
	 * it can be distinguished from the S24 card for the SS5 by the
	 * presence of the "tcx-8-bit" attribute on the SS4 version.
	 */
	sc->sc_8bit = node_has_property(node, "tcx-8-bit");
	fb->fb_type.fb_depth = 8;
	fb_setsize_obp(fb, fb->fb_type.fb_depth, 1152, 900, node);

	/*
	 * actual FB size ( of the 8bit region )
	 * no reason to restrict userland mappings to the visible VRAM
	 */
	if (sc->sc_8bit) {
		aprint_normal(" (8-bit only TCX)\n");
		/* at least the SS4 can have 2MB with a VSIMM */
		sc->sc_fbsize = 0x100000 * prom_getpropint(node, "vram", 1);
	} else {
		aprint_normal(" (S24)\n");
		/* all S24 I know of have 4MB, non-expandable */
		sc->sc_fbsize = 0x100000;
	}

	fb->fb_type.fb_cmsize = 256;
	fb->fb_type.fb_size = sc->sc_fbsize;	/* later code assumes 8bit */
	aprint_normal_dev(self, "%s, %d x %d\n", OBPNAME,
		fb->fb_type.fb_width,
		fb->fb_type.fb_height);

	fb->fb_type.fb_type = FBTYPE_TCXCOLOR;

	if (sa->sa_nreg != TCX_NREG) {
		aprint_error("\n");
		aprint_error_dev(self, "only %d register sets\n",
			sa->sa_nreg);
		return;
	}
	if (sa->sa_reg[TCX_REG_STIP].oa_size < 0x1000) {
		aprint_error("\n");
		aprint_error_dev(self, "STIP register too small (0x%x)\n",
		    sa->sa_reg[TCX_REG_STIP].oa_size);
		return;
	}

	memcpy(sc->sc_physaddr, sa->sa_reg,
	      sa->sa_nreg * sizeof(struct openprom_addr));

	/* Map the register banks we care about */
	if (sbus_bus_map(sa->sa_bustag,
			 sc->sc_physaddr[TCX_REG_THC].oa_space,
			 sc->sc_physaddr[TCX_REG_THC].oa_base,
			 0x1000,
			 BUS_SPACE_MAP_LINEAR, &sc->sc_thc) != 0) {
		aprint_error_dev(self,
		    "tcxattach: cannot map thc registers\n");
		return;
	}

	if (sbus_bus_map(sa->sa_bustag,
			 sc->sc_physaddr[TCX_REG_CMAP].oa_space,
			 sc->sc_physaddr[TCX_REG_CMAP].oa_base,
			 0x1000,
			 BUS_SPACE_MAP_LINEAR, &sc->sc_bt) != 0) {
		aprint_error_dev(self, "tcxattach: cannot map DAC registers\n");
		return;
	}

	/* map the 8bit dumb FB for the console */
	if (sbus_bus_map(sa->sa_bustag,
		 sc->sc_physaddr[TCX_REG_DFB8].oa_space,
		 sc->sc_physaddr[TCX_REG_DFB8].oa_base,
			 sc->sc_fbsize,
			 BUS_SPACE_MAP_LINEAR,
			 &bh) != 0) {
		aprint_error_dev(self, "tcxattach: cannot map framebuffer\n");
		return;
	}
	sc->sc_fbaddr = bus_space_vaddr(sa->sa_bustag, bh);

	/*
	 * 8bit tcx has the RSTIP and RBLIT ranges set to size 0.
	 * On Real Hardware they work anyway ( on my SS4 at least ) but
	 * emulators may not be so forgiving.
	 */
	if (sc->sc_8bit) {
		/* BLIT space */
		if (sbus_bus_map(sa->sa_bustag,
			 sc->sc_physaddr[TCX_REG_BLIT].oa_space,
			 sc->sc_physaddr[TCX_REG_BLIT].oa_base,
				 sc->sc_fbsize << 3,
				 BUS_SPACE_MAP_LINEAR | BUS_SPACE_MAP_LARGE,
				 &bh) != 0) {
			aprint_error_dev(self,
			    "tcxattach: cannot map BLIT space\n");
			return;
		}
		sc->sc_rblit = bus_space_vaddr(sa->sa_bustag, bh);
	
		/* STIP space */
		if (sbus_bus_map(sa->sa_bustag,
			 sc->sc_physaddr[TCX_REG_STIP].oa_space,
			 sc->sc_physaddr[TCX_REG_STIP].oa_base,
				 sc->sc_fbsize << 3,
				 BUS_SPACE_MAP_LINEAR | BUS_SPACE_MAP_LARGE,
				 &bh) != 0) {
			aprint_error_dev(self,
			    "tcxattach: cannot map STIP space\n");
			return;
		}
		sc->sc_rstip = bus_space_vaddr(sa->sa_bustag, bh);
	} else {
		/* RBLIT space */
		if (sbus_bus_map(sa->sa_bustag,
			 sc->sc_physaddr[TCX_REG_RBLIT].oa_space,
			 sc->sc_physaddr[TCX_REG_RBLIT].oa_base,
				 sc->sc_fbsize << 3,
				 BUS_SPACE_MAP_LINEAR | BUS_SPACE_MAP_LARGE,
				 &bh) != 0) {
			aprint_error_dev(self,
			    "tcxattach: cannot map RBLIT space\n");
			return;
		}
		sc->sc_rblit = bus_space_vaddr(sa->sa_bustag, bh);
	
		/* RSTIP space */
		if (sbus_bus_map(sa->sa_bustag,
			 sc->sc_physaddr[TCX_REG_RSTIP].oa_space,
			 sc->sc_physaddr[TCX_REG_RSTIP].oa_base,
				 sc->sc_fbsize << 3,
				 BUS_SPACE_MAP_LINEAR | BUS_SPACE_MAP_LARGE,
				 &bh) != 0) {
			aprint_error_dev(self,
			    "tcxattach: cannot map RSTIP space\n");
			return;
		}
		sc->sc_rstip = bus_space_vaddr(sa->sa_bustag, bh);
	}
	isconsole = fb_is_console(node);

	confreg = bus_space_read_4(sa->sa_bustag, sc->sc_thc, THC_CONFIG);
	aprint_normal_dev(self, "id %d, rev %d, sense %d\n",
		(confreg & THC_CFG_FBID) >> THC_CFG_FBID_SHIFT,
		(confreg & THC_CFG_REV) >> THC_CFG_REV_SHIFT,
		(confreg & THC_CFG_SENSE) >> THC_CFG_SENSE_SHIFT
	);

	/* reset cursor & frame buffer controls */
	tcx_reset(sc);

	if (!sc->sc_8bit)
	    tcx_set_cursor(sc);

	/* enable video */
	confreg = bus_space_read_4(sa->sa_bustag, sc->sc_thc, THC_MISC);
	confreg |= THC_MISC_VIDEN;
	bus_space_write_4(sa->sa_bustag, sc->sc_thc, THC_MISC, confreg);

	if (isconsole) {
		aprint_error_dev(self, "(console)\n");
	}

	fb_attach(&sc->sc_fb, isconsole);

	sc->sc_mode = WSDISPLAYIO_MODE_EMUL;
	wsfont_init();

	vcons_init(&sc->vd, sc, &tcx_defscreendesc, &tcx_accessops);
	sc->vd.init_screen = tcx_init_screen;

	vcons_init_screen(&sc->vd, &tcx_console_screen, 1, &defattr);
	tcx_console_screen.scr_flags |= VCONS_SCREEN_IS_STATIC;

	ri = &tcx_console_screen.scr_ri;

	sc->sc_bg = ri->ri_devcmap[(defattr >> 16) & 0xff];
	tcx_clearscreen(sc, 0);
	tcx_init_cmap(sc);

	tcx_defscreendesc.nrows = ri->ri_rows;
	tcx_defscreendesc.ncols = ri->ri_cols;
	tcx_defscreendesc.textops = &ri->ri_ops;
	tcx_defscreendesc.capabilities = ri->ri_caps;

	if(isconsole) {
		wsdisplay_cnattach(&tcx_defscreendesc, ri, 0, 0, defattr);
		vcons_replay_msgbuf(&tcx_console_screen);
	}

	aa.console = isconsole;
	aa.scrdata = &tcx_screenlist;
	aa.accessops = &tcx_accessops;
	aa.accesscookie = &sc->vd;

	config_found(self, &aa, wsemuldisplaydevprint);
	/*
	 * we need to do this again - something overwrites a handful
	 * palette registers and we end up with white in reg. 0
	 */
	tcx_loadcmap(sc, 0, 256);
}

int
tcxopen(dev_t dev, int flags, int mode, struct lwp *l)
{
	return (0);
}

int
tcxclose(dev_t dev, int flags, int mode, struct lwp *l)
{
	struct tcx_softc *sc = device_lookup_private(&tcx_cd, minor(dev));

	tcx_reset(sc);
	/* we may want to clear and redraw the console here */
	return (0);
}

int
tcxioctl(dev_t dev, u_long cmd, void *data, int flags, struct lwp *l)
{
	struct tcx_softc *sc = device_lookup_private(&tcx_cd, minor(dev));

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
		fba->emu_types[1] = FBTYPE_SUN3COLOR;
		fba->emu_types[2] = -1;
#undef fba
		break;

	case FBIOGETCMAP:
#define	p ((struct fbcmap *)data)
		if (copyout(&sc->sc_cmap_red[p->index], p->red, p->count) != 0)
			return EINVAL;
		if (copyout(&sc->sc_cmap_green[p->index], p->green, p->count)
		    != 0)
			return EINVAL;
		if (copyout(&sc->sc_cmap_blue[p->index], p->blue, p->count)
		    != 0)
			return EINVAL;
		return 0;

	case FBIOPUTCMAP:
		/* copy to software map */
		if (copyin(p->red, &sc->sc_cmap_red[p->index], p->count) != 0)
			return EINVAL;
		if (copyin(p->green, &sc->sc_cmap_green[p->index], p->count)
		    != 0)
			return EINVAL;
		if (copyin(p->blue, &sc->sc_cmap_blue[p->index], p->count) != 0)
			return EINVAL;
		tcx_loadcmap(sc, p->index, p->count);
#undef p
		break;
	case FBIOGVIDEO:
		*(int *)data = sc->sc_blanked;
		break;

	case FBIOSVIDEO:
		tcx_set_video(sc, *(int *)data);
		break;

	default:
#ifdef DEBUG
		log(LOG_NOTICE, "tcxioctl(0x%lx) (%s[%d])\n", cmd,
		    l->l_proc->p_comm, l->l_proc->p_pid);
#endif
		return (ENOTTY);
	}
	return (0);
}

/*
 * Clean up hardware state (e.g., after bootup or after X crashes).
 */
static void
tcx_reset(struct tcx_softc *sc)
{
	uint32_t reg;

	reg = bus_space_read_4(sc->sc_bustag, sc->sc_thc, THC_MISC);
	reg |= THC_MISC_CURSRES;
	bus_space_write_4(sc->sc_bustag, sc->sc_thc, THC_MISC, reg);
}

static void
tcx_init_cmap(struct tcx_softc *sc)
{
	int i, j;

	/* Initialize the default color map. */
	j = 0;
	for (i = 0; i < 256; i++) {

		sc->sc_cmap_red[i] = rasops_cmap[j];
		sc->sc_cmap_green[i] = rasops_cmap[j + 1];
		sc->sc_cmap_blue[i] = rasops_cmap[j + 2];
		j += 3;
	}
	tcx_loadcmap(sc, 0, 256);
}

static void
tcx_loadcmap(struct tcx_softc *sc, int start, int ncolors)
{
	int i;

	for (i = 0; i < ncolors; i++) {
		bus_space_write_4(sc->sc_bustag, sc->sc_bt, DAC_ADDRESS,
		    (start + i) << 24);
		bus_space_write_4(sc->sc_bustag, sc->sc_bt, DAC_FB_LUT,
		    sc->sc_cmap_red[i + start] << 24);
		bus_space_write_4(sc->sc_bustag, sc->sc_bt, DAC_FB_LUT,
		    sc->sc_cmap_green[i + start] << 24);
		bus_space_write_4(sc->sc_bustag, sc->sc_bt, DAC_FB_LUT,
		    sc->sc_cmap_blue[i + start] << 24);
	}
	bus_space_write_4(sc->sc_bustag, sc->sc_bt, DAC_ADDRESS, 0);
}

static void
tcx_unblank(device_t dev)
{
	struct tcx_softc *sc = device_private(dev);

	if (sc->sc_blanked) {
		uint32_t reg;
		sc->sc_blanked = 0;
		reg = bus_space_read_4(sc->sc_bustag, sc->sc_thc, THC_MISC);
		reg &= ~THC_MISC_VSYNC_DISABLE;
		reg &= ~THC_MISC_HSYNC_DISABLE;
		reg |= THC_MISC_VIDEN;
		bus_space_write_4(sc->sc_bustag, sc->sc_thc, THC_MISC, reg);
	}
}

static void
tcx_set_video(struct tcx_softc *sc, int unblank)
{
	uint32_t reg;
	if (unblank) {
		sc->sc_blanked = 0;
		reg = bus_space_read_4(sc->sc_bustag, sc->sc_thc, THC_MISC);
		reg &= ~THC_MISC_VSYNC_DISABLE;
		reg &= ~THC_MISC_HSYNC_DISABLE;
		reg |= THC_MISC_VIDEN;
		bus_space_write_4(sc->sc_bustag, sc->sc_thc, THC_MISC, reg);
	} else {
		sc->sc_blanked = 1;
		reg = bus_space_read_4(sc->sc_bustag, sc->sc_thc, THC_MISC);
		reg |= THC_MISC_VSYNC_DISABLE;
		reg |= THC_MISC_HSYNC_DISABLE;
		reg &= ~THC_MISC_VIDEN;
		bus_space_write_4(sc->sc_bustag, sc->sc_thc, THC_MISC, reg);
	}
}

/*
 * Base addresses at which users can mmap() the various pieces of a tcx.
 */
#define	TCX_USER_RAM	0x00000000
#define	TCX_USER_RAM24	0x01000000
#define	TCX_USER_RAM_COMPAT	0x04000000	/* cg3 emulation */
#define	TCX_USER_STIP	0x10000000
#define	TCX_USER_BLIT	0x20000000
#define	TCX_USER_RDFB32	0x28000000
#define	TCX_USER_RSTIP	0x30000000
#define	TCX_USER_RBLIT	0x38000000
#define	TCX_USER_TEC	0x70001000
#define	TCX_USER_BTREGS	0x70002000
#define	TCX_USER_THC	0x70004000
#define	TCX_USER_DHC	0x70008000
#define	TCX_USER_ALT	0x7000a000
#define	TCX_USER_UART	0x7000c000
#define	TCX_USER_VRT	0x7000e000
#define	TCX_USER_ROM	0x70010000

struct mmo {
	u_int	mo_uaddr;	/* user (virtual) address */
	u_int	mo_size;	/* size, or 0 for video ram size */
	u_int	mo_bank;	/* register bank number */
};

/*
 * Return the address that would map the given device at the given
 * offset, allowing for the given protection, or return -1 for error.
 *
 * XXX	needs testing against `demanding' applications (e.g., aviator)
 */
paddr_t
tcxmmap(dev_t dev, off_t off, int prot)
{
	struct tcx_softc *sc = device_lookup_private(&tcx_cd, minor(dev));
	struct openprom_addr *rr = sc->sc_physaddr;
	struct mmo *mo, *mo_end;
	u_int u, sz;
	static struct mmo mmo[] = {
		{ TCX_USER_RAM, 0, TCX_REG_DFB8 },
		{ TCX_USER_RAM24, 0, TCX_REG_DFB24 },
		{ TCX_USER_RAM_COMPAT, 0, TCX_REG_DFB8 },

		{ TCX_USER_STIP, 1, TCX_REG_STIP },
		{ TCX_USER_BLIT, 1, TCX_REG_BLIT },
		{ TCX_USER_RDFB32, 0, TCX_REG_RDFB32 },
		{ TCX_USER_RSTIP, 1, TCX_REG_RSTIP },
		{ TCX_USER_RBLIT, 1, TCX_REG_RBLIT },
		{ TCX_USER_TEC, 1, TCX_REG_TEC },
		{ TCX_USER_BTREGS, 8192 /* XXX */, TCX_REG_CMAP },
		{ TCX_USER_THC, 0x2000, TCX_REG_THC },
		{ TCX_USER_DHC, 1, TCX_REG_DHC },
		{ TCX_USER_ALT, 1, TCX_REG_ALT },
		{ TCX_USER_ROM, 65536, TCX_REG_ROM },
	};
#define NMMO (sizeof mmo / sizeof *mmo)

	if (off & PGOFSET)
		panic("tcxmmap");

	/*
	 * Entries with size 0 map video RAM (i.e., the size in fb data).
	 * Entries that map 32-bit deep regions are adjusted for their
	 * depth (fb_size gives the size of the 8-bit-deep region).
	 *
	 * Since we work in pages, the fact that the map offset table's
	 * sizes are sometimes bizarre (e.g., 1) is effectively ignored:
	 * one byte is as good as one page.
	 */

	mo = mmo;
	mo_end = &mmo[NMMO];

	for (; mo < mo_end; mo++) {
		if ((u_int)off < mo->mo_uaddr)
			continue;

		u = off - mo->mo_uaddr;
		sz = mo->mo_size;

		if (sz == 0) {
			sz = sc->sc_fb.fb_type.fb_size;
			/*
			 * check for the 32-bit-deep regions and adjust
			 * accordingly
			 */
			if (mo->mo_uaddr == TCX_USER_RAM24 ||
			    mo->mo_uaddr == TCX_USER_RDFB32) {
				if (sc->sc_8bit) {
					/*
					 * not present on 8-bit hardware
					 */
					continue;
				}
				sz *= 4;
			}
		}
		if (sz == 1)
			sz = rr[mo->mo_bank].oa_size;

		if (u < sz) {
			return (bus_space_mmap(sc->sc_bustag,
				BUS_ADDR(rr[mo->mo_bank].oa_space,
					 rr[mo->mo_bank].oa_base),
				u,
				prot,
				BUS_SPACE_MAP_LINEAR));
		}
	}
	return (-1);
}

int
tcx_ioctl(void *v, void *vs, u_long cmd, void *data, int flag,
	struct lwp *l)
{
	struct vcons_data *vd = v;
	struct tcx_softc *sc = vd->cookie;
	struct wsdisplay_fbinfo *wdf;
	struct vcons_screen *ms = vd->active;

	switch (cmd) {
		case WSDISPLAYIO_GTYPE:
			*(u_int *)data = WSDISPLAY_TYPE_SUNTCX;
			return 0;

		case FBIOGVIDEO:
		case WSDISPLAYIO_GVIDEO:
			*(int *)data = !sc->sc_blanked;
			return 0;

		case WSDISPLAYIO_SVIDEO:
		case FBIOSVIDEO:
			tcx_set_video(sc, *(int *)data);
			return 0;

		case WSDISPLAYIO_GINFO:
			wdf = (void *)data;
			wdf->height = ms->scr_ri.ri_height;
			wdf->width = ms->scr_ri.ri_width;
			if (sc->sc_8bit) {
				wdf->depth = 8;
			} else {
				wdf->depth = 32;
			}
			wdf->cmsize = 256;
			return 0;
		case WSDISPLAYIO_LINEBYTES:
			{
				int *ret = (int *)data;
				*ret = sc->sc_8bit ? ms->scr_ri.ri_width :
				    ms->scr_ri.ri_width << 2;
			}
			return 0;
#if 0
		case WSDISPLAYIO_GETCMAP:
			return tcx_getcmap(sc, (struct wsdisplay_cmap *)data);

		case WSDISPLAYIO_PUTCMAP:
			return tcx_putcmap(sc, (struct wsdisplay_cmap *)data);
#endif
		case WSDISPLAYIO_SMODE:
			{
				int new_mode = *(int*)data;
				if (new_mode != sc->sc_mode)
				{
					sc->sc_mode = new_mode;
					if (new_mode == WSDISPLAYIO_MODE_EMUL)
					{
						tcx_init_cmap(sc);
						tcx_clearscreen(sc, 0);
						vcons_redraw_screen(ms);
					} else if (!sc->sc_8bit)
						tcx_clearscreen(sc, 3);
				}
			}
			return 0;

		case WSDISPLAYIO_GCURPOS:
			if (sc->sc_8bit) {
				return EOPNOTSUPP;
			} else {
				struct wsdisplay_curpos *cp = (void *)data;

				cp->x = sc->sc_cursor_x;
				cp->y = sc->sc_cursor_y;
			}
			return 0;

		case WSDISPLAYIO_SCURPOS:
			if (sc->sc_8bit) {
				return EOPNOTSUPP;
			} else {
				struct wsdisplay_curpos *cp = (void *)data;

				sc->sc_cursor_x = cp->x;
				sc->sc_cursor_y = cp->y;
				tcx_set_cursor(sc);
			}
			return 0;

		case WSDISPLAYIO_GCURMAX:
			if (sc->sc_8bit) {
				return EOPNOTSUPP;
			} else {
				struct wsdisplay_curpos *cp = (void *)data;

				cp->x = 32;
				cp->y = 32;
			}
			return 0;

		case WSDISPLAYIO_SCURSOR:
			if (sc->sc_8bit) {
				return EOPNOTSUPP;
			} else {
				struct wsdisplay_cursor *cursor = (void *)data;

				return tcx_do_cursor(sc, cursor);
			}
	}
	return EPASSTHROUGH;
}

static paddr_t
tcx_mmap(void *v, void *vs, off_t offset, int prot)
{
	struct vcons_data *vd = v;
	struct tcx_softc *sc = vd->cookie;

	/* 'regular' framebuffer mmap()ing */
	if (sc->sc_8bit) {
		/* on 8Bit boards hand over the 8 bit aperture */
		if (offset > sc->sc_fbsize)
			return -1;
		return bus_space_mmap(sc->sc_bustag,
		    sc->sc_physaddr[TCX_REG_DFB8].oa_base + offset, 0, prot,
		    BUS_SPACE_MAP_LINEAR);
	} else {
		/* ... but if we have a 24bit aperture we use it */
		if (offset > sc->sc_fbsize * 4)
			return -1;
		return bus_space_mmap(sc->sc_bustag,
		    sc->sc_physaddr[TCX_REG_DFB24].oa_base + offset, 0, prot,
		    BUS_SPACE_MAP_LINEAR);
	}
	return -1;
}

static void
tcx_init_screen(void *cookie, struct vcons_screen *scr,
    int existing, long *defattr)
{
	struct tcx_softc *sc = cookie;
	struct rasops_info *ri = &scr->scr_ri;

	ri->ri_depth = 8;
	ri->ri_width = sc->sc_fb.fb_type.fb_width;
	ri->ri_height = sc->sc_fb.fb_type.fb_height;
	ri->ri_stride = sc->sc_fb.fb_linebytes;
	ri->ri_flg = RI_CENTER | RI_FULLCLEAR;

	ri->ri_bits = sc->sc_fbaddr;

	rasops_init(ri, 0, 0);
	ri->ri_caps = WSSCREEN_WSCOLORS;
	rasops_reconfig(ri, ri->ri_height / ri->ri_font->fontheight,
		    ri->ri_width / ri->ri_font->fontwidth);

	/* enable acceleration */
	ri->ri_ops.copyrows  = tcx_copyrows;
	ri->ri_ops.eraserows = tcx_eraserows;
	ri->ri_ops.putchar   = tcx_putchar;
#if 0
	ri->ri_ops.cursor    = tcx_cursor;
	ri->ri_ops.copycols  = tcx_copycols;
	ri->ri_ops.erasecols = tcx_erasecols;
#endif
}

static void
tcx_clearscreen(struct tcx_softc *sc, int spc)
{
	/* ROP in the upper 4bit is necessary, tcx actually uses it */
	uint64_t bg = 0x30000000ffffffffLL;
	uint64_t spc64;
	int i, len;

	spc64 = ((spc & 3) << 24) | sc->sc_bg;
	bg |= (spc64 << 32);

	len = sc->sc_fb.fb_type.fb_width * sc->sc_fb.fb_type.fb_height;
	for (i = 0; i < len; i += 32)
		sc->sc_rstip[i] = bg;
}

static void
tcx_copyrows(void *cookie, int srcrow, int dstrow, int nrows)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;
	struct tcx_softc *sc = scr->scr_cookie;
	int i, last, first, len, dest, leftover;

	i = ri->ri_width * ri->ri_font->fontheight * nrows;
	len = i & 0xffffe0;
	leftover = i & 0x1f;
	if (srcrow < dstrow) {
		/* we must go bottom to top */
		first = ri->ri_width * 
		    (ri->ri_font->fontheight * srcrow + ri->ri_yorigin);
		last = first + len;
		dest = ri->ri_width *
		    (ri->ri_font->fontheight * dstrow + ri->ri_yorigin) + len;
		if (leftover > 0) {
			sc->sc_rblit[dest + 32] = 
			    (uint64_t)((leftover - 1) << 24) | 
			    (uint64_t)(i + 32);
		}
		for (i = last; i >= first; i -= 32) {
			sc->sc_rblit[dest] = 0x300000001f000000LL | (uint64_t)i;
			dest -= 32;
		}
	} else {
		/* top to bottom */
		first = ri->ri_width * 
		    (ri->ri_font->fontheight * srcrow + ri->ri_yorigin);
		dest = ri->ri_width * 
		    (ri->ri_font->fontheight * dstrow + ri->ri_yorigin);
		last = first + len;
		for (i = first; i <= last; i+= 32) {
			sc->sc_rblit[dest] = 0x300000001f000000LL | (uint64_t)i;
			dest += 32;
		}
		if (leftover > 0) {
			sc->sc_rblit[dest] = 
			    (uint64_t)((leftover - 1) << 24) | (uint64_t)i;
		}
	}
}

static void
tcx_eraserows(void *cookie, int start, int nrows, long attr)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;
	struct tcx_softc *sc = scr->scr_cookie;
	uint64_t temp;
	int i, last, first, len, leftover;

	i = ri->ri_width * ri->ri_font->fontheight * nrows;
	len = i & 0xffffe0;
	leftover = i & 0x1f;
	first = ri->ri_width * 
	    (ri->ri_font->fontheight * start + ri->ri_yorigin);
	last = first + len;
	temp = 0x30000000ffffffffLL | 
	    ((uint64_t)ri->ri_devcmap[(attr >> 16) & 0xff] << 32);

	for (i = first; i <= last; i+= 32)
		sc->sc_rblit[i] = temp;

	if (leftover > 0) {
		temp &= 0xffffffffffffffffLL << (32 - leftover);
		sc->sc_rblit[i] = temp;
	}
}
/*
 * The stipple engine is 100% retarded. All drawing operations have to start 
 * at 32 pixel boundaries so we'll have to deal with characters being split.
 */

static void
tcx_putchar(void *cookie, int row, int col, u_int c, long attr)
{
	struct rasops_info *ri = cookie;
	struct wsdisplay_font *font = PICK_FONT(ri, c);
	struct vcons_screen *scr = ri->ri_hw;
	struct tcx_softc *sc = scr->scr_cookie;
	uint64_t bg, fg, temp, mask;
	int addr, i, uc, shift;
	uint32_t fmask;
	uint8_t *cdata;
	uint16_t *wdata;

	addr = ri->ri_xorigin + col * font->fontwidth +
	    (ri->ri_yorigin + row * font->fontheight) * ri->ri_width;

	/* check if the character is crossing a 32 pixel boundary */
	if ((addr & 0xffffe0) ==
	    ((addr + font->fontwidth - 1) & 0xffffe0)) {
		/* phew, not split */
		shift = addr & 0x1f;
		addr &= 0xffffe0;
		fmask = 0xffffffff >> (32 - font->fontwidth);
		fmask = fmask << (32 - font->fontwidth - shift);
		mask = fmask;
		bg = 0x3000000000000000LL |
		    ((uint64_t)ri->ri_devcmap[(attr >> 16) & 0xff] &
		      0xff) << 32;
		bg |= mask;
		temp = 0x3000000000000000LL |
		    ((uint64_t)ri->ri_devcmap[(attr >> 24) & 0xff] & 0xff) << 
		    	32;
		uc = c - font->firstchar;
		cdata = (uint8_t *)font->data + uc * ri->ri_fontscale;

		if (font->fontwidth < 9) {
			/* byte by byte */
			for (i = 0; i < font->fontheight; i++) {
				sc->sc_rstip[addr] = bg;
				if (*cdata != 0) {
					if (shift > 24) {
						fg = (uint64_t)*cdata >>
					  	  (shift - 24);
					} else {
						fg = (uint64_t)*cdata <<
					  	  (24 - shift);
					}
					sc->sc_rstip[addr] = fg | temp;
				}
				cdata++;
				addr += ri->ri_width;
			}
		} else if (font->fontwidth < 17) {
			/* short by short */
			wdata = (uint16_t *)cdata;
			for (i = 0; i < font->fontheight; i++) {
				sc->sc_rstip[addr] = bg;
				if (*wdata != 0) {
					if (shift > 16) {
						fg = temp | (uint64_t)*wdata >> 
					  	  (shift - 16);
					} else {
						fg = temp | (uint64_t)*wdata << 
					  	  (16 - shift);
					}
					sc->sc_rstip[addr] = fg;
				}
				wdata++;
				addr += ri->ri_width;
			}
		}
	} else {
		/* and now the split case ( man this hardware is dumb ) */
		uint64_t bgr, maskr, fgr;
		uint32_t bork;

		shift = addr & 0x1f;
		addr &= 0xffffe0;
		mask = 0xffffffff >> shift;
		maskr = (uint64_t)(0xffffffffUL << 
		    (32 - (font->fontwidth + shift - 32)));
		bg = 0x3000000000000000LL |
		    ((uint64_t)ri->ri_devcmap[(attr >> 16) & 0xff] &
		      0xff) << 32;
		bgr = bg | maskr;
		bg |= mask;
		temp = 0x3000000000000000LL |
		    ((uint64_t)ri->ri_devcmap[(attr >> 24) & 0xff] & 0xff) << 
		      32;

		uc = c - font->firstchar;
		cdata = (uint8_t *)font->data + uc * ri->ri_fontscale;

		if (font->fontwidth < 9) {
			/* byte by byte */
			for (i = 0; i < font->fontheight; i++) {
				sc->sc_rstip[addr] = bg;
				sc->sc_rstip[addr + 32] = bgr;
				bork = *cdata;
				if (bork != 0) {
					fg = (uint64_t)bork >> (shift - 24);
					sc->sc_rstip[addr] = fg | temp;
					fgr = (uint64_t)(bork << (52 - shift));
					sc->sc_rstip[addr] = fgr | temp;
				}
				cdata++;
				addr += ri->ri_width;
			}
		} else if (font->fontwidth < 17) {
			/* short by short */
			wdata = (uint16_t *)cdata;
			for (i = 0; i < font->fontheight; i++) {
				sc->sc_rstip[addr] = bg;
				sc->sc_rstip[addr + 32] = bgr;
				bork = *wdata;
				if (bork != 0) {
					fg = (uint64_t)bork >> (shift - 16);
					sc->sc_rstip[addr] = fg | temp;
					fgr = (uint64_t)(bork << (48 - shift));
					sc->sc_rstip[addr + 32] = fgr | temp;
				}
				wdata++;
				addr += ri->ri_width;
			}
		}
		
	}
}

static int
tcx_do_cursor(struct tcx_softc *sc, struct wsdisplay_cursor *cur)
{
	if (sc->sc_8bit) {
		/* hw cursor is not implemented on tcx */
		return -1;
	}
	if (cur->which & WSDISPLAY_CURSOR_DOCUR) {

		if (cur->enable) {
			tcx_set_cursor(sc);
		} else {
			/* move the cursor out of sight */
			bus_space_write_4(sc->sc_bustag, sc->sc_thc,
			    THC_CURSOR_POS, 0x7fff7fff);
		}
	}
	if (cur->which & WSDISPLAY_CURSOR_DOHOT) {

		sc->sc_hotspot_x = cur->hot.x;
		sc->sc_hotspot_y = cur->hot.y;
		tcx_set_cursor(sc);
	}
	if (cur->which & WSDISPLAY_CURSOR_DOPOS) {

		sc->sc_cursor_x = cur->pos.x;
		sc->sc_cursor_y = cur->pos.y;
		tcx_set_cursor(sc);
	}
	if (cur->which & WSDISPLAY_CURSOR_DOCMAP) {
#if 0
	/*
	 * apparently we're not writing in the right register here - if we do
	 * this the screen goes all funky
	 */
		int i;
	
		for (i = 0; i < cur->cmap.count; i++) {
			bus_space_write_4(sc->sc_bustag, sc->sc_bt, DAC_ADDRESS,
			    (cur->cmap.index + i + 2) << 24);
			bus_space_write_4(sc->sc_bustag, sc->sc_bt,
			    DAC_CURSOR_LUT, cur->cmap.red[i] << 24);
			bus_space_write_4(sc->sc_bustag, sc->sc_bt,
			    DAC_CURSOR_LUT, cur->cmap.green[i] << 24);
			bus_space_write_4(sc->sc_bustag, sc->sc_bt,
			    DAC_CURSOR_LUT, cur->cmap.blue[i] << 24);
		}
#endif
	}
	if (cur->which & WSDISPLAY_CURSOR_DOSHAPE) {
		int i;
		uint32_t temp, poof;

		for (i = 0; i < 128; i += 4) {
			memcpy(&temp, &cur->mask[i], 4);
			printf("%08x -> ", temp);
			poof = ((temp & 0x80808080) >> 7) |
			       ((temp & 0x40404040) >> 5) |
			       ((temp & 0x20202020) >> 3) |
			       ((temp & 0x10101010) >> 1) |
			       ((temp & 0x08080808) << 1) |
			       ((temp & 0x04040404) << 3) |
			       ((temp & 0x02020202) << 5) |
			       ((temp & 0x01010101) << 7);
			printf("%08x\n", poof);
			bus_space_write_4(sc->sc_bustag, sc->sc_thc,
			    THC_CURSOR_1 + i, poof);
			memcpy(&temp, &cur->image[i], 4);
			poof = ((temp & 0x80808080) >> 7) |
			       ((temp & 0x40404040) >> 5) |
			       ((temp & 0x20202020) >> 3) |
			       ((temp & 0x10101010) >> 1) |
			       ((temp & 0x08080808) << 1) |
			       ((temp & 0x04040404) << 3) |
			       ((temp & 0x02020202) << 5) |
			       ((temp & 0x01010101) << 7);
			bus_space_write_4(sc->sc_bustag, sc->sc_thc,
			    THC_CURSOR_0 + i, poof);
		}
	}
	return 0;
}

static void
tcx_set_cursor(struct tcx_softc *sc)
{
	uint32_t reg;

	reg = (sc->sc_cursor_x - sc->sc_hotspot_x) << 16 | 
	     ((sc->sc_cursor_y - sc->sc_hotspot_y) & 0xffff);
	bus_space_write_4(sc->sc_bustag, sc->sc_thc, THC_CURSOR_POS, reg);
}

