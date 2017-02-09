/* $NetBSD: cfb.c,v 1.61 2012/01/11 21:12:36 macallan Exp $ */

/*-
 * Copyright (c) 1998, 1999 The NetBSD Foundation, Inc.
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
__KERNEL_RCSID(0, "$NetBSD: cfb.c,v 1.61 2012/01/11 21:12:36 macallan Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
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

#if defined(pmax)
#define	machine_btop(x) mips_btop(MIPS_KSEG1_TO_PHYS(x))
#endif

#if defined(alpha)
#define	machine_btop(x) alpha_btop(ALPHA_K0SEG_TO_PHYS(x))
#endif

/*
 * N.B., Bt459 registers are 8bit width.  Some of TC framebuffers have
 * obscure register layout such as 2nd and 3rd Bt459 registers are
 * adjacent each other in a word, i.e.,
 *	struct bt459triplet {
 * 		struct {
 *			uint8_t u0;
 *			uint8_t u1;
 *			uint8_t u2;
 *			unsigned :8;
 *		} bt_lo;
 *		...
 * Although CX has single Bt459, 32bit R/W can be done w/o any trouble.
 *	struct bt459reg {
 *		   uint32_t	   bt_lo;
 *		   uint32_t	   bt_hi;
 *		   uint32_t	   bt_reg;
 *		   uint32_t	   bt_cmap;
 *	};
 */

/* Bt459 hardware registers, memory-mapped in 32bit stride */
#define	bt_lo	0x0
#define	bt_hi	0x4
#define	bt_reg	0x8
#define	bt_cmap 0xc

#define	REGWRITE32(p,i,v) do {					\
	*(volatile uint32_t *)((p) + (i)) = (v); tc_wmb();	\
    } while (0)
#define	VDACSELECT(p,r) do {					\
	REGWRITE32(p, bt_lo, 0xff & (r));			\
	REGWRITE32(p, bt_hi, 0x0f & ((r)>>8));			\
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

struct cfb_softc {
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
};

#define	CX_MAGIC_X	220
#define	CX_MAGIC_Y 	35

#define	CX_FB_OFFSET	0x000000
#define	CX_FB_SIZE	0x100000
#define	CX_BT459_OFFSET	0x200000
#define	CX_OFFSET_IREQ	0x300000	/* Interrupt req. control */

static int  cfbmatch(device_t, cfdata_t, void *);
static void cfbattach(device_t, device_t, void *);

CFATTACH_DECL_NEW(cfb, sizeof(struct cfb_softc),
    cfbmatch, cfbattach, NULL, NULL);

static void cfb_common_init(struct rasops_info *);
static struct rasops_info cfb_console_ri;
static tc_addr_t cfb_consaddr;

static struct wsscreen_descr cfb_stdscreen = {
	"std", 0, 0,
	0, /* textops */
	0, 0,
	WSSCREEN_REVERSE
};

static const struct wsscreen_descr *_cfb_scrlist[] = {
	&cfb_stdscreen,
};

static const struct wsscreen_list cfb_screenlist = {
	sizeof(_cfb_scrlist) / sizeof(struct wsscreen_descr *), _cfb_scrlist
};

static int	cfbioctl(void *, void *, u_long, void *, int, struct lwp *);
static paddr_t	cfbmmap(void *, void *, off_t, int);

static int	cfb_alloc_screen(void *, const struct wsscreen_descr *,
				      void **, int *, int *, long *);
static void	cfb_free_screen(void *, void *);
static int	cfb_show_screen(void *, void *, int,
				     void (*) (void *, int, int), void *);

static const struct wsdisplay_accessops cfb_accessops = {
	cfbioctl,
	cfbmmap,
	cfb_alloc_screen,
	cfb_free_screen,
	cfb_show_screen,
	0 /* load_font */
};

int  cfb_cnattach(tc_addr_t);
static int  cfbintr(void *);
static void cfbhwinit(void *);
static void cfb_cmap_init(struct cfb_softc *);

static int  get_cmap(struct cfb_softc *, struct wsdisplay_cmap *);
static int  set_cmap(struct cfb_softc *, struct wsdisplay_cmap *);
static int  set_cursor(struct cfb_softc *, struct wsdisplay_cursor *);
static int  get_cursor(struct cfb_softc *, struct wsdisplay_cursor *);
static void set_curpos(struct cfb_softc *, struct wsdisplay_curpos *);

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
cfbmatch(device_t parent, cfdata_t match, void *aux)
{
	struct tc_attach_args *ta = aux;

	if (strncmp("PMAG-BA ", ta->ta_modname, TC_ROM_LLEN) != 0)
		return (0);

	return (1);
}

static void
cfbattach(device_t parent, device_t self, void *aux)
{
	struct cfb_softc *sc = device_private(self);
	struct tc_attach_args *ta = aux;
	struct rasops_info *ri;
	struct wsemuldisplaydev_attach_args waa;
	int console;

	console = (ta->ta_addr == cfb_consaddr);
	if (console) {
		sc->sc_ri = ri = &cfb_console_ri;
		ri->ri_flg &= ~RI_NO_AUTO;
		sc->nscreens = 1;
	}
	else {
		ri = malloc(sizeof(struct rasops_info),
			M_DEVBUF, M_NOWAIT|M_ZERO);
		if (ri == NULL) {
			printf(": can't alloc memory\n");
			return;
		}

		ri->ri_hw = (void *)ta->ta_addr;
		cfb_common_init(ri);
		sc->sc_ri = ri;
	}
	printf(": %dx%d, %dbpp\n", ri->ri_width, ri->ri_height, ri->ri_depth);

	cfb_cmap_init(sc);

	sc->sc_vaddr = ta->ta_addr;
	sc->sc_cursor.cc_magic.x = CX_MAGIC_X;
	sc->sc_cursor.cc_magic.y = CX_MAGIC_Y;
	sc->sc_blanked = sc->sc_curenb = 0;

	tc_intr_establish(parent, ta->ta_cookie, IPL_TTY, cfbintr, sc);

	/* clear any pending interrupts */
	*(volatile uint8_t *)((char *)ri->ri_hw + CX_OFFSET_IREQ) = 0;

	waa.console = console;
	waa.scrdata = &cfb_screenlist;
	waa.accessops = &cfb_accessops;
	waa.accesscookie = sc;

	config_found(self, &waa, wsemuldisplaydevprint);
}

static void
cfb_cmap_init(struct cfb_softc *sc)
{
	struct hwcmap256 *cm;
	const uint8_t *p;
	int index;

	cm = &sc->sc_cmap;
	p = rasops_cmap;
	for (index = 0; index < CMAP_SIZE; index++, p += 3) {
		cm->r[index] = p[0];
		cm->g[index] = p[1];
		cm->b[index] = p[2];
	}
}

static void
cfb_common_init(struct rasops_info *ri)
{
	char *base;
	int cookie;

	base = (void *)ri->ri_hw;

	/* initialize colormap and cursor hardware */
	cfbhwinit(base);

	ri->ri_flg = RI_CENTER;
	if (ri == &cfb_console_ri)
		ri->ri_flg |= RI_NO_AUTO;
	ri->ri_depth = 8;
	ri->ri_width = 1024;
	ri->ri_height = 864;
	ri->ri_stride = 1024;
	ri->ri_bits = base + CX_FB_OFFSET;

	/* clear the screen */
	memset(ri->ri_bits, 0, ri->ri_stride * ri->ri_height);

	wsfont_init();
	/* prefer 12 pixel wide font */
	cookie = wsfont_find(NULL, 12, 0, 0, WSDISPLAY_FONTORDER_L2R,
	    WSDISPLAY_FONTORDER_L2R, WSFONT_FIND_BITMAP);
	if (cookie <= 0)
		cookie = wsfont_find(NULL, 0, 0, 0, WSDISPLAY_FONTORDER_L2R,
		    WSDISPLAY_FONTORDER_L2R, WSFONT_FIND_BITMAP);
	if (cookie <= 0) {
		printf("cfb: font table is empty\n");
		return;
	}

	if (wsfont_lock(cookie, &ri->ri_font)) {
		printf("cfb: couldn't lock font\n");
		return;
	}
	ri->ri_wsfcookie = cookie;

	rasops_init(ri, 34, 80);

	/* XXX shouldn't be global */
	cfb_stdscreen.nrows = ri->ri_rows;
	cfb_stdscreen.ncols = ri->ri_cols;
	cfb_stdscreen.textops = &ri->ri_ops;
	cfb_stdscreen.capabilities = ri->ri_caps;
}

static int
cfbioctl(void *v, void *vs, u_long cmd, void *data, int flag, struct lwp *l)
{
	struct cfb_softc *sc = v;
	struct rasops_info *ri = sc->sc_ri;
	int turnoff, s;

	switch (cmd) {
	case WSDISPLAYIO_GTYPE:
		*(u_int *)data = WSDISPLAY_TYPE_CFB;
		return (0);

	case WSDISPLAYIO_GINFO:
#define	wsd_fbip ((struct wsdisplay_fbinfo *)data)
		wsd_fbip->height = ri->ri_height;
		wsd_fbip->width = ri->ri_width;
		wsd_fbip->depth = ri->ri_depth;
		wsd_fbip->cmsize = CMAP_SIZE;
#undef fbt
		return (0);

	case WSDISPLAYIO_GETCMAP:
		return get_cmap(sc, (struct wsdisplay_cmap *)data);

	case WSDISPLAYIO_PUTCMAP:
		return set_cmap(sc, (struct wsdisplay_cmap *)data);

	case WSDISPLAYIO_SVIDEO:
		turnoff = *(int *)data == WSDISPLAYIO_VIDEO_OFF;
		if (sc->sc_blanked != turnoff) {
			sc->sc_blanked = turnoff;
			/* XXX later XXX */
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
			cfb_cmap_init(sc);
			sc->sc_curenb = 0;
			sc->sc_blanked = 0;
			sc->sc_changed |= (WSDISPLAY_CURSOR_DOCUR |
			    WSDISPLAY_CMAP_DOLUT);
			splx(s);
		}
		return (0);
	}
	return EPASSTHROUGH;
}

paddr_t
cfbmmap(void *v, void *vs, off_t offset, int prot)
{
	struct cfb_softc *sc = v;

	if (offset >= CX_FB_SIZE || offset < 0)
		return (-1);
	return machine_btop(sc->sc_vaddr + CX_FB_OFFSET + offset);
}

static int
cfb_alloc_screen(void *v, const struct wsscreen_descr *type, void **cookiep,
    int *curxp, int *curyp, long *attrp)
{
	struct cfb_softc *sc = v;
	struct rasops_info *ri = sc->sc_ri;
	long defattr;

	if (sc->nscreens > 0)
		return (ENOMEM);

	*cookiep = ri;	 /* one and only for now */
	*curxp = 0;
	*curyp = 0;
	(*ri->ri_ops.allocattr)(ri, 0, 0, 0, &defattr);
	*attrp = defattr;
	sc->nscreens++;
	return (0);
}

static void
cfb_free_screen(void *v, void *cookie)
{
	struct cfb_softc *sc = v;

	if (sc->sc_ri == &cfb_console_ri)
		panic("cfb_free_screen: console");

	sc->nscreens--;
}

static int
cfb_show_screen(void *v, void *cookie, int waitok,
    void (*cb)(void *, int, int), void *cbarg)
{

	return (0);
}

/* EXPORT */ int
cfb_cnattach(tc_addr_t addr)
{
	struct rasops_info *ri;
	long defattr;

	ri = &cfb_console_ri;
	ri->ri_hw = (void *)addr;
	cfb_common_init(ri);
	(*ri->ri_ops.allocattr)(ri, 0, 0, 0, &defattr);
	wsdisplay_cnattach(&cfb_stdscreen, ri, 0, 0, defattr);
	cfb_consaddr = addr;
	return(0);
}

static int
cfbintr(void *arg)
{
	struct cfb_softc *sc = arg;
	char *base, *vdac;
	int v;

	base = (void *)sc->sc_ri->ri_hw;
	*(uint8_t *)(base + CX_OFFSET_IREQ) = 0;
	if (sc->sc_changed == 0)
		return (1);

	vdac = base + CX_BT459_OFFSET;
	v = sc->sc_changed;
	if (v & WSDISPLAY_CURSOR_DOCUR) {
		VDACSELECT(vdac, BT459_IREG_CCR);
		REGWRITE32(vdac, bt_reg, (sc->sc_curenb) ? 0xc0 : 0x00);
	}
	if (v & (WSDISPLAY_CURSOR_DOPOS | WSDISPLAY_CURSOR_DOHOT)) {
		int x, y;

		x = sc->sc_cursor.cc_pos.x - sc->sc_cursor.cc_hot.x;
		y = sc->sc_cursor.cc_pos.y - sc->sc_cursor.cc_hot.y;

		x += sc->sc_cursor.cc_magic.x;
		y += sc->sc_cursor.cc_magic.y;

		VDACSELECT(vdac, BT459_IREG_CURSOR_X_LOW);
		REGWRITE32(vdac, bt_reg, x);
		REGWRITE32(vdac, bt_reg, x >> 8);
		REGWRITE32(vdac, bt_reg, y);
		REGWRITE32(vdac, bt_reg, y >> 8);
	}
	if (v & WSDISPLAY_CURSOR_DOCMAP) {
		uint8_t *cp = sc->sc_cursor.cc_color;

		VDACSELECT(vdac, BT459_IREG_CCOLOR_2);
		REGWRITE32(vdac, bt_reg, cp[1]);
		REGWRITE32(vdac, bt_reg, cp[3]);
		REGWRITE32(vdac, bt_reg, cp[5]);

		REGWRITE32(vdac, bt_reg, cp[0]);
		REGWRITE32(vdac, bt_reg, cp[2]);
		REGWRITE32(vdac, bt_reg, cp[4]);
	}
	if (v & WSDISPLAY_CURSOR_DOSHAPE) {
		uint8_t *ip, *mp, img, msk;
		uint8_t u;
		int bcnt;

		ip = (uint8_t *)sc->sc_cursor.cc_image;
		mp = (uint8_t *)sc->sc_cursor.cc_mask;

		bcnt = 0;
		VDACSELECT(vdac, BT459_IREG_CRAM_BASE+0);
		/* 64 pixel scan line is consisted with 16 byte cursor ram */
		while (bcnt < sc->sc_cursor.cc_size.y * 16) {
			/* pad right half 32 pixel when smaller than 33 */
			if ((bcnt & 0x8) && sc->sc_cursor.cc_size.x < 33) {
				REGWRITE32(vdac, bt_reg, 0);
				REGWRITE32(vdac, bt_reg, 0);
			}
			else {
				img = *ip++;
				msk = *mp++;
				img &= msk;	/* cookie off image */
				u = (msk & 0x0f) << 4 | (img & 0x0f);
				REGWRITE32(vdac, bt_reg, shuffle[u]);
				u = (msk & 0xf0) | (img & 0xf0) >> 4;
				REGWRITE32(vdac, bt_reg, shuffle[u]);
			}
			bcnt += 2;
		}
		/* pad unoccupied scan lines */
		while (bcnt < CURSOR_MAX_SIZE * 16) {
			REGWRITE32(vdac, bt_reg, 0);
			REGWRITE32(vdac, bt_reg, 0);
			bcnt += 2;
		}
	}
	if (v & WSDISPLAY_CMAP_DOLUT) {
		struct hwcmap256 *cm = &sc->sc_cmap;
		int index;

		VDACSELECT(vdac, 0);
		for (index = 0; index < CMAP_SIZE; index++) {
			REGWRITE32(vdac, bt_cmap, cm->r[index]);
			REGWRITE32(vdac, bt_cmap, cm->g[index]);
			REGWRITE32(vdac, bt_cmap, cm->b[index]);
		}
	}
	sc->sc_changed = 0;
	return (1);
}

static void
cfbhwinit(void *cfbbase)
{
	char *vdac = (char *)cfbbase + CX_BT459_OFFSET;
	const uint8_t *p;
	int i;

	VDACSELECT(vdac, BT459_IREG_COMMAND_0);
	REGWRITE32(vdac, bt_reg, 0x40); /* CMD0 */
	REGWRITE32(vdac, bt_reg, 0x0);  /* CMD1 */
	REGWRITE32(vdac, bt_reg, 0xc0); /* CMD2 */
	REGWRITE32(vdac, bt_reg, 0xff); /* PRM */
	REGWRITE32(vdac, bt_reg, 0);    /* 205 */
	REGWRITE32(vdac, bt_reg, 0x0);  /* PBM */
	REGWRITE32(vdac, bt_reg, 0);    /* 207 */
	REGWRITE32(vdac, bt_reg, 0x0);  /* ORM */
	REGWRITE32(vdac, bt_reg, 0x0);  /* OBM */
	REGWRITE32(vdac, bt_reg, 0x0);  /* ILV */
	REGWRITE32(vdac, bt_reg, 0x0);  /* TEST */

	VDACSELECT(vdac, BT459_IREG_CCR);
	REGWRITE32(vdac, bt_reg, 0x0);
	REGWRITE32(vdac, bt_reg, 0x0);
	REGWRITE32(vdac, bt_reg, 0x0);
	REGWRITE32(vdac, bt_reg, 0x0);
	REGWRITE32(vdac, bt_reg, 0x0);
	REGWRITE32(vdac, bt_reg, 0x0);
	REGWRITE32(vdac, bt_reg, 0x0);
	REGWRITE32(vdac, bt_reg, 0x0);
	REGWRITE32(vdac, bt_reg, 0x0);
	REGWRITE32(vdac, bt_reg, 0x0);
	REGWRITE32(vdac, bt_reg, 0x0);
	REGWRITE32(vdac, bt_reg, 0x0);
	REGWRITE32(vdac, bt_reg, 0x0);

	/* build sane colormap */
	VDACSELECT(vdac, 0);
	p = rasops_cmap;
	for (i = 0; i < CMAP_SIZE; i++, p += 3) {
		REGWRITE32(vdac, bt_cmap, p[0]);
		REGWRITE32(vdac, bt_cmap, p[1]);
		REGWRITE32(vdac, bt_cmap, p[2]);
	}

	/* clear out cursor image */
	VDACSELECT(vdac, BT459_IREG_CRAM_BASE);
	for (i = 0; i < 1024; i++)
		REGWRITE32(vdac, bt_reg, 0xff);

	/*
	 * 2 bit/pixel cursor.  Assign MSB for cursor mask and LSB for
	 * cursor image.  CCOLOR_2 for mask color, while CCOLOR_3 for
	 * image color.  CCOLOR_1 will be never used.
	 */
	VDACSELECT(vdac, BT459_IREG_CCOLOR_1);
	REGWRITE32(vdac, bt_reg, 0xff);
	REGWRITE32(vdac, bt_reg, 0xff);
	REGWRITE32(vdac, bt_reg, 0xff);

	REGWRITE32(vdac, bt_reg, 0);
	REGWRITE32(vdac, bt_reg, 0);
	REGWRITE32(vdac, bt_reg, 0);

	REGWRITE32(vdac, bt_reg, 0xff);
	REGWRITE32(vdac, bt_reg, 0xff);
	REGWRITE32(vdac, bt_reg, 0xff);
}

static int
get_cmap(struct cfb_softc *sc, struct wsdisplay_cmap *p)
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
set_cmap(struct cfb_softc *sc, struct wsdisplay_cmap *p)
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
set_cursor(struct cfb_softc *sc, struct wsdisplay_cursor *p)
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
get_cursor(struct cfb_softc *sc, struct wsdisplay_cursor *p)
{
	return (EPASSTHROUGH); /* XXX */
}

static void
set_curpos(struct cfb_softc *sc, struct wsdisplay_curpos *curpos)
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
