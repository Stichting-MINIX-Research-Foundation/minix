/* $NetBSD: tfb.c,v 1.61 2012/01/11 21:12:36 macallan Exp $ */

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
__KERNEL_RCSID(0, "$NetBSD: tfb.c,v 1.61 2012/01/11 21:12:36 macallan Exp $");

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
#include <dev/ic/bt463reg.h>
#include <dev/ic/bt431reg.h>

#if defined(pmax)
#define	machine_btop(x) mips_btop(MIPS_KSEG1_TO_PHYS(x))
#endif

#if defined(alpha)
#define	machine_btop(x) alpha_btop(ALPHA_K0SEG_TO_PHYS(x))
#endif

/*
 * struct bt463reg {
 * 	uint8_t		bt_lo;
 * 	unsigned : 24;
 * 	uint8_t		bt_hi;
 * 	unsigned : 24;
 * 	uint8_t		bt_reg;
 * 	unsigned : 24;
 * 	uint8_t		bt_cmap;
 * };
 *
 * N.B. a pair of Bt431s are located adjascently.
 * 	struct bt431twin {
 *		struct {
 *			uint8_t u0;	for sprite mask
 *			uint8_t u1;	for sprite image
 *			unsigned :16;
 *		} bt_lo;
 *		...
 *
 * struct bt431reg {
 * 	uint16_t	bt_lo;
 * 	unsigned : 16;
 * 	uint16_t	bt_hi;
 * 	unsigned : 16;
 * 	uint16_t	bt_ram;
 * 	unsigned : 16;
 * 	uint16_t	bt_ctl;
 * };
 */

/* Bt463 hardware registers, memory-mapped in 32bit stride */
#define	bt_lo	0x0
#define	bt_hi	0x4
#define	bt_reg	0x8
#define	bt_cmap	0xc

/* Bt431 hardware registers, memory-mapped in 32bit stride */
#define	bt_ram	0x8
#define	bt_ctl	0xc

#define	REGWRITE32(p,i,v) do {					\
	*(volatile uint32_t *)((p) + (i)) = (v); tc_wmb();	\
    } while (0)

#define	SELECT463(p,r) do {					\
	REGWRITE32((p), bt_lo, 0xff & (r));			\
	REGWRITE32((p), bt_hi, 0xff & ((r)>>8));		\
   } while (0)

#define	TWIN(x)	   ((x) | ((x) << 8))
#define	TWIN_LO(x) (twin = (x) & 0x00ff, (twin << 8) | twin)
#define	TWIN_HI(x) (twin = (x) & 0xff00, twin | (twin >> 8))

#define	SELECT431(p,r) do {					\
	REGWRITE32((p), bt_lo, TWIN(r));			\
	REGWRITE32((p), bt_hi, 0);				\
   } while (0)

struct hwcmap256 {
#define	CMAP_SIZE	256	/* R/G/B entries */
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

struct tfb_softc {
	vaddr_t sc_vaddr;
	size_t sc_size;
	struct rasops_info *sc_ri;
	struct hwcmap256 sc_cmap;	/* software copy of colormap */
	struct hwcursor64 sc_cursor;	/* software copy of cursor */
	int sc_blanked;			/* video visibility disabled */
	int sc_curenb;			/* cursor sprite enabled */
	int sc_changed;			/* need update of hardware */
#define	WSDISPLAY_CMAP_DOLUT	0x20
	int nscreens;
};

#define	TX_MAGIC_X	360
#define	TX_MAGIC_Y	36

#define	TX_BT463_OFFSET	0x040000
#define	TX_BT431_OFFSET	0x040010
#define	TX_CONTROL	0x040030
#define	TX_MAP_REGISTER	0x040030
#define	TX_PIP_OFFSET	0x0800c0
#define	TX_SELECTION	0x100000
#define	TX_8BPP_OFFSET	0x200000
#define	TX_8BPP_SIZE	0x200000
#define	TX_24BPP_OFFSET	0x400000
#define	TX_24BPP_SIZE	0x600000
#define	TX_VIDEO_ENABLE	0xa00000

#define	TX_CTL_VIDEO_ON	0x80
#define	TX_CTL_INT_ENA	0x40
#define	TX_CTL_INT_PEND	0x20
#define	TX_CTL_SEG_ENA	0x10
#define	TX_CTL_SEG	0x0f

static int  tfbmatch(device_t, cfdata_t, void *);
static void tfbattach(device_t, device_t, void *);

CFATTACH_DECL_NEW(tfb, sizeof(struct tfb_softc),
    tfbmatch, tfbattach, NULL, NULL);

static void tfb_common_init(struct rasops_info *);
static void tfb_cmap_init(struct tfb_softc *);
static struct rasops_info tfb_console_ri;
static tc_addr_t tfb_consaddr;

static struct wsscreen_descr tfb_stdscreen = {
	"std", 0, 0,
	0, /* textops */
	0, 0,
	WSSCREEN_REVERSE
};

static const struct wsscreen_descr *_tfb_scrlist[] = {
	&tfb_stdscreen,
};

static const struct wsscreen_list tfb_screenlist = {
	sizeof(_tfb_scrlist) / sizeof(struct wsscreen_descr *), _tfb_scrlist
};

static int	tfbioctl(void *, void *, u_long, void *, int, struct lwp *);
static paddr_t	tfbmmap(void *, void *, off_t, int);

static int	tfb_alloc_screen(void *, const struct wsscreen_descr *,
				      void **, int *, int *, long *);
static void	tfb_free_screen(void *, void *);
static int	tfb_show_screen(void *, void *, int,
				     void (*) (void *, int, int), void *);

static const struct wsdisplay_accessops tfb_accessops = {
	tfbioctl,
	tfbmmap,
	tfb_alloc_screen,
	tfb_free_screen,
	tfb_show_screen,
	0 /* load_font */
};

int  tfb_cnattach(tc_addr_t);
static int  tfbintr(void *);
static void tfbhwinit(void *);

static int  get_cmap(struct tfb_softc *, struct wsdisplay_cmap *);
static int  set_cmap(struct tfb_softc *, struct wsdisplay_cmap *);
static int  set_cursor(struct tfb_softc *, struct wsdisplay_cursor *);
static int  get_cursor(struct tfb_softc *, struct wsdisplay_cursor *);
static void set_curpos(struct tfb_softc *, struct wsdisplay_curpos *);

/* bit order reverse */
static const uint8_t flip[256] = {
	0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0,
	0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0,
	0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8,
	0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8,
	0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4,
	0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
	0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec,
	0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc,
	0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2,
	0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2,
	0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea,
	0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
	0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6,
	0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6,
	0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee,
	0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe,
	0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1,
	0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
	0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9,
	0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9,
	0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5,
	0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5,
	0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed,
	0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
	0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3,
	0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3,
	0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb,
	0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb,
	0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7,
	0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
	0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef,
	0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff,
};

static int
tfbmatch(device_t parent, cfdata_t match, void *aux)
{
	struct tc_attach_args *ta = aux;

	if (strncmp("PMAG-RO ", ta->ta_modname, TC_ROM_LLEN) != 0
	    && strncmp("PMAG-JA ", ta->ta_modname, TC_ROM_LLEN) != 0)
		return (0);

	return (1);
}


static void
tfbattach(device_t parent, device_t self, void *aux)
{
	struct tfb_softc *sc = device_private(self);
	struct tc_attach_args *ta = aux;
	struct rasops_info *ri;
	struct wsemuldisplaydev_attach_args waa;
	int console;

	console = (ta->ta_addr == tfb_consaddr);
	if (console) {
		sc->sc_ri = ri = &tfb_console_ri;
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
		tfb_common_init(ri);
		sc->sc_ri = ri;
	}
	printf(": %dx%d, 8,24bpp\n", ri->ri_width, ri->ri_height);

	tfb_cmap_init(sc);

	sc->sc_vaddr = ta->ta_addr;
	sc->sc_cursor.cc_magic.x = TX_MAGIC_X;
	sc->sc_cursor.cc_magic.y = TX_MAGIC_Y;
	sc->sc_blanked = sc->sc_curenb = 0;

	tc_intr_establish(parent, ta->ta_cookie, IPL_TTY, tfbintr, sc);

	*(uint8_t *)((char *)ri->ri_hw + TX_CONTROL) &= ~0x40;
	*(uint8_t *)((char *)ri->ri_hw + TX_CONTROL) |= 0x40;

	waa.console = console;
	waa.scrdata = &tfb_screenlist;
	waa.accessops = &tfb_accessops;
	waa.accesscookie = sc;

	config_found(self, &waa, wsemuldisplaydevprint);
}

static void
tfb_common_init(struct rasops_info *ri)
{
	char *base;
	int cookie;

	base = (void *)ri->ri_hw;

	/* initialize colormap and cursor hardware */
	tfbhwinit(base);

	ri->ri_flg = RI_CENTER;
	if (ri == &tfb_console_ri)
		ri->ri_flg |= RI_NO_AUTO;
	ri->ri_depth = 8;
	ri->ri_width = 1280;
	ri->ri_height = 1024;
	ri->ri_stride = 1280;
	ri->ri_bits = base + TX_8BPP_OFFSET;

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
		printf("tfb: font table is empty\n");
		return;
	}

	if (wsfont_lock(cookie, &ri->ri_font)) {
		printf("tfb: couldn't lock font\n");
		return;
	}
	ri->ri_wsfcookie = cookie;

	rasops_init(ri, 34, 80);

	/* XXX shouldn't be global */
	tfb_stdscreen.nrows = ri->ri_rows;
	tfb_stdscreen.ncols = ri->ri_cols;
	tfb_stdscreen.textops = &ri->ri_ops;
	tfb_stdscreen.capabilities = ri->ri_caps;
}

static void
tfb_cmap_init(struct tfb_softc *sc)
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

static int
tfbioctl(void *v, void *vs, u_long cmd, void *data, int flag, struct lwp *l)
{
	struct tfb_softc *sc = v;
	struct rasops_info *ri = sc->sc_ri;
	int turnoff, s;

	switch (cmd) {
	case WSDISPLAYIO_GTYPE:
		*(u_int *)data = WSDISPLAY_TYPE_TX;
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
#if 0	/* XXX later XXX */
	To turn off;
	- clear the MSB of TX control register; &= ~0x80,
	- assign Bt431 register #0 with value 0x4 to hide sprite cursor.
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
			tfb_cmap_init(sc);
			sc->sc_curenb = 0;
			sc->sc_blanked = 0;
			sc->sc_changed |= (WSDISPLAY_CURSOR_DOCUR |
			    WSDISPLAY_CMAP_DOLUT);
			splx(s);
		}
		return (0);
	}
	return (EPASSTHROUGH);
}

static paddr_t
tfbmmap(void *v, void *vs, off_t offset, int prot)
{
	struct tfb_softc *sc = v;

	if (offset >= TX_8BPP_SIZE || offset < 0) /* XXX 24bpp XXX */
		return (-1);
	return machine_btop(sc->sc_vaddr + TX_8BPP_OFFSET + offset);
}

static int
tfb_alloc_screen(void *v, const struct wsscreen_descr *type, void **cookiep,
    int *curxp, int *curyp, long *attrp)
{
	struct tfb_softc *sc = v;
	struct rasops_info *ri = sc->sc_ri;
	long defattr;

	if (sc->nscreens > 0)
		return (ENOMEM);

	*cookiep = ri; /* one and only for now */
	*curxp = 0;
	*curyp = 0;
	(*ri->ri_ops.allocattr)(ri, 0, 0, 0, &defattr);
	*attrp = defattr;
	sc->nscreens++;
	return (0);
}

static void
tfb_free_screen(void *v, void *cookie)
{
	struct tfb_softc *sc = v;

	if (sc->sc_ri == &tfb_console_ri)
		panic("tfb_free_screen: console");

	sc->nscreens--;
}

static int
tfb_show_screen(void *v, void *cookie, int waitok,
    void (*cb)(void *, int, int), void *cbarg)
{

	return (0);
}

/* EXPORT */ int
tfb_cnattach(tc_addr_t addr)
{
	struct rasops_info *ri;
	long defattr;

	ri = &tfb_console_ri;
	ri->ri_hw = (void *)addr;
	tfb_common_init(ri);
	(*ri->ri_ops.allocattr)(ri, 0, 0, 0, &defattr);
	wsdisplay_cnattach(&tfb_stdscreen, ri, 0, 0, defattr);
	tfb_consaddr = addr;
	return (0);
}

static int
tfbintr(void *arg)
{
	struct tfb_softc *sc = arg;
	char *base, *vdac, *curs;
	int v;

	base = (void *)sc->sc_ri->ri_hw;
	*(uint8_t *)(base + TX_CONTROL) &= ~0x40;
	if (sc->sc_changed == 0)
		goto done;

	vdac = base + TX_BT463_OFFSET;
	curs = base + TX_BT431_OFFSET;
	v = sc->sc_changed;
	if (v & WSDISPLAY_CURSOR_DOCUR) {
		int onoff;

		onoff = (sc->sc_curenb) ? 0x4444 : 0x0404;
		SELECT431(curs, BT431_REG_COMMAND);
		REGWRITE32(curs, bt_ctl, onoff);
	}
	if (v & (WSDISPLAY_CURSOR_DOPOS | WSDISPLAY_CURSOR_DOHOT)) {
		int x, y;
		uint32_t twin;

		x = sc->sc_cursor.cc_pos.x - sc->sc_cursor.cc_hot.x;
		y = sc->sc_cursor.cc_pos.y - sc->sc_cursor.cc_hot.y;

		x += sc->sc_cursor.cc_magic.x;
		y += sc->sc_cursor.cc_magic.y;

		SELECT431(curs, BT431_REG_CURSOR_X_LOW);
		REGWRITE32(curs, bt_ctl, TWIN_LO(x));
		REGWRITE32(curs, bt_ctl, TWIN_HI(x));
		REGWRITE32(curs, bt_ctl, TWIN_LO(y));
		REGWRITE32(curs, bt_ctl, TWIN_HI(y));
	}
	if (v & WSDISPLAY_CURSOR_DOCMAP) {
		uint8_t *cp = sc->sc_cursor.cc_color;

		SELECT463(vdac, BT463_IREG_CURSOR_COLOR_0);
		REGWRITE32(vdac, bt_reg, cp[1]);
		REGWRITE32(vdac, bt_reg, cp[3]);
		REGWRITE32(vdac, bt_reg, cp[5]);

		REGWRITE32(vdac, bt_reg, cp[0]);
		REGWRITE32(vdac, bt_reg, cp[2]);
		REGWRITE32(vdac, bt_reg, cp[4]);

		REGWRITE32(vdac, bt_reg, cp[1]);
		REGWRITE32(vdac, bt_reg, cp[3]);
		REGWRITE32(vdac, bt_reg, cp[5]);

		REGWRITE32(vdac, bt_reg, cp[1]);
		REGWRITE32(vdac, bt_reg, cp[3]);
		REGWRITE32(vdac, bt_reg, cp[5]);
	}
	if (v & WSDISPLAY_CURSOR_DOSHAPE) {
		uint8_t *ip, *mp, img, msk;
		int bcnt;

		ip = (uint8_t *)sc->sc_cursor.cc_image;
		mp = (uint8_t *)sc->sc_cursor.cc_mask;
		bcnt = 0;
		SELECT431(curs, BT431_REG_CRAM_BASE);

		/* 64 pixel scan line is consisted with 16 byte cursor ram */
		while (bcnt < sc->sc_cursor.cc_size.y * 16) {
			/* pad right half 32 pixel when smaller than 33 */
			if ((bcnt & 0x8) && sc->sc_cursor.cc_size.x < 33) {
				REGWRITE32(curs, bt_ram, 0);
			}
			else {
				int half;
				img = *ip++;
				msk = *mp++;
				img &= msk;	/* cookie off image */
				half = (flip[img] << 8) | flip[msk];
				REGWRITE32(curs, bt_ram, half);
			}
			bcnt += 2;
		}
		/* pad unoccupied scan lines */
		while (bcnt < CURSOR_MAX_SIZE * 16) {
			REGWRITE32(curs, bt_ram, 0);
			bcnt += 2;
		}
	}
	if (v & WSDISPLAY_CMAP_DOLUT) {
		struct hwcmap256 *cm = &sc->sc_cmap;
		int index;

		SELECT463(vdac, BT463_IREG_CPALETTE_RAM);
		for (index = 0; index < CMAP_SIZE; index++) {
			REGWRITE32(vdac, bt_cmap, cm->r[index]);
			REGWRITE32(vdac, bt_cmap, cm->g[index]);
			REGWRITE32(vdac, bt_cmap, cm->b[index]);
		}
	}
	sc->sc_changed = 0;
done:
	*(uint8_t *)(base + TX_CONTROL) &= ~0x40;	/* !? Eeeh !? */
	*(uint8_t *)(base + TX_CONTROL) |= 0x40;
	return (1);
}

static void
tfbhwinit(void *tfbbase)
{
	char *vdac, *curs;
	const uint8_t *p;
	int i;

	vdac = (char *)tfbbase + TX_BT463_OFFSET;
	curs = (char *)tfbbase + TX_BT431_OFFSET;
	SELECT463(vdac, BT463_IREG_COMMAND_0);
	REGWRITE32(vdac, bt_reg, 0x40);	/* CMD 0 */
	REGWRITE32(vdac, bt_reg, 0x46);	/* CMD 1 */
	REGWRITE32(vdac, bt_reg, 0xc0);	/* CMD 2 */
	REGWRITE32(vdac, bt_reg, 0);	/* !? 204 !? */
	REGWRITE32(vdac, bt_reg, 0xff);	/* plane  0:7  */
	REGWRITE32(vdac, bt_reg, 0xff);	/* plane  8:15 */
	REGWRITE32(vdac, bt_reg, 0xff);	/* plane 16:23 */
	REGWRITE32(vdac, bt_reg, 0xff);	/* plane 24:27 */
	REGWRITE32(vdac, bt_reg, 0x00);	/* blink  0:7  */
	REGWRITE32(vdac, bt_reg, 0x00);	/* blink  8:15 */
	REGWRITE32(vdac, bt_reg, 0x00);	/* blink 16:23 */
	REGWRITE32(vdac, bt_reg, 0x00);	/* blink 24:27 */
	REGWRITE32(vdac, bt_reg, 0x00);

#if 0 /* XXX ULTRIX does initialize 16 entry window type here XXX */
  {
	static uint32_t windowtype[BT463_IREG_WINDOW_TYPE_TABLE] = {
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	};

	SELECT463(vdac, BT463_IREG_WINDOW_TYPE_TABLE);
	for (i = 0; i < BT463_NWTYPE_ENTRIES; i++) {
		BYTE(vdac, bt_reg) = windowtype[i];	  /*   0:7  */
		BYTE(vdac, bt_reg) = windowtype[i] >> 8;  /*   8:15 */
		BYTE(vdac, bt_reg) = windowtype[i] >> 16; /*  16:23 */
	}
  }
#endif

	SELECT463(vdac, BT463_IREG_CPALETTE_RAM);
	p = rasops_cmap;
	for (i = 0; i < 256; i++, p += 3) {
		REGWRITE32(vdac, bt_cmap, p[0]);
		REGWRITE32(vdac, bt_cmap, p[1]);
		REGWRITE32(vdac, bt_cmap, p[2]);
	}

	/* !? Eeeh !? */
	SELECT463(vdac, 0x0100 /* BT463_IREG_CURSOR_COLOR_0 */);
	for (i = 0; i < 256; i++) {
		REGWRITE32(vdac, bt_cmap, i);
		REGWRITE32(vdac, bt_cmap, i);
		REGWRITE32(vdac, bt_cmap, i);
	}

	SELECT431(curs, BT431_REG_COMMAND);
	REGWRITE32(curs, bt_ctl, 0x0404);
	REGWRITE32(curs, bt_ctl, 0); /* XLO */
	REGWRITE32(curs, bt_ctl, 0); /* XHI */
	REGWRITE32(curs, bt_ctl, 0); /* YLO */
	REGWRITE32(curs, bt_ctl, 0); /* YHI */
	REGWRITE32(curs, bt_ctl, 0); /* XWLO */
	REGWRITE32(curs, bt_ctl, 0); /* XWHI */
	REGWRITE32(curs, bt_ctl, 0); /* WYLO */
	REGWRITE32(curs, bt_ctl, 0); /* WYLO */
	REGWRITE32(curs, bt_ctl, 0); /* WWLO */
	REGWRITE32(curs, bt_ctl, 0); /* WWHI */
	REGWRITE32(curs, bt_ctl, 0); /* WHLO */
	REGWRITE32(curs, bt_ctl, 0); /* WHHI */

	SELECT431(curs, BT431_REG_CRAM_BASE);
	for (i = 0; i < 512; i++) {
		REGWRITE32(curs, bt_ram, 0);
	}
}

static int
get_cmap(struct tfb_softc *sc, struct wsdisplay_cmap *p)
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
set_cmap(struct tfb_softc *sc, struct wsdisplay_cmap *p)
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
set_cursor(struct tfb_softc *sc, struct wsdisplay_cursor *p)
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
get_cursor(struct tfb_softc *sc, struct wsdisplay_cursor *p)
{
	return (EPASSTHROUGH); /* XXX */
}

static void
set_curpos(struct tfb_softc *sc, struct wsdisplay_curpos *curpos)
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
