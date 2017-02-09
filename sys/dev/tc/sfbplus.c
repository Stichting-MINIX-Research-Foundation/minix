/* $NetBSD: sfbplus.c,v 1.37 2012/01/11 21:12:36 macallan Exp $ */

/*-
 * Copyright (c) 1999, 2000, 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Tohru Nishimura.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: sfbplus.c,v 1.37 2012/01/11 21:12:36 macallan Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/ioctl.h>

#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>

#include <dev/rasops/rasops.h>
#include <dev/wsfont/wsfont.h>

#include <dev/tc/tcvar.h>
#include <dev/ic/bt459reg.h>
#include <dev/ic/bt463reg.h>
#include <dev/tc/sfbreg.h>
#include <dev/pci/tgareg.h>

#if defined(pmax)
#define	machine_btop(x) mips_btop(MIPS_KSEG1_TO_PHYS(x))
#endif

#if defined(alpha)
#define machine_btop(x) alpha_btop(ALPHA_K0SEG_TO_PHYS(x))
#endif

/* Bt459/Bt463 hardware registers */
#define bt_lo	0
#define bt_hi	1
#define bt_reg	2
#define bt_cmap 3

#define REG(base, index)	*((uint32_t *)(base) + (index))
#define SELECT(vdac, regno) do {			\
	REG(vdac, bt_lo) = ((regno) & 0x00ff);		\
	REG(vdac, bt_hi) = ((regno) & 0x0f00) >> 8;	\
	tc_wmb();					\
   } while (0)

struct hwcmap256 {
#define	CMAP_SIZE	256	/* 256 R/G/B entries */
	uint8_t r[CMAP_SIZE];
	uint8_t g[CMAP_SIZE];
	uint8_t b[CMAP_SIZE];
};

struct hwcursor64 {
	struct wsdisplay_curpos cc_pos;
	struct wsdisplay_curpos cc_hot;
	struct wsdisplay_curpos cc_size;
	struct wsdisplay_curpos cc_magic;
#define	CURSOR_MAX_SIZE	64
	uint8_t cc_color[6];
	uint64_t cc_image[CURSOR_MAX_SIZE];
	uint64_t cc_mask[CURSOR_MAX_SIZE];
};

struct hwops {
	void (*setlut)(void *, struct hwcmap256 *);
	void (*getlut)(void *, struct hwcmap256 *);
	void (*visible)(void *, int);
	void (*locate)(void *, struct hwcursor64 *);
	void (*shape)(void *, struct wsdisplay_curpos *, uint64_t *);
	void (*color)(void *, uint8_t *);
};

struct sfbp_softc {
	vaddr_t sc_vaddr;
	size_t sc_size;
	struct rasops_info *sc_ri;
	struct hwcmap256 sc_cmap;	/* software copy of colormap */
	struct hwcursor64 sc_cursor;	/* software copy of cursor */
	int sc_blanked;
	int sc_curenb;			/* cursor sprite enabled */
	int sc_changed;			/* need update of hardware */
#define	WSDISPLAY_CMAP_DOLUT	0x20
	int nscreens;
	struct hwops sc_hwops;
};

#define	HX_MAGIC_X	368
#define	HX_MAGIC_Y	38

static int  sfbpmatch(device_t, cfdata_t, void *);
static void sfbpattach(device_t, device_t, void *);

CFATTACH_DECL_NEW(sfbp, sizeof(struct sfbp_softc),
    sfbpmatch, sfbpattach, NULL, NULL);

static void sfbp_common_init(struct rasops_info *);
static struct rasops_info sfbp_console_ri;
static tc_addr_t sfbp_consaddr;

static struct wsscreen_descr sfbp_stdscreen = {
	"std", 0, 0,
	NULL, /* textops */
	0, 0,
	WSSCREEN_REVERSE
};

static const struct wsscreen_descr *_sfb_scrlist[] = {
	&sfbp_stdscreen,
};

static const struct wsscreen_list sfb_screenlist = {
	sizeof(_sfb_scrlist) / sizeof(struct wsscreen_descr *), _sfb_scrlist
};

static int	sfbioctl(void *, void *, u_long, void *, int, struct lwp *);
static paddr_t	sfbmmap(void *, void *, off_t, int);

static int	sfb_alloc_screen(void *, const struct wsscreen_descr *,
				      void **, int *, int *, long *);
static void	sfb_free_screen(void *, void *);
static int	sfb_show_screen(void *, void *, int,
				     void (*) (void *, int, int), void *);
static void sfbp_putchar(void *, int, int, u_int, long);
static void sfbp_erasecols(void *, int, int, int, long);
static void sfbp_eraserows(void *, int, int, long);
static void sfbp_copyrows(void *, int, int, int);

static const struct wsdisplay_accessops sfb_accessops = {
	sfbioctl,
	sfbmmap,
	sfb_alloc_screen,
	sfb_free_screen,
	sfb_show_screen,
	0 /* load_font */
};

static void bt459init(void *);
static void bt459visible(void *, int);
static void bt459locate(void *, struct hwcursor64 *);
static void bt459shape(void *, struct wsdisplay_curpos *, uint64_t *);
static void bt459color(void *, uint8_t *);
static void bt459setlut(void *, struct hwcmap256 *);

static void sfbpvisible(void *, int);
static void sfbplocate(void *, struct hwcursor64 *);
static void sfbpshape(void *, struct wsdisplay_curpos *, uint64_t *);
static void bt463init(void *);
static void bt463color(void *, uint8_t *);
static void noplut(void *, struct hwcmap256 *);

/* EXPORT */ int sfbp_cnattach(tc_addr_t);
static int  sfbpintr(void *);
static void sfbp_cmap_init(struct sfbp_softc *);

static int  get_cmap(struct sfbp_softc *, struct wsdisplay_cmap *);
static int  set_cmap(struct sfbp_softc *, struct wsdisplay_cmap *);
static int  set_cursor(struct sfbp_softc *, struct wsdisplay_cursor *);
static int  get_cursor(struct sfbp_softc *, struct wsdisplay_cursor *);
static void set_curpos(struct sfbp_softc *, struct wsdisplay_curpos *);

/*
 * Compose 2 bit/pixel cursor image.  Bit order will be reversed.
 *   M M M M I I I I		M I M I M I M I
 *	[ before ]		   [ after ]
 *   3 2 1 0 3 2 1 0		0 0 1 1 2 2 3 3
 *   7 6 5 4 7 6 5 4		4 4 5 5 6 6 7 7
 */
static const uint8_t shuffle[256] = {
	0x00, 0x40, 0x10, 0x50, 0x04, 0x44, 0x14, 0x54,
	0x01, 0x41, 0x11, 0x51, 0x05, 0x45, 0x15, 0x55,
	0x80, 0xc0, 0x90, 0xd0, 0x84, 0xc4, 0x94, 0xd4,
	0x81, 0xc1, 0x91, 0xd1, 0x85, 0xc5, 0x95, 0xd5,
	0x20, 0x60, 0x30, 0x70, 0x24, 0x64, 0x34, 0x74,
	0x21, 0x61, 0x31, 0x71, 0x25, 0x65, 0x35, 0x75,
	0xa0, 0xe0, 0xb0, 0xf0, 0xa4, 0xe4, 0xb4, 0xf4,
	0xa1, 0xe1, 0xb1, 0xf1, 0xa5, 0xe5, 0xb5, 0xf5,
	0x08, 0x48, 0x18, 0x58, 0x0c, 0x4c, 0x1c, 0x5c,
	0x09, 0x49, 0x19, 0x59, 0x0d, 0x4d, 0x1d, 0x5d,
	0x88, 0xc8, 0x98, 0xd8, 0x8c, 0xcc, 0x9c, 0xdc,
	0x89, 0xc9, 0x99, 0xd9, 0x8d, 0xcd, 0x9d, 0xdd,
	0x28, 0x68, 0x38, 0x78, 0x2c, 0x6c, 0x3c, 0x7c,
	0x29, 0x69, 0x39, 0x79, 0x2d, 0x6d, 0x3d, 0x7d,
	0xa8, 0xe8, 0xb8, 0xf8, 0xac, 0xec, 0xbc, 0xfc,
	0xa9, 0xe9, 0xb9, 0xf9, 0xad, 0xed, 0xbd, 0xfd,
	0x02, 0x42, 0x12, 0x52, 0x06, 0x46, 0x16, 0x56,
	0x03, 0x43, 0x13, 0x53, 0x07, 0x47, 0x17, 0x57,
	0x82, 0xc2, 0x92, 0xd2, 0x86, 0xc6, 0x96, 0xd6,
	0x83, 0xc3, 0x93, 0xd3, 0x87, 0xc7, 0x97, 0xd7,
	0x22, 0x62, 0x32, 0x72, 0x26, 0x66, 0x36, 0x76,
	0x23, 0x63, 0x33, 0x73, 0x27, 0x67, 0x37, 0x77,
	0xa2, 0xe2, 0xb2, 0xf2, 0xa6, 0xe6, 0xb6, 0xf6,
	0xa3, 0xe3, 0xb3, 0xf3, 0xa7, 0xe7, 0xb7, 0xf7,
	0x0a, 0x4a, 0x1a, 0x5a, 0x0e, 0x4e, 0x1e, 0x5e,
	0x0b, 0x4b, 0x1b, 0x5b, 0x0f, 0x4f, 0x1f, 0x5f,
	0x8a, 0xca, 0x9a, 0xda, 0x8e, 0xce, 0x9e, 0xde,
	0x8b, 0xcb, 0x9b, 0xdb, 0x8f, 0xcf, 0x9f, 0xdf,
	0x2a, 0x6a, 0x3a, 0x7a, 0x2e, 0x6e, 0x3e, 0x7e,
	0x2b, 0x6b, 0x3b, 0x7b, 0x2f, 0x6f, 0x3f, 0x7f,
	0xaa, 0xea, 0xba, 0xfa, 0xae, 0xee, 0xbe, 0xfe,
	0xab, 0xeb, 0xbb, 0xfb, 0xaf, 0xef, 0xbf, 0xff,
};

static int
sfbpmatch(device_t parent, cfdata_t match, void *aux)
{
	struct tc_attach_args *ta = aux;

	if (strncmp("PMAGD   ", ta->ta_modname, TC_ROM_LLEN) != 0)
		return (0);

	return (1);
}

static void
sfbpattach(device_t parent, device_t self, void *aux)
{
	struct sfbp_softc *sc = device_private(self);
	struct tc_attach_args *ta = aux;
	struct rasops_info *ri;
	struct wsemuldisplaydev_attach_args waa;
	char *asic;
	int console;

	console = (ta->ta_addr == sfbp_consaddr);
	if (console) {
		sc->sc_ri = ri = &sfbp_console_ri;
		ri->ri_flg &= ~RI_NO_AUTO;
		sc->nscreens = 1;
	}
	else {
		ri = malloc(sizeof(struct rasops_info), M_DEVBUF, M_NOWAIT);
		if (ri == NULL) {
			printf(": can't alloc memory\n");
			return;
		}
		memset(ri, 0, sizeof(struct rasops_info));

		ri->ri_hw = (void *)ta->ta_addr;
		sfbp_common_init(ri);
		sc->sc_ri = ri;
	}
	printf(": %dx%d, %dbpp\n", ri->ri_width, ri->ri_height,
	    (ri->ri_depth != 32) ? 8 : 24);

	sc->sc_vaddr = ta->ta_addr;
	sc->sc_cursor.cc_magic.x = HX_MAGIC_X;
	sc->sc_cursor.cc_magic.y = HX_MAGIC_Y;
	sc->sc_blanked = sc->sc_curenb = 0;

	if (ri->ri_depth == 8) {
		sc->sc_hwops.visible = bt459visible;
		sc->sc_hwops.locate = bt459locate;
		sc->sc_hwops.shape = bt459shape;
		sc->sc_hwops.color = bt459color;
		sc->sc_hwops.setlut = bt459setlut;
		sc->sc_hwops.getlut = noplut;
	} else {
		sc->sc_hwops.visible = sfbpvisible;
		sc->sc_hwops.locate = sfbplocate;
		sc->sc_hwops.shape = sfbpshape;
		sc->sc_hwops.color = bt463color;
		sc->sc_hwops.setlut = noplut;
		sc->sc_hwops.getlut = noplut;
	}
	sfbp_cmap_init(sc);

        tc_intr_establish(parent, ta->ta_cookie, IPL_TTY, sfbpintr, sc);

	asic = (char *)ri->ri_hw + SFB_ASIC_OFFSET;
	*(uint32_t *)(asic + SFB_ASIC_CLEAR_INTR) = 0;
	*(uint32_t *)(asic + SFB_ASIC_ENABLE_INTR) = 1;

	waa.console = console;
	waa.scrdata = &sfb_screenlist;
	waa.accessops = &sfb_accessops;
	waa.accesscookie = sc;

	config_found(self, &waa, wsemuldisplaydevprint);
}

static void
sfbp_cmap_init(struct sfbp_softc *sc)
{
	struct hwcmap256 *cm;
	const uint8_t *p;
	int index;

	if (sc->sc_ri->ri_depth != 8)
		return;

	cm = &sc->sc_cmap;
	p = rasops_cmap;
	for (index = 0; index < CMAP_SIZE; index++, p += 3) {
		cm->r[index] = p[0];
		cm->g[index] = p[1];
		cm->b[index] = p[2];
	}
}

static void
sfbp_common_init(struct rasops_info *ri)
{
	char *base, *asic;
	int i, depth, hsetup, vsetup, vbase, cookie;

	base = ri->ri_hw;
	asic = base + SFB_ASIC_OFFSET;
	hsetup = *(uint32_t *)(asic + SFB_ASIC_VIDEO_HSETUP);
	vsetup = *(uint32_t *)(asic + SFB_ASIC_VIDEO_VSETUP);
	i = *(uint32_t *)(asic + SFB_ASIC_DEEP);
	depth = (i & 01) ? 32 : 8;

	/*
	 * - neglect 0,1 cases of hsetup register.
	 * - observed 804x600?, 644x480? values.
	 */

	*(uint32_t *)(asic + SFB_ASIC_VIDEO_BASE) = vbase = 1;
	vbase *= (i & 0x20) ? 2048 : 4096;	/* VRAM chip size */
	if (i & 1) vbase *= 4;			/* bytes per pixel */

	*(uint32_t *)(asic + SFB_ASIC_PLANEMASK) = ~0;
	*(uint32_t *)(asic + SFB_ASIC_PIXELMASK) = ~0;
	*(uint32_t *)(asic + SFB_ASIC_MODE) = 0;	/* MODE_SIMPLE */
	*(uint32_t *)(asic + SFB_ASIC_ROP) = 3;		/* ROP_COPY */

	/* initialize colormap and cursor hardware */
	if (depth != 32) {
		*(uint32_t *)(asic + 0x180000) = 0;	/* Bt459 reset */
		bt459init(base + SFB_RAMDAC_OFFSET);
	}
	else {
		bt463init(base + SFB_RAMDAC_OFFSET);
	}

	ri->ri_flg = RI_CENTER;
	if (ri == &sfbp_console_ri)
		ri->ri_flg |= RI_NO_AUTO;
	ri->ri_flg = 0;			/* XXX 32bpp RI_CENTER fails XXX */
	ri->ri_depth = depth;
	ri->ri_width = (hsetup & 0x1ff) << 2;
	ri->ri_height = (vsetup & 0x7ff);
	ri->ri_stride = ri->ri_width * (ri->ri_depth / 8);
	ri->ri_bits = base + 0x800000 + vbase;

	if (depth == 32) {
		ri->ri_rnum = 8;
		ri->ri_gnum = 8;
		ri->ri_bnum = 8;
		ri->ri_rpos = 16;
		ri->ri_gpos = 8;
		ri->ri_bpos = 0;
	}

	/* clear the screen */
	memset(ri->ri_bits, 0, ri->ri_stride * ri->ri_height);

	wsfont_init();
	/* prefer 12 pixel wide font */
	cookie = wsfont_find(NULL, 12, 0, 0, WSDISPLAY_FONTORDER_R2L,
	    WSDISPLAY_FONTORDER_L2R, WSFONT_FIND_BITMAP);
	if (cookie <= 0)
		cookie = wsfont_find(NULL, 0, 0, 0, WSDISPLAY_FONTORDER_R2L,
		    WSDISPLAY_FONTORDER_L2R, WSFONT_FIND_BITMAP);
	if (cookie <= 0) {
		printf("sfbp: font table is empty\n");
		return;
	}

	/* the accelerated sfbp_putchar() needs LSbit left */
	if (wsfont_lock(cookie, &ri->ri_font)) {
		printf("sfb: couldn't lock font\n");
		return;
	}
	ri->ri_wsfcookie = cookie;

	rasops_init(ri, 34, 80);

	/* add our accelerated functions */
	ri->ri_ops.putchar = sfbp_putchar;
	ri->ri_ops.erasecols = sfbp_erasecols;
	ri->ri_ops.copyrows = sfbp_copyrows;
	ri->ri_ops.eraserows = sfbp_eraserows;

	/* XXX shouldn't be global */
	sfbp_stdscreen.nrows = ri->ri_rows;
	sfbp_stdscreen.ncols = ri->ri_cols;
	sfbp_stdscreen.textops = &ri->ri_ops;
	sfbp_stdscreen.capabilities = ri->ri_caps;
	/* our accelerated putchar can't underline */
	sfbp_stdscreen.capabilities &= ~WSSCREEN_UNDERLINE;
}

static int
sfbioctl(void *v, void *vs, u_long cmd, void *data, int flag, struct lwp *l)
{
	struct sfbp_softc *sc = v;
	struct rasops_info *ri = sc->sc_ri;
	int turnoff, s;

	switch (cmd) {
	case WSDISPLAYIO_GTYPE:
		*(u_int *)data = WSDISPLAY_TYPE_SFBP;
		return (0);

	case WSDISPLAYIO_GINFO:
#define	wsd_fbip ((struct wsdisplay_fbinfo *)data)
		wsd_fbip->height = ri->ri_height;
		wsd_fbip->width = ri->ri_width;
		wsd_fbip->depth = ri->ri_depth;
		wsd_fbip->cmsize = CMAP_SIZE;	/* XXX */
#undef fbt
		return (0);

	case WSDISPLAYIO_GETCMAP:
		return get_cmap(sc, (struct wsdisplay_cmap *)data);

	case WSDISPLAYIO_PUTCMAP:
		return set_cmap(sc, (struct wsdisplay_cmap *)data);

	case WSDISPLAYIO_SVIDEO:
		turnoff = *(int *)data == WSDISPLAYIO_VIDEO_OFF;
		if ((sc->sc_blanked == 0) ^ turnoff) {
			sc->sc_blanked = turnoff;
#if 0 /* XXX later XXX */
	Low order 3bit control visibilities of screen and builtin cursor.
#endif	/* XXX XXX XXX */
		}
		return (0);

	case WSDISPLAYIO_GVIDEO:
		*(u_int *)data = sc->sc_blanked ?
		    WSDISPLAYIO_VIDEO_OFF : WSDISPLAYIO_VIDEO_ON;
		return (0);

	case WSDISPLAYIO_GCURPOS:
		*(struct wsdisplay_curpos *)data = sc->sc_cursor.cc_pos;
		return (0);

	case WSDISPLAYIO_SCURPOS:
		s = spltty();
		set_curpos(sc, (struct wsdisplay_curpos *)data);
		sc->sc_changed |= WSDISPLAY_CURSOR_DOPOS;
		splx(s);
		return (0);

	case WSDISPLAYIO_GCURMAX:
		((struct wsdisplay_curpos *)data)->x =
		((struct wsdisplay_curpos *)data)->y = CURSOR_MAX_SIZE;
		return (0);

	case WSDISPLAYIO_GCURSOR:
		return get_cursor(sc, (struct wsdisplay_cursor *)data);

	case WSDISPLAYIO_SCURSOR:
		return set_cursor(sc, (struct wsdisplay_cursor *)data);

	case WSDISPLAYIO_SMODE:
		if (*(int *)data == WSDISPLAYIO_MODE_EMUL) {
			s = spltty();
			sfbp_cmap_init(sc);
			sc->sc_curenb = 0;
			sc->sc_changed |= (WSDISPLAY_CURSOR_DOCUR |
			    WSDISPLAY_CMAP_DOLUT);
			splx(s);
		}
		return (0);
	}
	return (EPASSTHROUGH);
}

paddr_t
sfbmmap(void *v, void *vs, off_t offset, int prot)
{
	struct sfbp_softc *sc = v;

	if (offset >= 0x1000000 || offset < 0) /* XXX 16MB XXX */
		return (-1);
	return machine_btop(sc->sc_vaddr + offset);
}

static int
sfb_alloc_screen(void *v, const struct wsscreen_descr *type, void **cookiep,
    int *curxp, int *curyp, long *attrp)
{
	struct sfbp_softc *sc = v;
	struct rasops_info *ri = sc->sc_ri;
	long defattr;

	if (sc->nscreens > 0)
		return (ENOMEM);

	*cookiep = ri;		 /* one and only for now */
	*curxp = 0;
	*curyp = 0;
	(*ri->ri_ops.allocattr)(ri, 0, 0, 0, &defattr);
	*attrp = defattr;
	sc->nscreens++;
	return (0);
}

void
sfb_free_screen(void *v, void *cookie)
{
	struct sfbp_softc *sc = v;

	if (sc->sc_ri == &sfbp_console_ri)
		panic("sfb_free_screen: console");

	sc->nscreens--;
}

static int
sfb_show_screen(void *v, void *cookie, int waitok,
    void (*cb)(void *, int, int), void *cbarg)
{

	return (0);
}

int
sfbp_cnattach(tc_addr_t addr)
{
	struct rasops_info *ri;
	long defattr;

	ri = &sfbp_console_ri;
	ri->ri_hw = (void *)addr;
	sfbp_common_init(ri);
	(*ri->ri_ops.allocattr)(&ri, 0, 0, 0, &defattr);
	wsdisplay_cnattach(&sfbp_stdscreen, ri, 0, 0, defattr);
	sfbp_consaddr = addr;
	return (0);
}

static int
sfbpintr(void *arg)
{
#define	cc (&sc->sc_cursor)
	struct sfbp_softc *sc = arg;
	char *base, *asic;
	uint32_t sisr;
	int v;

	base = sc->sc_ri->ri_hw;
	asic = base + SFB_ASIC_OFFSET;
	sisr = *((uint32_t *)asic + TGA_REG_SISR);
	*(uint32_t *)(asic + SFB_ASIC_CLEAR_INTR) = 0;

	if (sc->sc_changed == 0)
		goto done;

	v = sc->sc_changed;
	if (v & WSDISPLAY_CURSOR_DOCUR)
		(*sc->sc_hwops.visible)(base, sc->sc_curenb);
	if (v & (WSDISPLAY_CURSOR_DOPOS | WSDISPLAY_CURSOR_DOHOT))
		(*sc->sc_hwops.locate)(base, cc);
	if (v & WSDISPLAY_CURSOR_DOCMAP)
		(*sc->sc_hwops.color)(base, cc->cc_color);
	if (v & WSDISPLAY_CURSOR_DOSHAPE)
		(*sc->sc_hwops.shape)(base, &cc->cc_size, cc->cc_image);
	if (v & WSDISPLAY_CMAP_DOLUT)
		(*sc->sc_hwops.setlut)(base, &sc->sc_cmap);
	sc->sc_changed = 0;
done:
	*((uint32_t *)asic + TGA_REG_SISR) = sisr = 0x00000001; tc_wmb();
	return (1);
#undef cc
}

static void
bt459init(void *vdac)
{
	const uint8_t *p;
	int i;

	SELECT(vdac, BT459_IREG_COMMAND_0);
	REG(vdac, bt_reg) = 0x40; /* CMD0 */	tc_wmb();
	REG(vdac, bt_reg) = 0x0;  /* CMD1 */	tc_wmb();
	REG(vdac, bt_reg) = 0xc0; /* CMD2 */	tc_wmb();
	REG(vdac, bt_reg) = 0xff; /* PRM */	tc_wmb();
	REG(vdac, bt_reg) = 0;    /* 205 */	tc_wmb();
	REG(vdac, bt_reg) = 0x0;  /* PBM */	tc_wmb();
	REG(vdac, bt_reg) = 0;    /* 207 */	tc_wmb();
	REG(vdac, bt_reg) = 0x0;  /* ORM */	tc_wmb();
	REG(vdac, bt_reg) = 0x0;  /* OBM */	tc_wmb();
	REG(vdac, bt_reg) = 0x0;  /* ILV */	tc_wmb();
	REG(vdac, bt_reg) = 0x0;  /* TEST */	tc_wmb();

	SELECT(vdac, BT459_IREG_CCR);
	REG(vdac, bt_reg) = 0x0;	tc_wmb();
	REG(vdac, bt_reg) = 0x0;	tc_wmb();
	REG(vdac, bt_reg) = 0x0;	tc_wmb();
	REG(vdac, bt_reg) = 0x0;	tc_wmb();
	REG(vdac, bt_reg) = 0x0;	tc_wmb();
	REG(vdac, bt_reg) = 0x0;	tc_wmb();
	REG(vdac, bt_reg) = 0x0;	tc_wmb();
	REG(vdac, bt_reg) = 0x0;	tc_wmb();
	REG(vdac, bt_reg) = 0x0;	tc_wmb();
	REG(vdac, bt_reg) = 0x0;	tc_wmb();
	REG(vdac, bt_reg) = 0x0;	tc_wmb();
	REG(vdac, bt_reg) = 0x0;	tc_wmb();
	REG(vdac, bt_reg) = 0x0;	tc_wmb();

	/* build sane colormap */
	SELECT(vdac, 0);
	p = rasops_cmap;
	for (i = 0; i < CMAP_SIZE; i++, p += 3) {
		REG(vdac, bt_cmap) = p[0];	tc_wmb();
		REG(vdac, bt_cmap) = p[1];	tc_wmb();
		REG(vdac, bt_cmap) = p[2];	tc_wmb();
	}

	/* clear out cursor image */
	SELECT(vdac, BT459_IREG_CRAM_BASE);
	for (i = 0; i < 1024; i++)
		REG(vdac, bt_reg) = 0xff;	tc_wmb();

	/*
	 * 2 bit/pixel cursor.  Assign MSB for cursor mask and LSB for
	 * cursor image.  CCOLOR_2 for mask color, while CCOLOR_3 for
	 * image color.  CCOLOR_1 will be never used.
	 */
	SELECT(vdac, BT459_IREG_CCOLOR_1);
	REG(vdac, bt_reg) = 0xff;	tc_wmb();
	REG(vdac, bt_reg) = 0xff;	tc_wmb();
	REG(vdac, bt_reg) = 0xff;	tc_wmb();

	REG(vdac, bt_reg) = 0;		tc_wmb();
	REG(vdac, bt_reg) = 0;		tc_wmb();
	REG(vdac, bt_reg) = 0;		tc_wmb();

	REG(vdac, bt_reg) = 0xff;	tc_wmb();
	REG(vdac, bt_reg) = 0xff;	tc_wmb();
	REG(vdac, bt_reg) = 0xff;	tc_wmb();
}

static void
bt463init(void *vdac)
{
	int i;

	SELECT(vdac, BT463_IREG_COMMAND_0);
	REG(vdac, bt_reg) = 0x40;	tc_wmb();	/* CMD 0 */
	REG(vdac, bt_reg) = 0x48;	tc_wmb();	/* CMD 1 */
	REG(vdac, bt_reg) = 0xc0;	tc_wmb();	/* CMD 2 */
	REG(vdac, bt_reg) = 0;		tc_wmb();	/* !? 204 !? */
	REG(vdac, bt_reg) = 0xff;	tc_wmb();	/* plane  0:7  */
	REG(vdac, bt_reg) = 0xff;	tc_wmb();	/* plane  8:15 */
	REG(vdac, bt_reg) = 0xff;	tc_wmb();	/* plane 16:23 */
	REG(vdac, bt_reg) = 0xff;	tc_wmb();	/* plane 24:27 */
	REG(vdac, bt_reg) = 0x00;	tc_wmb();	/* blink  0:7  */
	REG(vdac, bt_reg) = 0x00;	tc_wmb();	/* blink  8:15 */
	REG(vdac, bt_reg) = 0x00;	tc_wmb();	/* blink 16:23 */
	REG(vdac, bt_reg) = 0x00;	tc_wmb();	/* blink 24:27 */
	REG(vdac, bt_reg) = 0x00;	tc_wmb();

	SELECT(vdac, BT463_IREG_WINDOW_TYPE_TABLE);
	for (i = 0; i < BT463_NWTYPE_ENTRIES; i++) {
		REG(vdac, bt_reg) = 0x00;	/*   0:7  */
		REG(vdac, bt_reg) = 0xe1;	/*   8:15 */
		REG(vdac, bt_reg) = 0x81; 	/*  16:23 */
	}
}

static int
get_cmap(struct sfbp_softc *sc, struct wsdisplay_cmap *p)
{
	u_int index = p->index, count = p->count;
	int error;

	if (index >= CMAP_SIZE || count > CMAP_SIZE - index)
		return (EINVAL);

	error = copyout(&sc->sc_cmap.r[index], p->red, count);
	if (error)
		return error;
	error = copyout(&sc->sc_cmap.g[index], p->green, count);
	if (error)
		return error;
	error = copyout(&sc->sc_cmap.b[index], p->blue, count);
	return error;
}

static int
set_cmap(struct sfbp_softc *sc, struct wsdisplay_cmap *p)
{
	struct hwcmap256 cmap;
	u_int index = p->index, count = p->count;
	int error, s;

	if (index >= CMAP_SIZE || count > CMAP_SIZE - index)
		return (EINVAL);

	error = copyin(p->red, &cmap.r[index], count);
	if (error)
		return error;
	error = copyin(p->green, &cmap.g[index], count);
	if (error)
		return error;
	error = copyin(p->blue, &cmap.b[index], count);
	if (error)
		return error;

	s = spltty();
	memcpy(&sc->sc_cmap.r[index], &cmap.r[index], count);
	memcpy(&sc->sc_cmap.g[index], &cmap.g[index], count);
	memcpy(&sc->sc_cmap.b[index], &cmap.b[index], count);
	sc->sc_changed |= WSDISPLAY_CMAP_DOLUT;
	splx(s);
	return (0);
}

static int
set_cursor(struct sfbp_softc *sc, struct wsdisplay_cursor *p)
{
#define	cc (&sc->sc_cursor)
	u_int v, index = 0, count = 0, icount = 0;
	uint8_t r[2], g[2], b[2], image[512], mask[512];
	int error, s;

	v = p->which;
	if (v & WSDISPLAY_CURSOR_DOCMAP) {
		index = p->cmap.index;
		count = p->cmap.count;
		if (index >= 2 || (index + count) > 2)
			return (EINVAL);
		error = copyin(p->cmap.red, &r[index], count);
		if (error)
			return error;
		error = copyin(p->cmap.green, &g[index], count);
		if (error)
			return error;
		error = copyin(p->cmap.blue, &b[index], count);
		if (error)
			return error;
	}
	if (v & WSDISPLAY_CURSOR_DOSHAPE) {
		if (p->size.x > CURSOR_MAX_SIZE || p->size.y > CURSOR_MAX_SIZE)
			return (EINVAL);
		icount = ((p->size.x < 33) ? 4 : 8) * p->size.y;
		error = copyin(p->image, image, icount);
		if (error)
			return error;
		error = copyin(p->mask, mask, icount);
		if (error)
			return error;
	}

	s = spltty();
	if (v & WSDISPLAY_CURSOR_DOCUR)
		sc->sc_curenb = p->enable;
	if (v & WSDISPLAY_CURSOR_DOPOS)
		set_curpos(sc, &p->pos);
	if (v & WSDISPLAY_CURSOR_DOHOT)
		cc->cc_hot = p->hot;
	if (v & WSDISPLAY_CURSOR_DOCMAP) {
		memcpy(&cc->cc_color[index], &r[index], count);
		memcpy(&cc->cc_color[index + 2], &g[index], count);
		memcpy(&cc->cc_color[index + 4], &b[index], count);
	}
	if (v & WSDISPLAY_CURSOR_DOSHAPE) {
		cc->cc_size = p->size;
		memset(cc->cc_image, 0, sizeof cc->cc_image);
		memcpy(cc->cc_image, image, icount);
		memset(cc->cc_mask, 0, sizeof cc->cc_mask);
		memcpy(cc->cc_mask, mask, icount);
	}
	sc->sc_changed |= v;
	splx(s);

	return (0);
#undef cc
}

static int
get_cursor(struct sfbp_softc *sc, struct wsdisplay_cursor *p)
{
	return (EPASSTHROUGH); /* XXX */
}

static void
set_curpos(struct sfbp_softc *sc, struct wsdisplay_curpos *curpos)
{
	struct rasops_info *ri = sc->sc_ri;
	int x = curpos->x, y = curpos->y;

	if (y < 0)
		y = 0;
	else if (y > ri->ri_height)
		y = ri->ri_height;
	if (x < 0)
		x = 0;
	else if (x > ri->ri_width)
		x = ri->ri_width;
	sc->sc_cursor.cc_pos.x = x;
	sc->sc_cursor.cc_pos.y = y;
}

static void
bt459visible(void *hw, int on)
{
	hw = (char *)hw + SFB_RAMDAC_OFFSET;
	SELECT(hw, BT459_IREG_CCR);
	REG(hw, bt_reg) = (on) ? 0xc0 : 0x00;
	tc_wmb();
}

static void
sfbpvisible(void *hw, int on)
{
	/* XXX use SFBplus ASIC XX */
}

static void
bt459locate(void *hw, struct hwcursor64 *cc)
{
	int x, y, s;

	x = cc->cc_pos.x - cc->cc_hot.x;
	y = cc->cc_pos.y - cc->cc_hot.y;
	x += cc->cc_magic.x;
	y += cc->cc_magic.y;

	hw = (char *)hw + SFB_RAMDAC_OFFSET;

	s = spltty();
	SELECT(hw, BT459_IREG_CURSOR_X_LOW);
	REG(hw, bt_reg) = x;		tc_wmb();
	REG(hw, bt_reg) = x >> 8;	tc_wmb();
	REG(hw, bt_reg) = y;		tc_wmb();
	REG(hw, bt_reg) = y >> 8;	tc_wmb();
	splx(s);
}

static void
sfbplocate(void *hw, struct hwcursor64 *cc)
{
	int x, y;

	x = cc->cc_pos.x - cc->cc_hot.x;
	y = cc->cc_pos.y - cc->cc_hot.y;

	hw = (char *)hw + SFB_ASIC_OFFSET;
	*((uint32_t *)hw + TGA_REG_CXYR) = ((y & 0xfff) << 12) | (x & 0xfff);
	tc_wmb();
}

static void
bt459color(void *hw, uint8_t *cp)
{

	hw = (char *)hw + SFB_RAMDAC_OFFSET;

	SELECT(hw, BT459_IREG_CCOLOR_2);
	REG(hw, bt_reg) = cp[1]; tc_wmb();
	REG(hw, bt_reg) = cp[3]; tc_wmb();
	REG(hw, bt_reg) = cp[5]; tc_wmb();

	REG(hw, bt_reg) = cp[0]; tc_wmb();
	REG(hw, bt_reg) = cp[2]; tc_wmb();
	REG(hw, bt_reg) = cp[4]; tc_wmb();
}

static void
bt463color(void *hw, uint8_t *cp)
{
}

static void
bt459shape(void *hw, struct wsdisplay_curpos *size, uint64_t *image)
{
	uint8_t *ip, *mp, img, msk;
	uint8_t u;
	int bcnt;

	hw = (char *)hw + SFB_RAMDAC_OFFSET;
	ip = (uint8_t *)image;
	mp = (uint8_t *)(image + CURSOR_MAX_SIZE);

	bcnt = 0;
	SELECT(hw, BT459_IREG_CRAM_BASE+0);
	/* 64 pixel scan line is consisted with 16 byte cursor ram */
	while (bcnt < size->y * 16) {
		/* pad right half 32 pixel when smaller than 33 */
		if ((bcnt & 0x8) && size->x < 33) {
			REG(hw, bt_reg) = 0; tc_wmb();
			REG(hw, bt_reg) = 0; tc_wmb();
		}
		else {
			img = *ip++;
			msk = *mp++;
			img &= msk;	/* cookie off image */
			u = (msk & 0x0f) << 4 | (img & 0x0f);
			REG(hw, bt_reg) = shuffle[u];	tc_wmb();
			u = (msk & 0xf0) | (img & 0xf0) >> 4;
			REG(hw, bt_reg) = shuffle[u];	tc_wmb();
		}
		bcnt += 2;
	}
	/* pad unoccupied scan lines */
	while (bcnt < CURSOR_MAX_SIZE * 16) {
		REG(hw, bt_reg) = 0; tc_wmb();
		REG(hw, bt_reg) = 0; tc_wmb();
		bcnt += 2;
	}
}

static void
sfbpshape(void *hw, struct wsdisplay_curpos *size, uint64_t *image)
{
	/* XXX use SFBplus ASIC XXX */
}

static void
bt459setlut(void *hw, struct hwcmap256 *cm)
{
	int index;

	hw = (char *)hw + SFB_RAMDAC_OFFSET;
	SELECT(hw, 0);
	for (index = 0; index < CMAP_SIZE; index++) {
		REG(hw, bt_cmap) = cm->r[index];	tc_wmb();
		REG(hw, bt_cmap) = cm->g[index];	tc_wmb();
		REG(hw, bt_cmap) = cm->b[index];	tc_wmb();
	}
}

static void
noplut(void *hw, struct hwcmap256 *cm)
{
}

#define SFBBPP 32

#define	MODE_SIMPLE		0
#define	MODE_OPAQUESTIPPLE	1
#define	MODE_OPAQUELINE		2
#define	MODE_TRANSPARENTSTIPPLE	5
#define	MODE_TRANSPARENTLINE	6
#define	MODE_COPY		7

#if SFBBPP == 8
/* parameters for 8bpp configuration */
#define	SFBALIGNMASK		0x7
#define	SFBPIXELBYTES		1
#define	SFBSTIPPLEALL1		0xffffffff
#define	SFBSTIPPLEBITS		32
#define	SFBSTIPPLEBITMASK	0x1f
#define	SFBSTIPPLEBYTESDONE	32
#define	SFBCOPYALL1		0xffffffff
#define	SFBCOPYBITS		32
#define	SFBCOPYBITMASK		0x1f
#define	SFBCOPYBYTESDONE	32

#elif SFBBPP == 32
/* parameters for 32bpp configuration */
#define	SFBALIGNMASK		0x7
#define	SFBPIXELBYTES		4
#define	SFBSTIPPLEALL1		0x0000ffff
#define	SFBSTIPPLEBITS		16
#define	SFBSTIPPLEBITMASK	0xf
#define	SFBSTIPPLEBYTESDONE	32
#define	SFBCOPYALL1		0x000000ff
#define	SFBCOPYBITS		8
#define	SFBCOPYBITMASK		0x3
#define	SFBCOPYBYTESDONE	32
#endif

#ifdef pmax
#define	WRITE_MB()
#define	BUMP(p) (p)
#endif

#ifdef alpha
#define	WRITE_MB() tc_wmb()
/* registers is replicated in 1KB stride; rap round 4th iteration */
#define	BUMP(p) ((p) = (void *)(((long)(p) + 0x400) & ~0x1000))
#endif

#define	SFBMODE(p, v) \
		(*(uint32_t *)(BUMP(p) + SFB_ASIC_MODE) = (v))
#define	SFBROP(p, v) \
		(*(uint32_t *)(BUMP(p) + SFB_ASIC_ROP) = (v))
#define	SFBPLANEMASK(p, v) \
		(*(uint32_t *)(BUMP(p) + SFB_ASIC_PLANEMASK) = (v))
#define	SFBPIXELMASK(p, v) \
		(*(uint32_t *)(BUMP(p) + SFB_ASIC_PIXELMASK) = (v))
#define	SFBADDRESS(p, v) \
		(*(uint32_t *)(BUMP(p) + SFB_ASIC_ADDRESS) = (v))
#define	SFBSTART(p, v) \
		(*(uint32_t *)(BUMP(p) + SFB_ASIC_START) = (v))
#define	SFBPIXELSHIFT(p, v) \
		(*(uint32_t *)(BUMP(p) + SFB_ASIC_PIXELSHIFT) = (v))
#define	SFBFG(p, v) \
		(*(uint32_t *)(BUMP(p) + SFB_ASIC_FG) = (v))
#define	SFBBG(p, v) \
		(*(uint32_t *)(BUMP(p) + SFB_ASIC_BG) = (v))
#define	SFBBCONT(p, v) \
		(*(uint32_t *)(BUMP(p) + SFB_ASIC_BCONT) = (v))

#define	SFBDATA(p, v) \
		(*((uint32_t *)BUMP(p) + TGA_REG_GDAR) = (v))

#define	SFBCOPY64BYTESDONE	8
#define	SFBCOPY64BITS		64
#define	SFBCOPY64SRC(p, v) \
		(*((uint32_t *)BUMP(p) + TGA_REG_GCSR) = (long)(v))
#define	SFBCOPY64DST(p, v) \
		(*((uint32_t *)BUMP(p) + TGA_REG_GCDR) = (long)(v))

/*
 * Actually write a string to the frame buffer.
 */
static void
sfbp_putchar(void *id, int row, int col, u_int uc, long attr)
{
	struct rasops_info *ri = id;
	char *sfb, *p;
	int scanspan, height, width, align, x, y;
	uint32_t lmask, rmask, glyph;
	uint8_t *g;

	x = col * ri->ri_font->fontwidth;
	y = row * ri->ri_font->fontheight;
	scanspan = ri->ri_stride;
	height = ri->ri_font->fontheight;
	uc -= ri->ri_font->firstchar;
	g = (u_char *)ri->ri_font->data + uc * ri->ri_fontscale;

	p = ri->ri_bits + y * scanspan + x * SFBPIXELBYTES;
	align = (long)p & SFBALIGNMASK;
	p -= align;
	align /= SFBPIXELBYTES;
	width = ri->ri_font->fontwidth + align;
	lmask = SFBSTIPPLEALL1 << align;
	rmask = SFBSTIPPLEALL1 >> (-width & SFBSTIPPLEBITMASK);
	sfb = (char *)ri->ri_hw + SFB_ASIC_OFFSET;

	SFBMODE(sfb, MODE_OPAQUESTIPPLE);
	SFBPLANEMASK(sfb, ~0);
	SFBFG(sfb, ri->ri_devcmap[(attr >> 24) & 15]);
	SFBBG(sfb, ri->ri_devcmap[(attr >> 16) & 15]);
	SFBROP(sfb, (3 << 8) | 3); /* ROP_COPY24 */
	*((uint32_t *)sfb + TGA_REG_GPXR_P) = lmask & rmask;

	/* XXX 2B stride fonts only XXX */
	while (height > 0) {
		glyph = *(uint16_t *)g;			/* XXX */
		*(uint32_t *)p = glyph << align;
		p += scanspan;
		g += 2;					/* XXX */
		height--;
	}
	SFBMODE(sfb, MODE_SIMPLE);
	*((uint32_t *)sfb + TGA_REG_GPXR_P) = ~0;
}

#undef	SFBSTIPPLEALL1
#undef	SFBSTIPPLEBITS
#undef	SFBSTIPPLEBITMASK
#define	SFBSTIPPLEALL1		SFBCOPYALL1
#define	SFBSTIPPLEBITS		SFBCOPYBITS
#define	SFBSTIPPLEBITMASK	SFBCOPYBITMASK

/*
 * Clear characters in a line.
 */
static void
sfbp_erasecols(void *id, int row, int startcol, int ncols, long attr)
{
	struct rasops_info *ri = id;
	char *sfb, *p;
	int scanspan, startx, height, width, align, w, y;
	uint32_t lmask, rmask;

	scanspan = ri->ri_stride;
	y = row * ri->ri_font->fontheight;
	startx = startcol * ri->ri_font->fontwidth;
	height = ri->ri_font->fontheight;
	w = ri->ri_font->fontwidth * ncols;

	p = ri->ri_bits + y * scanspan + startx * SFBPIXELBYTES;
	align = (long)p & SFBALIGNMASK;
	align /= SFBPIXELBYTES;
	p -= align;
	width = w + align;
	lmask = SFBSTIPPLEALL1 << align;
	rmask = SFBSTIPPLEALL1 >> (-width & SFBSTIPPLEBITMASK);
	sfb = (char *)ri->ri_hw + SFB_ASIC_OFFSET;

	SFBMODE(sfb, MODE_TRANSPARENTSTIPPLE);
	SFBPLANEMASK(sfb, ~0);
	SFBFG(sfb, ri->ri_devcmap[(attr >> 16) & 15]); /* fill with bg */
	if (width <= SFBSTIPPLEBITS) {
		lmask = lmask & rmask;
		while (height > 0) {
			*(uint32_t *)p = lmask;
			p += scanspan;
			height--;
		}
	}
	else {
		char *q = p;
		while (height > 0) {
			*(uint32_t *)p = lmask;
			WRITE_MB();
			width -= 2 * SFBSTIPPLEBITS;
			while (width > 0) {
				p += SFBSTIPPLEBYTESDONE;
				*(uint32_t *)p = SFBSTIPPLEALL1;
				WRITE_MB();
				width -= SFBSTIPPLEBITS;
			}
			p += SFBSTIPPLEBYTESDONE;
			*(uint32_t *)p = rmask;
			WRITE_MB();

			p = (q += scanspan);
			width = w + align;
			height--;
		}
	}
	SFBMODE(sfb, MODE_SIMPLE);
}

#if 1
/*
 * Copy lines.
 */
static void
sfbp_copyrows(void *id, int srcrow, int dstrow, int nrows)
{
	struct rasops_info *ri = id;
	char *sfb, *p;
	int scanspan, offset, srcy, height, width, align, w;
	uint32_t lmask, rmask;

	scanspan = ri->ri_stride;
	height = ri->ri_font->fontheight * nrows;
	offset = (dstrow - srcrow) * ri->ri_yscale;
	srcy = ri->ri_font->fontheight * srcrow;
	if (srcrow < dstrow && srcrow + nrows > dstrow) {
		scanspan = -scanspan;
		srcy += height;
	}

	p = ri->ri_bits + srcy * ri->ri_stride;
	align = (long)p & SFBALIGNMASK;
	p -= align;
	align /= SFBPIXELBYTES;
	w = ri->ri_emuwidth;
	width = w + align;
	lmask = SFBCOPYALL1 << align;
	rmask = SFBCOPYALL1 >> (-width & SFBCOPYBITMASK);
	sfb = (char *)ri->ri_hw + SFB_ASIC_OFFSET;

	SFBMODE(sfb, MODE_COPY);
	SFBPLANEMASK(sfb, ~0);
	SFBPIXELSHIFT(sfb, 0);
	if (width <= SFBCOPYBITS) {
		/* never happens */;
	}
	else {
		char *q = p;
		while (height > 0) {
			*(uint32_t *)p = lmask;
			*(uint32_t *)(p + offset) = lmask;
			width -= 2 * SFBCOPYBITS;
			while (width > 0) {
				p += SFBCOPYBYTESDONE;
				*(uint32_t *)p = SFBCOPYALL1;
				*(uint32_t *)(p + offset) = SFBCOPYALL1;
				width -= SFBCOPYBITS;
			}
			p += SFBCOPYBYTESDONE;
			*(uint32_t *)p = rmask;
			*(uint32_t *)(p + offset) = rmask;

			p = (q += scanspan);
			width = w + align;
			height--;
		}
	}
	SFBMODE(sfb, MODE_SIMPLE);
}

#else


static void
sfbp_copyrows(void *id, int srcrow, int dstrow, int nrows)
{
	struct rasops_info *ri = id;
	void *sfb, *p, *q;
	int scanspan, offset, srcy, height, width, w, align;
	uint32_t rmask, lmask;

	scanspan = ri->ri_stride;
	height = ri->ri_font->fontheight * nrows;
	offset = (dstrow - srcrow) * ri->ri_yscale;
	srcy = ri->ri_font->fontheight * srcrow;
	if (srcrow < dstrow && srcrow + nrows > dstrow) {
		scanspan = -scanspan;
		srcy += height;
	}

	p = ri->ri_bits + srcy * ri->ri_stride;
	align = (long)p & SFBALIGNMASK;
	w = ri->ri_emuwidth;
	width = w + align;
	lmask = SFBCOPYALL1 << align;
	rmask = SFBCOPYALL1 >> (-width & SFBCOPYBITMASK);
	sfb = (void *)ri->ri_hw + SFB_ASIC_OFFSET;
	q = p;

	SFBMODE(sfb, MODE_COPY);
	SFBPLANEMASK(sfb, ~0);
	SFBPIXELSHIFT(sfb, 0);

	if (width <= SFBCOPYBITS)
		; /* never happens */
	else if (width < SFBCOPY64BITS) {
		; /* unlikely happens */

	}
	else {
		while (height > 0) {
			while (width >= SFBCOPY64BITS) {
				SFBCOPY64SRC(sfb, *p);
				SFBCOPY64DST(sfb, *p + offset);
				p += SFBCOPY64BYTESDONE;
				width -= SFBCOPY64BITS;
			}
			if (width >= SFBCOPYBITS) {
				*(uint32_t *)p = SFBCOPYALL1;
				*(uint32_t *)(p + offset) = SFBCOPYALL1;
				p += SFBCOPYBYTESDONE;
				width -= SFBCOPYBITS;
			}
			if (width > 0) {
				*(uint32_t *)p = rmask;
				*(uint32_t *)(p + offset) = rmask;
			}

			p = (q += scanspan);
			width = w;
			height--;
		}
	}
	SFBMODE(sfb, MODE_SIMPLE);
}
#endif

/*
 * Erase lines.
 */
static void
sfbp_eraserows(void *id, int startrow, int nrows, long attr)
{
	struct rasops_info *ri = id;
	char *sfb, *p;
	int scanspan, starty, height, width, align, w;
	uint32_t lmask, rmask;

	scanspan = ri->ri_stride;
	starty = ri->ri_font->fontheight * startrow;
	height = ri->ri_font->fontheight * nrows;

	p = ri->ri_bits + starty * scanspan;
	align = (long)p & SFBALIGNMASK;
	p -= align;
	align /= SFBPIXELBYTES;
	w = ri->ri_emuwidth * SFBPIXELBYTES;
	width = w + align;
	lmask = SFBSTIPPLEALL1 << align;
	rmask = SFBSTIPPLEALL1 >> (-width & SFBSTIPPLEBITMASK);
	sfb = (char *)ri->ri_hw + SFB_ASIC_OFFSET;

	SFBMODE(sfb, MODE_TRANSPARENTSTIPPLE);
	SFBPLANEMASK(sfb, ~0);
	SFBFG(sfb, ri->ri_devcmap[(attr >> 16) & 15]);
	if (width <= SFBSTIPPLEBITS) {
		/* never happens */;
	}
	else {
		char *q = p;
		while (height > 0) {
			*(uint32_t *)p = lmask;
			WRITE_MB();
			width -= 2 * SFBSTIPPLEBITS;
			while (width > 0) {
				p += SFBSTIPPLEBYTESDONE;
				*(uint32_t *)p = SFBSTIPPLEALL1;
				WRITE_MB();
				width -= SFBSTIPPLEBITS;
			}
			p += SFBSTIPPLEBYTESDONE;
			*(uint32_t *)p = rmask;
			WRITE_MB();

			p = (q += scanspan);
			width = w + align;
			height--;
		}
	}
	SFBMODE(sfb, MODE_SIMPLE);
}
