/*	$NetBSD: radeonfb.c,v 1.88 2014/11/05 19:39:17 macallan Exp $ */

/*-
 * Copyright (c) 2006 Itronix Inc.
 * All rights reserved.
 *
 * Written by Garrett D'Amore for Itronix Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of Itronix Inc. may not be used to endorse
 *    or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ITRONIX INC. ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL ITRONIX INC. BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */ 

/*
 * ATI Technologies Inc. ("ATI") has not assisted in the creation of, and
 * does not endorse, this software.  ATI will not be responsible or liable
 * for any actual or alleged damage or loss caused by or in connection with
 * the use of or reliance on this software.
 */

/*
 * Portions of this code were taken from XFree86's Radeon driver, which bears
 * this notice:
 *
 * Copyright 2000 ATI Technologies Inc., Markham, Ontario, and
 *                VA Linux Systems Inc., Fremont, California.
 *
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation on the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON-INFRINGEMENT.  IN NO EVENT SHALL ATI, VA LINUX SYSTEMS AND/OR
 * THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: radeonfb.c,v 1.88 2014/11/05 19:39:17 macallan Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/lwp.h>
#include <sys/kauth.h>

#include <dev/wscons/wsdisplayvar.h>
#include <dev/wscons/wsconsio.h>
#include <dev/wsfont/wsfont.h>
#include <dev/rasops/rasops.h>
#include <dev/videomode/videomode.h>
#include <dev/videomode/edidvar.h>
#include <dev/wscons/wsdisplay_vconsvar.h>
#include <dev/pci/wsdisplay_pci.h>
#include <dev/wscons/wsdisplay_glyphcachevar.h>

#include <dev/pci/pcidevs.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pciio.h>
#include <dev/pci/radeonfbreg.h>
#include <dev/pci/radeonfbvar.h>
#include "opt_radeonfb.h"
#include "opt_vcons.h"

#ifdef RADEONFB_DEPTH_32
#define RADEONFB_DEFAULT_DEPTH 32
#else
#define RADEONFB_DEFAULT_DEPTH 8
#endif

static int radeonfb_match(device_t, cfdata_t, void *);
static void radeonfb_attach(device_t, device_t, void *);
static int radeonfb_ioctl(void *, void *, unsigned long, void *, int,
    struct lwp *);
static paddr_t radeonfb_mmap(void *, void *, off_t, int);
static int radeonfb_scratch_test(struct radeonfb_softc *, int, uint32_t);
static void radeonfb_loadbios(struct radeonfb_softc *,
    const struct pci_attach_args *);

static uintmax_t radeonfb_getprop_num(struct radeonfb_softc *, const char *,
    uintmax_t);
static int radeonfb_getclocks(struct radeonfb_softc *);
static int radeonfb_gettmds(struct radeonfb_softc *);
static int radeonfb_calc_dividers(struct radeonfb_softc *, uint32_t,
    uint32_t *, uint32_t *);
static int radeonfb_getconnectors(struct radeonfb_softc *);
static const struct videomode *radeonfb_modelookup(const char *);
static void radeonfb_init_screen(void *, struct vcons_screen *, int, long *);
static void radeonfb_pllwriteupdate(struct radeonfb_softc *, int);
static void radeonfb_pllwaitatomicread(struct radeonfb_softc *, int);
static void radeonfb_program_vclk(struct radeonfb_softc *, int, int);
static void radeonfb_modeswitch(struct radeonfb_display *);
static void radeonfb_setcrtc(struct radeonfb_display *, int);
static void radeonfb_init_misc(struct radeonfb_softc *);
static void radeonfb_set_fbloc(struct radeonfb_softc *);
static void radeonfb_init_palette(struct radeonfb_display *);
static void radeonfb_r300cg_workaround(struct radeonfb_softc *);

static int radeonfb_isblank(struct radeonfb_display *);
static void radeonfb_blank(struct radeonfb_display *, int);
static int radeonfb_set_cursor(struct radeonfb_display *,
    struct wsdisplay_cursor *);
static int radeonfb_set_curpos(struct radeonfb_display *,
    struct wsdisplay_curpos *);
static void radeonfb_putpal(struct radeonfb_display *, int, int, int, int);
static int radeonfb_putcmap(struct radeonfb_display *, struct wsdisplay_cmap *);
static int radeonfb_getcmap(struct radeonfb_display *, struct wsdisplay_cmap *);

/* acceleration support */
static void  radeonfb_rectfill(struct radeonfb_display *, int dstx, int dsty,
    int width, int height, uint32_t color);
static void  radeonfb_rectfill_a(void *, int, int, int, int, long);
static void radeonfb_bitblt(void *, int srcx, int srcy,
    int dstx, int dsty, int width, int height, int rop);

/* hw cursor support */
static void radeonfb_cursor_cmap(struct radeonfb_display *);
static void radeonfb_cursor_shape(struct radeonfb_display *);
static void radeonfb_cursor_position(struct radeonfb_display *);
static void radeonfb_cursor_visible(struct radeonfb_display *);
static void radeonfb_cursor_update(struct radeonfb_display *, unsigned);

static inline void radeonfb_wait_fifo(struct radeonfb_softc *, int);
static void radeonfb_engine_idle(struct radeonfb_softc *);
static void radeonfb_engine_flush(struct radeonfb_softc *);
static void radeonfb_engine_reset(struct radeonfb_softc *);
static void radeonfb_engine_init(struct radeonfb_display *);
static inline void radeonfb_unclip(struct radeonfb_softc *) __unused;

static void radeonfb_eraserows(void *, int, int, long);
static void radeonfb_erasecols(void *, int, int, int, long);
static void radeonfb_copyrows(void *, int, int, int);
static void radeonfb_copycols(void *, int, int, int, int);
static void radeonfb_cursor(void *, int, int, int);
static void radeonfb_putchar(void *, int, int, unsigned, long);
static void radeonfb_putchar_aa32(void *, int, int, unsigned, long);
static void radeonfb_putchar_aa8(void *, int, int, unsigned, long);
#ifndef RADEONFB_ALWAYS_ACCEL_PUTCHAR
static void radeonfb_putchar_wrapper(void *, int, int, unsigned, long);
#endif

static int radeonfb_set_backlight(struct radeonfb_display *, int);
static int radeonfb_get_backlight(struct radeonfb_display *);
static void radeonfb_switch_backlight(struct radeonfb_display *, int);
static void radeonfb_lvds_callout(void *);

static void radeonfb_brightness_up(device_t);
static void radeonfb_brightness_down(device_t);

static struct videomode *radeonfb_best_refresh(struct videomode *,
    struct videomode *);
static void radeonfb_pickres(struct radeonfb_display *, uint16_t *,
    uint16_t *, int);
static const struct videomode *radeonfb_port_mode(struct radeonfb_softc *, 
    struct radeonfb_port *, int, int);

static int radeonfb_drm_print(void *, const char *);

#ifdef	RADEONFB_DEBUG
int	radeon_debug = 1;
#define	DPRINTF(x)	\
	if (radeon_debug) printf x
#define	PRINTREG(r)	DPRINTF((#r " = %08x\n", GET32(sc, r)))
#define	PRINTPLL(r)	DPRINTF((#r " = %08x\n", GETPLL(sc, r)))
#else
#define	DPRINTF(x)
#define	PRINTREG(r)
#define	PRINTPLL(r)
#endif

#define	ROUNDUP(x,y)	(((x) + ((y) - 1)) & ~((y) - 1))

#ifndef	RADEON_DEFAULT_MODE
/* any reasonably modern display should handle this */
#define	RADEON_DEFAULT_MODE	"1024x768x60"
#endif

extern const u_char rasops_cmap[768];

const char	*radeonfb_default_mode = RADEON_DEFAULT_MODE;

static struct {
	int		size;		/* minimum memory size (MB) */
	int		maxx;		/* maximum x dimension */
	int		maxy;		/* maximum y dimension */
	int		maxbpp;		/* maximum bpp */
	int		maxdisp;	/* maximum logical display count */
} radeonfb_limits[] = {
	{ 32,	2048, 1536, 32, 2 },
	{ 16,	1600, 1200, 32, 2 },
	{ 8,	1600, 1200, 32, 1 },
	{ 0,	0, 0, 0, 0 },
};

static struct wsscreen_descr radeonfb_stdscreen = {
	"fb",		/* name */
	0, 0,		/* ncols, nrows */
	NULL,		/* textops */
	8, 16,		/* fontwidth, fontheight */
	WSSCREEN_WSCOLORS | WSSCREEN_UNDERLINE, /* capabilities */
	0,		/* modecookie */
};

struct wsdisplay_accessops radeonfb_accessops = {
	radeonfb_ioctl,
	radeonfb_mmap,
	NULL,		/* vcons_alloc_screen */
	NULL,		/* vcons_free_screen */
	NULL,		/* vcons_show_screen */
	NULL,		/* load_font */
	NULL,		/* pollc */
	NULL,		/* scroll */
};

static struct {
	uint16_t	devid;
	uint16_t	family;
	uint16_t	flags;
} radeonfb_devices[] = 
{
	/* R100 family */
	{ PCI_PRODUCT_ATI_RADEON_R100_QD,	RADEON_R100, 0 },
	{ PCI_PRODUCT_ATI_RADEON_R100_QE,	RADEON_R100, 0 },
	{ PCI_PRODUCT_ATI_RADEON_R100_QF,	RADEON_R100, 0 },
	{ PCI_PRODUCT_ATI_RADEON_R100_QG,	RADEON_R100, 0 },

	/* RV100 family */
	{ PCI_PRODUCT_ATI_RADEON_RV100_LY,	RADEON_RV100, RFB_MOB },
	{ PCI_PRODUCT_ATI_RADEON_RV100_LZ,	RADEON_RV100, RFB_MOB },
	{ PCI_PRODUCT_ATI_RADEON_RV100_QY,	RADEON_RV100, 0 },
	{ PCI_PRODUCT_ATI_RADEON_RV100_QZ,	RADEON_RV100, 0 },

	/* RS100 family */
	{ PCI_PRODUCT_ATI_RADEON_RS100_4136,	RADEON_RS100, 0 },
	{ PCI_PRODUCT_ATI_RADEON_RS100_4336,	RADEON_RS100, RFB_MOB },

	/* RS200/RS250 family */
	{ PCI_PRODUCT_ATI_RADEON_RS200_4337,	RADEON_RS200, RFB_MOB },
	{ PCI_PRODUCT_ATI_RADEON_RS200_A7,	RADEON_RS200, 0 },
	{ PCI_PRODUCT_ATI_RADEON_RS250_B7,	RADEON_RS200, RFB_MOB },
	{ PCI_PRODUCT_ATI_RADEON_RS250_D7,	RADEON_RS200, 0 },

	/* R200 family */
	/* add more R200 products? , 5148 */
	{ PCI_PRODUCT_ATI_RADEON_R200_BB,	RADEON_R200, 0 },
	{ PCI_PRODUCT_ATI_RADEON_R200_BC,	RADEON_R200, 0 },
	{ PCI_PRODUCT_ATI_RADEON_R200_QH,	RADEON_R200, 0 },
	{ PCI_PRODUCT_ATI_RADEON_R200_QL,	RADEON_R200, 0 },
	{ PCI_PRODUCT_ATI_RADEON_R200_QM,	RADEON_R200, 0 },

	/* RV200 family */
	{ PCI_PRODUCT_ATI_RADEON_RV200_LW,	RADEON_RV200, RFB_MOB },
	{ PCI_PRODUCT_ATI_RADEON_RV200_LX,	RADEON_RV200, RFB_MOB },
	{ PCI_PRODUCT_ATI_RADEON_RV200_QW,	RADEON_RV200, 0 },
	{ PCI_PRODUCT_ATI_RADEON_RV200_QX,	RADEON_RV200, 0 },

	/* RV250 family */
	{ PCI_PRODUCT_ATI_RADEON_RV250_4966,	RADEON_RV250, 0 },
	{ PCI_PRODUCT_ATI_RADEON_RV250_4967,	RADEON_RV250, 0 },
	{ PCI_PRODUCT_ATI_RADEON_RV250_4C64,	RADEON_RV250, RFB_MOB },
	{ PCI_PRODUCT_ATI_RADEON_RV250_4C66,	RADEON_RV250, RFB_MOB },
	{ PCI_PRODUCT_ATI_RADEON_RV250_4C67,	RADEON_RV250, RFB_MOB },

	/* RS300 family */
	{ PCI_PRODUCT_ATI_RADEON_RS300_X5,	RADEON_RS300, 0 },
	{ PCI_PRODUCT_ATI_RADEON_RS300_X4,	RADEON_RS300, 0 },
	{ PCI_PRODUCT_ATI_RADEON_RS300_7834,	RADEON_RS300, 0 },
	{ PCI_PRODUCT_ATI_RADEON_RS300_7835,	RADEON_RS300, RFB_MOB },

	/* RV280 family */
	{ PCI_PRODUCT_ATI_RADEON_RV280_5960,	RADEON_RV280, 0 },
	{ PCI_PRODUCT_ATI_RADEON_RV280_5961,	RADEON_RV280, 0 },
	{ PCI_PRODUCT_ATI_RADEON_RV280_5962,	RADEON_RV280, 0 },
	{ PCI_PRODUCT_ATI_RADEON_RV280_5963,	RADEON_RV280, 0 },
	{ PCI_PRODUCT_ATI_RADEON_RV280_5964,	RADEON_RV280, 0 },
	{ PCI_PRODUCT_ATI_RADEON_RV280_5C61,	RADEON_RV280, RFB_MOB },
	{ PCI_PRODUCT_ATI_RADEON_RV280_5C63,	RADEON_RV280, RFB_MOB },

	/* R300 family */
	{ PCI_PRODUCT_ATI_RADEON_R300_AD,	RADEON_R300, 0 },
	{ PCI_PRODUCT_ATI_RADEON_R300_AE,	RADEON_R300, 0 },
	{ PCI_PRODUCT_ATI_RADEON_R300_AF,	RADEON_R300, 0 },
	{ PCI_PRODUCT_ATI_RADEON_R300_AG,	RADEON_R300, 0 },
	{ PCI_PRODUCT_ATI_RADEON_R300_ND,	RADEON_R300, 0 },
	{ PCI_PRODUCT_ATI_RADEON_R300_NE,	RADEON_R300, 0 },
	{ PCI_PRODUCT_ATI_RADEON_R300_NF,	RADEON_R300, 0 },
	{ PCI_PRODUCT_ATI_RADEON_R300_NG,	RADEON_R300, 0 },

	/* RV350/RV360 family */
	{ PCI_PRODUCT_ATI_RADEON_RV350_AP,	RADEON_RV350, 0 },
	{ PCI_PRODUCT_ATI_RADEON_RV350_AQ,	RADEON_RV350, 0 },
	{ PCI_PRODUCT_ATI_RADEON_RV360_AR,	RADEON_RV350, 0 },
	{ PCI_PRODUCT_ATI_RADEON_RV350_AS,	RADEON_RV350, 0 },
	{ PCI_PRODUCT_ATI_RADEON_RV350_AT,	RADEON_RV350, 0 },
	{ PCI_PRODUCT_ATI_RADEON_RV350_AV,	RADEON_RV350, 0 },
	{ PCI_PRODUCT_ATI_RADEON_RV350_NP,	RADEON_RV350, RFB_MOB },
	{ PCI_PRODUCT_ATI_RADEON_RV350_NQ,	RADEON_RV350, RFB_MOB },
	{ PCI_PRODUCT_ATI_RADEON_RV350_NR,	RADEON_RV350, RFB_MOB },
	{ PCI_PRODUCT_ATI_RADEON_RV350_NS,	RADEON_RV350, RFB_MOB },
	{ PCI_PRODUCT_ATI_RADEON_RV350_NT,	RADEON_RV350, RFB_MOB },
	{ PCI_PRODUCT_ATI_RADEON_RV350_NV,	RADEON_RV350, RFB_MOB },

	/* R350/R360 family */
	{ PCI_PRODUCT_ATI_RADEON_R350_AH,	RADEON_R350, 0 },
	{ PCI_PRODUCT_ATI_RADEON_R350_AI,	RADEON_R350, 0 },
	{ PCI_PRODUCT_ATI_RADEON_R350_AJ,	RADEON_R350, 0 },
	{ PCI_PRODUCT_ATI_RADEON_R350_AK,	RADEON_R350, 0 },
	{ PCI_PRODUCT_ATI_RADEON_R350_NH,	RADEON_R350, 0 },
	{ PCI_PRODUCT_ATI_RADEON_R350_NI,	RADEON_R350, 0 },
	{ PCI_PRODUCT_ATI_RADEON_R350_NK,	RADEON_R350, 0 },
	{ PCI_PRODUCT_ATI_RADEON_R360_NJ,	RADEON_R350, 0 },

	/* RV380/RV370 family */
	{ PCI_PRODUCT_ATI_RADEON_RV380_3150,	RADEON_RV380, RFB_MOB },
	{ PCI_PRODUCT_ATI_RADEON_RV380_3154,	RADEON_RV380, RFB_MOB },
	{ PCI_PRODUCT_ATI_RADEON_RV380_3E50,	RADEON_RV380, 0 },
	{ PCI_PRODUCT_ATI_RADEON_RV380_3E54,	RADEON_RV380, 0 },
	{ PCI_PRODUCT_ATI_RADEON_RV370_5460,	RADEON_RV380, RFB_MOB },
	{ PCI_PRODUCT_ATI_RADEON_RV370_5464,	RADEON_RV380, RFB_MOB },
	{ PCI_PRODUCT_ATI_RADEON_RV370_5B60,	RADEON_RV380, 0 },
	{ PCI_PRODUCT_ATI_RADEON_RV370_5B63,	RADEON_RV380, 0 },
	{ PCI_PRODUCT_ATI_RADEON_RV370_5B64,	RADEON_RV380, 0 },
	{ PCI_PRODUCT_ATI_RADEON_RV370_5B65,	RADEON_RV380, 0 },

#if notyet
	/* R420/R423 family */
	{ PCI_PRODUCT_ATI_RADEON_R420_JH,	RADEON_R420, 0 },
	{ PCI_PRODUCT_ATI_RADEON_R420_JI,	RADEON_R420, 0 },
	{ PCI_PRODUCT_ATI_RADEON_R420_JJ,	RADEON_R420, 0 },
	{ PCI_PRODUCT_ATI_RADEON_R420_JK,	RADEON_R420, 0 },
	{ PCI_PRODUCT_ATI_RADEON_R420_JL,	RADEON_R420, 0 },
	{ PCI_PRODUCT_ATI_RADEON_R420_JM,	RADEON_R420, 0 },
	{ PCI_PRODUCT_ATI_RADEON_R420_JN,	RADEON_R420, RFB_MOB },
	{ PCI_PRODUCT_ATI_RADEON_R420_JP,	RADEON_R420, 0 },
	{ PCI_PRODUCT_ATI_RADEON_R423_UH,	RADEON_R420, 0 },
	{ PCI_PRODUCT_ATI_RADEON_R423_UI,	RADEON_R420, 0 },
	{ PCI_PRODUCT_ATI_RADEON_R423_UJ,	RADEON_R420, 0 },
	{ PCI_PRODUCT_ATI_RADEON_R423_UK,	RADEON_R420, 0 },
	{ PCI_PRODUCT_ATI_RADEON_R423_UQ,	RADEON_R420, 0 },
	{ PCI_PRODUCT_ATI_RADEON_R423_UR,	RADEON_R420, 0 },
	{ PCI_PRODUCT_ATI_RADEON_R423_UT,	RADEON_R420, 0 },
	{ PCI_PRODUCT_ATI_RADEON_R423_5D57,	RADEON_R420, 0 },
	{ PCI_PRODUCT_ATI_RADEON_R430_554F,	RADEON_R420, 0 },
#endif
	{ 0, 0, 0 }
};

static struct {
	int divider;
	int mask;
} radeonfb_dividers[] = {
	{ 16, 5 },
	{ 12, 7 },
	{  8, 3 },
	{  6, 6 },
	{  4, 2 },
	{  3, 4 },
	{  2, 1 },
	{  1, 0 },
	{  0, 0 }
};

/*
 * This table taken from X11.
 */
static const struct {
	int			family;
	struct radeon_tmds_pll	plls[4];
} radeonfb_tmds_pll[] = {
	{ RADEON_R100,	{{12000, 0xa1b}, {-1, 0xa3f}}},
	{ RADEON_RV100,	{{12000, 0xa1b}, {-1, 0xa3f}}},
	{ RADEON_RS100, {{0, 0}}},
	{ RADEON_RV200,	{{15000, 0xa1b}, {-1, 0xa3f}}},
	{ RADEON_RS200,	{{15000, 0xa1b}, {-1, 0xa3f}}},
	{ RADEON_R200,	{{15000, 0xa1b}, {-1, 0xa3f}}},
	{ RADEON_RV250,	{{15500, 0x81b}, {-1, 0x83f}}},
	{ RADEON_RS300, {{0, 0}}},
	{ RADEON_RV280,	{{13000, 0x400f4}, {15000, 0x400f7}}},
	{ RADEON_R300,	{{-1, 0xb01cb}}},
	{ RADEON_R350,	{{-1, 0xb01cb}}},
	{ RADEON_RV350,	{{15000, 0xb0155}, {-1, 0xb01cb}}},
	{ RADEON_RV380,	{{15000, 0xb0155}, {-1, 0xb01cb}}},
	{ RADEON_R420,	{{-1, 0xb01cb}}},
};

#define RADEONFB_BACKLIGHT_MAX    255  /* Maximum backlight level. */


CFATTACH_DECL_NEW(radeonfb, sizeof (struct radeonfb_softc),
    radeonfb_match, radeonfb_attach, NULL, NULL);

static int
radeonfb_match(device_t parent, cfdata_t match, void *aux)
{
	const struct pci_attach_args	*pa = aux;
	int			i;

	if (PCI_VENDOR(pa->pa_id) != PCI_VENDOR_ATI)
		return 0;

	for (i = 0; radeonfb_devices[i].devid; i++) {
		if (PCI_PRODUCT(pa->pa_id) == radeonfb_devices[i].devid)
			return 100;	/* high to defeat VGA/VESA */
	}

	return 0;
}

static void
radeonfb_attach(device_t parent, device_t dev, void *aux)
{
	struct radeonfb_softc	*sc = device_private(dev);
	const struct pci_attach_args	*pa = aux;
	const char		*mptr;
	bus_size_t		bsz;
	pcireg_t		screg;
	int			i, j, fg, bg, ul, flags;
	uint32_t		v;

	sc->sc_dev = dev;
	sc->sc_id = pa->pa_id;
	for (i = 0; radeonfb_devices[i].devid; i++) {
		if (PCI_PRODUCT(sc->sc_id) == radeonfb_devices[i].devid)
			break;
	}

	pci_aprint_devinfo(pa, NULL);

	DPRINTF(("%s", prop_dictionary_externalize(device_properties(dev))));

	KASSERT(radeonfb_devices[i].devid != 0);
	sc->sc_pt = pa->pa_tag;
	sc->sc_iot = pa->pa_iot;
	sc->sc_pc = pa->pa_pc;
	sc->sc_family = radeonfb_devices[i].family;
	sc->sc_flags = radeonfb_devices[i].flags;

	/* enable memory and IO access */
	screg = pci_conf_read(sc->sc_pc, sc->sc_pt, PCI_COMMAND_STATUS_REG);
	screg |= PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MEM_ENABLE;
	pci_conf_write(sc->sc_pc, sc->sc_pt, PCI_COMMAND_STATUS_REG, screg);

	/*
	 * Some flags are general to entire chip families, and rather
	 * than clutter up the table with them, we go ahead and set
	 * them here.
	 */
	switch (sc->sc_family) {
	case RADEON_RS100:
	case RADEON_RS200:
		sc->sc_flags |= RFB_IGP | RFB_RV100;
		break;

	case RADEON_RV100:
	case RADEON_RV200:
	case RADEON_RV250:
	case RADEON_RV280:
		sc->sc_flags |= RFB_RV100;
		break;

	case RADEON_RS300:
		sc->sc_flags |= RFB_SDAC | RFB_IGP | RFB_RV100;
		break;

	case RADEON_R300:
	case RADEON_RV350:
	case RADEON_R350:
	case RADEON_RV380:
	case RADEON_R420:
		/* newer chips */
		sc->sc_flags |= RFB_R300;
		break;

	case RADEON_R100:
		sc->sc_flags |= RFB_NCRTC2;
		break;
	}

	if ((sc->sc_family == RADEON_RV200) ||
	    (sc->sc_family == RADEON_RV250) ||
	    (sc->sc_family == RADEON_RV280) ||
	    (sc->sc_family == RADEON_RV350)) {
		bool inverted = 0;
		/* backlight level is linear */
		DPRINTF(("found RV* chip, backlight is supposedly linear\n"));
		prop_dictionary_get_bool(device_properties(sc->sc_dev),
		    "backlight_level_reverted", &inverted);
		if (inverted) {
			DPRINTF(("nope, it's inverted\n"));
			sc->sc_flags |= RFB_INV_BLIGHT;
		}
	} else
		sc->sc_flags |= RFB_INV_BLIGHT;

	/*
	 * XXX: to support true multihead, this must change.
	 */
	sc->sc_ndisplays = 1;

	/* XXX: */
	if (!HAS_CRTC2(sc)) {
		sc->sc_ndisplays = 1;
	}

	if (pci_mapreg_map(pa, RADEON_MAPREG_MMIO, PCI_MAPREG_TYPE_MEM,	0,
		&sc->sc_regt, &sc->sc_regh, &sc->sc_regaddr,
		&sc->sc_regsz) != 0) {
		aprint_error("%s: unable to map registers!\n", XNAME(sc));
		goto error;
	}

	if (pci_mapreg_info(sc->sc_pc, sc->sc_pt, PCI_MAPREG_ROM,
	     PCI_MAPREG_TYPE_ROM, &sc->sc_romaddr, &sc->sc_romsz, &flags) != 0)
	{
		aprint_error("%s: unable to find ROM!\n", XNAME(sc));
		goto error;
	}
	sc->sc_romt = sc->sc_memt;

	sc->sc_mapped = TRUE;

	/* scratch register test... */
	if (radeonfb_scratch_test(sc, RADEON_BIOS_0_SCRATCH, 0x55555555) ||
	    radeonfb_scratch_test(sc, RADEON_BIOS_0_SCRATCH, 0xaaaaaaaa)) {
		aprint_error("%s: scratch register test failed!\n", XNAME(sc));
		goto error;
	}

	PRINTREG(RADEON_CRTC_EXT_CNTL);
	PRINTREG(RADEON_CRTC_GEN_CNTL);
	PRINTREG(RADEON_CRTC2_GEN_CNTL);
	PRINTREG(RADEON_DISP_OUTPUT_CNTL);
	PRINTREG(RADEON_DAC_CNTL2);
	PRINTREG(RADEON_FP_GEN_CNTL);
	PRINTREG(RADEON_FP2_GEN_CNTL);

	PRINTREG(RADEON_BIOS_4_SCRATCH);
	PRINTREG(RADEON_FP_GEN_CNTL);
	sc->sc_fp_gen_cntl = GET32(sc, RADEON_FP_GEN_CNTL);
	PRINTREG(RADEON_FP2_GEN_CNTL);
	PRINTREG(RADEON_TMDS_CNTL);
	PRINTREG(RADEON_TMDS_TRANSMITTER_CNTL);
	PRINTREG(RADEON_TMDS_PLL_CNTL);
	PRINTREG(RADEON_LVDS_GEN_CNTL);
	PRINTREG(RADEON_FP_HORZ_STRETCH);
	PRINTREG(RADEON_FP_VERT_STRETCH);

	if (IS_RV100(sc))
		PUT32(sc, RADEON_TMDS_PLL_CNTL, 0xa27);

	/* XXX
	 * according to xf86-video-radeon R3xx has this bit backwards
	 */
	if (IS_R300(sc)) {
		PATCH32(sc, RADEON_TMDS_TRANSMITTER_CNTL,
		    0,
		    ~(RADEON_TMDS_TRANSMITTER_PLLEN | RADEON_TMDS_TRANSMITTER_PLLRST));
	} else {
		PATCH32(sc, RADEON_TMDS_TRANSMITTER_CNTL,
		    RADEON_TMDS_TRANSMITTER_PLLEN,
		    ~(RADEON_TMDS_TRANSMITTER_PLLEN | RADEON_TMDS_TRANSMITTER_PLLRST));
	}
	
	radeonfb_i2c_init(sc);

	radeonfb_loadbios(sc, pa);

#ifdef	RADEONFB_BIOS_INIT
	if (radeonfb_bios_init(sc)) {
		aprint_error("%s: BIOS inititialization failed\n", XNAME(sc));
	}
#endif

	if (radeonfb_getclocks(sc)) {
		aprint_error("%s: Unable to get reference clocks from BIOS\n",
		    XNAME(sc));
		goto error;
	}

	if (radeonfb_gettmds(sc)) {
		aprint_error("%s: Unable to identify TMDS PLL settings\n",
		    XNAME(sc));
		goto error;
	}

	aprint_verbose("%s: refclk = %d.%03d MHz, refdiv = %d "
	    "minpll = %d, maxpll = %d\n", XNAME(sc),
	    (int)sc->sc_refclk / 1000, (int)sc->sc_refclk % 1000,
	    (int)sc->sc_refdiv, (int)sc->sc_minpll, (int)sc->sc_maxpll);

	radeonfb_getconnectors(sc);

	radeonfb_set_fbloc(sc);

	/* 64 MB should be enough -- more just wastes map entries */
	if (sc->sc_memsz > (64 << 20))
		sc->sc_memsz = (64 << 20);

	for (i = 0; radeonfb_limits[i].size; i++) {
		if (sc->sc_memsz >= radeonfb_limits[i].size) {
			sc->sc_maxx = radeonfb_limits[i].maxx;
			sc->sc_maxy = radeonfb_limits[i].maxy;
			sc->sc_maxbpp = radeonfb_limits[i].maxbpp;
			/* framebuffer offset, start at a 4K page */
			sc->sc_fboffset = sc->sc_memsz /
			    radeonfb_limits[i].maxdisp;
			/*
			 * we use the fbsize to figure out where we can store
			 * things like cursor data.
			 */
			sc->sc_fbsize =
			    ROUNDUP(ROUNDUP(sc->sc_maxx * sc->sc_maxbpp / 8 ,
					RADEON_STRIDEALIGN) * sc->sc_maxy,
				4096);
			break;
		}
	}


	radeonfb_init_misc(sc);

	/* program the DAC wirings */
	for (i = 0; i < (HAS_CRTC2(sc) ? 2 : 1); i++) {
		switch (sc->sc_ports[i].rp_dac_type) {
		case RADEON_DAC_PRIMARY:
			PATCH32(sc, RADEON_DAC_CNTL2,
			    i ? RADEON_DAC2_DAC_CLK_SEL : 0,
			    ~RADEON_DAC2_DAC_CLK_SEL);
			break;
		case RADEON_DAC_TVDAC:
			/* we always use the TVDAC to drive a secondary analog
			 * CRT for now.  if we ever support TV-out this will
			 * have to change.
			 */
			SET32(sc, RADEON_DAC_CNTL2,
			    RADEON_DAC2_DAC2_CLK_SEL);
			PATCH32(sc, RADEON_DISP_HW_DEBUG,
			    i ? 0 : RADEON_CRT2_DISP1_SEL,
			    ~RADEON_CRT2_DISP1_SEL);
			/* we're using CRTC2 for the 2nd port */
			if (sc->sc_ports[i].rp_number == 1) {
				PATCH32(sc, RADEON_DISP_OUTPUT_CNTL,
				    RADEON_DISP_DAC2_SOURCE_CRTC2,
				    ~RADEON_DISP_DAC2_SOURCE_MASK);
			}
			
			break;
		}
		DPRINTF(("%s: port %d tmds type %d\n", __func__, i,
		    sc->sc_ports[i].rp_tmds_type));
		switch (sc->sc_ports[i].rp_tmds_type) {
		case RADEON_TMDS_INT:
			/* point FP0 at the CRTC this port uses */
			DPRINTF(("%s: plugging internal TMDS into CRTC %d\n",
			    __func__, sc->sc_ports[i].rp_number));
			if (IS_R300(sc)) {
				PATCH32(sc, RADEON_FP_GEN_CNTL,
				    sc->sc_ports[i].rp_number ?
				      R200_FP_SOURCE_SEL_CRTC2 :
				      R200_FP_SOURCE_SEL_CRTC1,
				    ~R200_FP_SOURCE_SEL_MASK);
			} else {
				PATCH32(sc, RADEON_FP_GEN_CNTL,
				    sc->sc_ports[i].rp_number ?
				      RADEON_FP_SEL_CRTC2 :
				      RADEON_FP_SEL_CRTC1,
				    ~RADEON_FP_SEL_MASK);
			}
		}
	}
	PRINTREG(RADEON_DAC_CNTL2);
	PRINTREG(RADEON_DISP_HW_DEBUG);

	/* other DAC programming */
	v = GET32(sc, RADEON_DAC_CNTL);
	v &= (RADEON_DAC_RANGE_CNTL_MASK | RADEON_DAC_BLANKING);
	v |= RADEON_DAC_MASK_ALL | RADEON_DAC_8BIT_EN;
	PUT32(sc, RADEON_DAC_CNTL, v);
	PRINTREG(RADEON_DAC_CNTL);
	
	/* XXX: this may need more investigation */
	PUT32(sc, RADEON_TV_DAC_CNTL, 0x00280203);
	PRINTREG(RADEON_TV_DAC_CNTL);

	/* enable TMDS */
	SET32(sc, RADEON_FP_GEN_CNTL,
	    RADEON_FP_TMDS_EN |
		RADEON_FP_CRTC_DONT_SHADOW_VPAR |
		RADEON_FP_CRTC_DONT_SHADOW_HEND);
	/*
	 * XXX
	 * no idea why this is necessary - if I do not clear this bit on my
	 * iBook G4 the screen remains black, even though it's already clear.
	 * It needs to be set on my Sun XVR-100 for the DVI port to work
	 * TODO:
	 * see if this is still necessary now that CRTCs, DACs and outputs are
	 * getting wired up in a halfway sane way
	 */
	if (sc->sc_fp_gen_cntl & RADEON_FP_SEL_CRTC2) {
		SET32(sc, RADEON_FP_GEN_CNTL, RADEON_FP_SEL_CRTC2);
	} else {
		CLR32(sc, RADEON_FP_GEN_CNTL, RADEON_FP_SEL_CRTC2);
	}

	/*
	 * we use bus_space_map instead of pci_mapreg, because we don't
	 * need the full aperature space.  no point in wasting virtual
	 * address space we don't intend to use, right?
	 */
	if ((sc->sc_memsz < (4096 * 1024)) ||
	    (pci_mapreg_info(sc->sc_pc, sc->sc_pt, RADEON_MAPREG_VRAM,
		PCI_MAPREG_TYPE_MEM, &sc->sc_memaddr, &bsz, NULL) != 0) ||
	    (bsz < sc->sc_memsz)) {
		sc->sc_memsz = 0;
		aprint_error("%s: Bad frame buffer configuration\n",
		    XNAME(sc));
		goto error;
	}

	sc->sc_memt = pa->pa_memt;
	if (bus_space_map(sc->sc_memt, sc->sc_memaddr, sc->sc_memsz,
		BUS_SPACE_MAP_LINEAR, &sc->sc_memh) != 0) {
		sc->sc_memsz = 0;
		aprint_error("%s: Unable to map frame buffer\n", XNAME(sc));
		goto error;
	}

	aprint_normal("%s: %d MB aperture at 0x%08x, "
	    "%d KB registers at 0x%08x\n", XNAME(sc),
	    (int)sc->sc_memsz >> 20, (unsigned)sc->sc_memaddr,
	    (int)sc->sc_regsz >> 10, (unsigned)sc->sc_regaddr);

	/* setup default video mode from devprop (allows PROM override) */
	sc->sc_defaultmode = radeonfb_default_mode;
	if (prop_dictionary_get_cstring_nocopy(device_properties(sc->sc_dev),
	    "videomode", &mptr)) {

		strncpy(sc->sc_modebuf, mptr, sizeof(sc->sc_modebuf));
		sc->sc_defaultmode = sc->sc_modebuf;
	}

	/* initialize some basic display parameters */
	for (i = 0; i < sc->sc_ndisplays; i++) {
		struct radeonfb_display *dp = &sc->sc_displays[i];
		struct rasops_info *ri;
		long defattr;
		struct wsemuldisplaydev_attach_args aa;
	
		/*
		 * Figure out how many "displays" (desktops) we are going to
		 * support.  If more than one, then each CRTC gets its own
		 * programming.
		 *
		 * XXX: this code needs to change to support mergedfb.
		 * XXX: would be nice to allow this to be overridden
		 */
		if (HAS_CRTC2(sc) && (sc->sc_ndisplays == 1)) {
			DPRINTF(("dual crtcs!\n"));
			dp->rd_ncrtcs = 2;
			dp->rd_crtcs[0].rc_port =
			    &sc->sc_ports[0];
			dp->rd_crtcs[0].rc_number = sc->sc_ports[0].rp_number;
			dp->rd_crtcs[1].rc_port =
			    &sc->sc_ports[1];
			dp->rd_crtcs[1].rc_number = sc->sc_ports[1].rp_number;
		} else {
			dp->rd_ncrtcs = 1;
			dp->rd_crtcs[0].rc_port =
			    &sc->sc_ports[i];
			dp->rd_crtcs[0].rc_number = sc->sc_ports[i].rp_number;
		}

		dp->rd_softc = sc;
		dp->rd_wsmode = WSDISPLAYIO_MODE_EMUL;
		dp->rd_bpp = RADEONFB_DEFAULT_DEPTH;	/* XXX */

		/* for text mode, we pick a resolution that won't
		 * require panning */
		radeonfb_pickres(dp, &dp->rd_virtx, &dp->rd_virty, 0);

		aprint_normal("%s: display %d: "
		    "initial virtual resolution %dx%d at %d bpp\n",
		    XNAME(sc), i, dp->rd_virtx, dp->rd_virty, dp->rd_bpp);
		aprint_normal_dev(sc->sc_dev, "using %d MB per display\n",
		    sc->sc_fboffset >> 20);
		/* now select the *video mode* that we will use */
		for (j = 0; j < dp->rd_ncrtcs; j++) {
			const struct videomode *vmp;
			vmp = radeonfb_port_mode(sc, dp->rd_crtcs[j].rc_port,
			    dp->rd_virtx, dp->rd_virty);

			/*
			 * virtual resolution should be at least as high as
			 * physical
			 */
			if (dp->rd_virtx < vmp->hdisplay ||
			    dp->rd_virty < vmp->vdisplay) {
				dp->rd_virtx = vmp->hdisplay;
				dp->rd_virty = vmp->vdisplay;
			}

			dp->rd_crtcs[j].rc_videomode = *vmp;
			printf("%s: port %d: physical %dx%d %dHz\n",
			    XNAME(sc), j, vmp->hdisplay, vmp->vdisplay,
			    DIVIDE(DIVIDE(vmp->dot_clock * 1000,
				       vmp->htotal), vmp->vtotal));
		}

		/* N.B.: radeon wants 64-byte aligned stride */
		dp->rd_stride = dp->rd_virtx * dp->rd_bpp / 8;
		dp->rd_stride = ROUNDUP(dp->rd_stride, RADEON_STRIDEALIGN);
		DPRINTF(("stride: %d\n", dp->rd_stride));

		dp->rd_offset = sc->sc_fboffset * i;
		dp->rd_fbptr = (vaddr_t)bus_space_vaddr(sc->sc_memt,
		    sc->sc_memh) + dp->rd_offset;
		dp->rd_curoff = sc->sc_fboffset - 4096;	/* 4KB cursor space */
		dp->rd_curptr = dp->rd_fbptr + dp->rd_curoff;

		DPRINTF(("fpbtr = %p\n", (void *)dp->rd_fbptr));

		switch (dp->rd_bpp) {
		case 8:
			dp->rd_format = 2;
			break;
		case 32:
			dp->rd_format = 6;
			break;
		default:
			aprint_error("%s: bad depth %d\n", XNAME(sc),
			    dp->rd_bpp);
			goto error;
		}

		DPRINTF(("init engine\n"));
		/* XXX: this seems suspicious - per display engine
		   initialization? */
		radeonfb_engine_init(dp);

		/* copy the template into place */
		dp->rd_wsscreens_storage[0] = radeonfb_stdscreen;
		dp->rd_wsscreens = dp->rd_wsscreens_storage;

		/* and make up the list */
		dp->rd_wsscreenlist.nscreens = 1;
		dp->rd_wsscreenlist.screens = (void *)&dp->rd_wsscreens;

		vcons_init(&dp->rd_vd, dp, dp->rd_wsscreens,
		    &radeonfb_accessops);

		dp->rd_vd.init_screen = radeonfb_init_screen;

#ifdef RADEONFB_DEBUG
		dp->rd_virty -= 200;
#endif

		dp->rd_console = 0;
		prop_dictionary_get_bool(device_properties(sc->sc_dev),
		    "is_console", &dp->rd_console);

		dp->rd_vscreen.scr_flags |= VCONS_SCREEN_IS_STATIC;


		vcons_init_screen(&dp->rd_vd, &dp->rd_vscreen,
		    dp->rd_console, &defattr);

		ri = &dp->rd_vscreen.scr_ri;

		/* clear the screen */
		rasops_unpack_attr(defattr, &fg, &bg, &ul);
		dp->rd_bg = ri->ri_devcmap[bg & 0xf];
		radeonfb_rectfill(dp, 0, 0, ri->ri_width, ri->ri_height,
		    dp->rd_bg);

		dp->rd_wsscreens->textops = &ri->ri_ops;
		dp->rd_wsscreens->capabilities = ri->ri_caps;
		dp->rd_wsscreens->nrows = ri->ri_rows;
		dp->rd_wsscreens->ncols = ri->ri_cols;

#ifdef SPLASHSCREEN
		dp->rd_splash.si_depth = ri->ri_depth;
		dp->rd_splash.si_bits = ri->ri_bits;
		dp->rd_splash.si_hwbits = ri->ri_hwbits;
		dp->rd_splash.si_width = ri->ri_width;
		dp->rd_splash.si_height = ri->ri_height;
		dp->rd_splash.si_stride = ri->ri_stride;
		dp->rd_splash.si_fillrect = NULL;
#endif
		dp->rd_gc.gc_bitblt = radeonfb_bitblt;
		dp->rd_gc.gc_rectfill = radeonfb_rectfill_a;
		dp->rd_gc.gc_rop = RADEON_ROP3_S;
		dp->rd_gc.gc_blitcookie = dp;
		/*
		 * use memory between framebuffer and cursor area as glyph
		 * cache, cap at 4096 lines
		 */
		glyphcache_init(&dp->rd_gc, dp->rd_virty + 4,
		    min(4096, 
		        (dp->rd_curoff / dp->rd_stride) - (dp->rd_virty + 4)),
		    dp->rd_virtx,
		    ri->ri_font->fontwidth,
		    ri->ri_font->fontheight,
		    defattr);
		if (dp->rd_console) {

			radeonfb_modeswitch(dp);
			wsdisplay_cnattach(dp->rd_wsscreens, ri, 0, 0,
			    defattr);
#ifdef SPLASHSCREEN
			if (splash_render(&dp->rd_splash,
			    SPLASH_F_CENTER|SPLASH_F_FILL) == 0)
				SCREEN_DISABLE_DRAWING(&dp->rd_vscreen);
			else
#endif
				vcons_replay_msgbuf(&dp->rd_vscreen);
		} else {

			/*
			 * since we're not the console we can postpone
			 * the rest until someone actually allocates a
			 * screen for us.  but we do clear the screen
			 * at least.
			 */
			memset(ri->ri_bits, 0, 1024);

			radeonfb_modeswitch(dp);
#ifdef SPLASHSCREEN
			if (splash_render(&dp->rd_splash,
			    SPLASH_F_CENTER|SPLASH_F_FILL) == 0)
				SCREEN_DISABLE_DRAWING(&dp->rd_vscreen);
#endif
		}

		aa.console = dp->rd_console;
		aa.scrdata = &dp->rd_wsscreenlist;
		aa.accessops = &radeonfb_accessops;
		aa.accesscookie = &dp->rd_vd;

		config_found(sc->sc_dev, &aa, wsemuldisplaydevprint);
		
		radeonfb_blank(dp, 0);

		/* Initialise delayed lvds operations for backlight. */
		callout_init(&dp->rd_bl_lvds_co, 0);
		callout_setfunc(&dp->rd_bl_lvds_co,
				radeonfb_lvds_callout, dp);
		dp->rd_bl_on = 1;
		dp->rd_bl_level = radeonfb_get_backlight(dp);
		radeonfb_set_backlight(dp, dp->rd_bl_level);
	}

	for (i = 0; i < RADEON_NDISPLAYS; i++)
		radeonfb_init_palette(&sc->sc_displays[i]);
 
	if (HAS_CRTC2(sc)) {
		CLR32(sc, RADEON_CRTC2_GEN_CNTL, RADEON_CRTC2_DISP_DIS);
	}

	CLR32(sc, RADEON_CRTC_EXT_CNTL, RADEON_CRTC_DISPLAY_DIS);
	SET32(sc, RADEON_FP_GEN_CNTL, RADEON_FP_FPON);
	pmf_event_register(dev, PMFE_DISPLAY_BRIGHTNESS_UP,
	    radeonfb_brightness_up, TRUE);
	pmf_event_register(dev, PMFE_DISPLAY_BRIGHTNESS_DOWN,
	    radeonfb_brightness_down, TRUE);

	/*
	 * if we attach a DRM we need to unmap registers in
	 * WSDISPLAYIO_MODE_MAPPED, since this keeps us from doing things like
	 * screen blanking we only do it if needed
	 */
	sc->sc_needs_unmap = 
	    (config_found_ia(dev, "drm", aux, radeonfb_drm_print) != 0);
	DPRINTF(("needs_unmap: %d\n", sc->sc_needs_unmap));

	PRINTREG(RADEON_CRTC_EXT_CNTL);
	PRINTREG(RADEON_CRTC_GEN_CNTL);
	PRINTREG(RADEON_CRTC2_GEN_CNTL);
	PRINTREG(RADEON_DISP_OUTPUT_CNTL);
	PRINTREG(RADEON_DAC_CNTL2);
	PRINTREG(RADEON_FP_GEN_CNTL);
	PRINTREG(RADEON_FP2_GEN_CNTL);

	return;

error:
	if (sc->sc_biossz)
		free(sc->sc_bios, M_DEVBUF);

	if (sc->sc_regsz)
		bus_space_unmap(sc->sc_regt, sc->sc_regh, sc->sc_regsz);

	if (sc->sc_memsz)
		bus_space_unmap(sc->sc_memt, sc->sc_memh, sc->sc_memsz);
}

static void
radeonfb_map(struct radeonfb_softc *sc)
{
	if (!sc->sc_mapped) {
		if (bus_space_map(sc->sc_regt, sc->sc_regaddr, sc->sc_regsz, 0,
		    &sc->sc_regh) != 0) {
			aprint_error_dev(sc->sc_dev, 
			    "unable to map registers!\n");
			return;
		}
		if (bus_space_map(sc->sc_memt, sc->sc_memaddr, sc->sc_memsz,
		    BUS_SPACE_MAP_LINEAR, &sc->sc_memh) != 0) {
			sc->sc_memsz = 0;
			aprint_error_dev(sc->sc_dev,
			    "Unable to map frame buffer\n");
			return;
		}
		sc->sc_mapped = TRUE;
	}
}

static void
radeonfb_unmap(struct radeonfb_softc *sc)
{
	if (!sc->sc_needs_unmap)
		return;

	if (sc->sc_mapped) {
		bus_space_unmap(sc->sc_regt, sc->sc_regh, sc->sc_regsz);
		bus_space_unmap(sc->sc_memt, sc->sc_memh, sc->sc_memsz);
		sc->sc_mapped = FALSE;
	}
}

static int
radeonfb_drm_print(void *aux, const char *pnp)
{
	if (pnp)
		aprint_normal("drm at %s", pnp);
	return (UNCONF);
}

int
radeonfb_ioctl(void *v, void *vs,
    unsigned long cmd, void *d, int flag, struct lwp *l)
{
	struct vcons_data	*vd;
	struct radeonfb_display	*dp;
	struct radeonfb_softc	*sc;
	struct wsdisplay_param  *param;
	struct vcons_screen 	*ms;

	vd = (struct vcons_data *)v;
	ms = vd->active;
	dp = (struct radeonfb_display *)vd->cookie;
	sc = dp->rd_softc;

	/* can't do these without registers being mapped */
	if (!sc->sc_mapped) {
		switch (cmd) {
			case WSDISPLAYIO_GVIDEO:
			case WSDISPLAYIO_SVIDEO:
			case WSDISPLAYIO_GETCMAP:
			case WSDISPLAYIO_PUTCMAP:
			case WSDISPLAYIO_SCURSOR:
			case WSDISPLAYIO_GCURPOS:
			case WSDISPLAYIO_SCURPOS:
			case WSDISPLAYIO_SETPARAM:
				return EINVAL;
		}
	}

	switch (cmd) {
	case WSDISPLAYIO_GTYPE:
		*(unsigned *)d = WSDISPLAY_TYPE_PCIMISC;
		return 0;

	case WSDISPLAYIO_GINFO:
		if (vd->active != NULL) {
			struct wsdisplay_fbinfo *fb;
			fb = (struct wsdisplay_fbinfo *)d;
			fb->width = dp->rd_virtx; 
			fb->height = dp->rd_virty; 
			fb->depth = dp->rd_bpp;
			fb->cmsize = 256;
			return 0;
		} else
			return ENODEV;
	case WSDISPLAYIO_GVIDEO:
		if (radeonfb_isblank(dp))
			*(unsigned *)d = WSDISPLAYIO_VIDEO_OFF;
		else
			*(unsigned *)d = WSDISPLAYIO_VIDEO_ON;
		return 0;

	case WSDISPLAYIO_SVIDEO:
		radeonfb_blank(dp,
		    (*(unsigned int *)d == WSDISPLAYIO_VIDEO_OFF));
		radeonfb_switch_backlight(dp,
		    (*(unsigned int *)d == WSDISPLAYIO_VIDEO_ON));
		return 0;

	case WSDISPLAYIO_GETCMAP:
		if (dp->rd_bpp == 8)
			return radeonfb_getcmap(dp,
			    (struct wsdisplay_cmap *)d);
		return EINVAL;
		
	case WSDISPLAYIO_PUTCMAP:
		if (dp->rd_bpp == 8)
			return radeonfb_putcmap(dp,
			    (struct wsdisplay_cmap *)d);
		return EINVAL;
		
	case WSDISPLAYIO_LINEBYTES:
		*(unsigned *)d = dp->rd_stride;
		return 0;

	case WSDISPLAYIO_SMODE:
		if (*(int *)d != dp->rd_wsmode) {
			dp->rd_wsmode = *(int *)d;
			if ((dp->rd_wsmode == WSDISPLAYIO_MODE_EMUL) ||
			    (dp->rd_wsmode == WSDISPLAYIO_MODE_DUMBFB))
				radeonfb_map(sc);
			
			if ((dp->rd_wsmode == WSDISPLAYIO_MODE_EMUL) &&
			    (dp->rd_vd.active)) {
				radeonfb_engine_init(dp);
				glyphcache_wipe(&dp->rd_gc);
				radeonfb_init_palette(dp);
				radeonfb_modeswitch(dp);
				radeonfb_rectfill(dp, 0, 0, dp->rd_virtx,
				    dp->rd_virty, dp->rd_bg);
				vcons_redraw_screen(dp->rd_vd.active);
			}
			if (dp->rd_wsmode == WSDISPLAYIO_MODE_MAPPED)
				radeonfb_unmap(sc);
		}
		return 0;

	case WSDISPLAYIO_GCURMAX:
		((struct wsdisplay_curpos *)d)->x = RADEON_CURSORMAXX;
		((struct wsdisplay_curpos *)d)->y = RADEON_CURSORMAXY;
		return 0;

	case WSDISPLAYIO_SCURSOR:
		return radeonfb_set_cursor(dp, (struct wsdisplay_cursor *)d);

	case WSDISPLAYIO_GCURSOR:
		return EPASSTHROUGH;

	case WSDISPLAYIO_GCURPOS:
		((struct wsdisplay_curpos *)d)->x = dp->rd_cursor.rc_pos.x;
		((struct wsdisplay_curpos *)d)->y = dp->rd_cursor.rc_pos.y;
		return 0;

	case WSDISPLAYIO_SCURPOS:
		return radeonfb_set_curpos(dp, (struct wsdisplay_curpos *)d);

	case WSDISPLAYIO_SSPLASH:
#if defined(SPLASHSCREEN)
		if (*(int *)d == 1) {
			SCREEN_DISABLE_DRAWING(&dp->rd_vscreen);
			splash_render(&dp->rd_splash,
			    SPLASH_F_CENTER|SPLASH_F_FILL);
		} else
			SCREEN_ENABLE_DRAWING(&dp->rd_vscreen);
		return 0;
#else
		return ENODEV;
#endif
	case WSDISPLAYIO_GETPARAM:
		param = (struct wsdisplay_param *)d;
		switch (param->param) {
		case WSDISPLAYIO_PARAM_BRIGHTNESS:
			param->min = 0;
			param->max = 255;
			param->curval = dp->rd_bl_level;
			return 0;
		case WSDISPLAYIO_PARAM_BACKLIGHT:
			param->min = 0;
			param->max = RADEONFB_BACKLIGHT_MAX;
			param->curval = dp->rd_bl_on;
			return 0;
		}
		return EPASSTHROUGH;

	case WSDISPLAYIO_SETPARAM:
		param = (struct wsdisplay_param *)d;
		switch (param->param) {
		case WSDISPLAYIO_PARAM_BRIGHTNESS:
			radeonfb_set_backlight(dp, param->curval);
			return 0;
		case WSDISPLAYIO_PARAM_BACKLIGHT:
			radeonfb_switch_backlight(dp,  param->curval);
			return 0;
		}
		return EPASSTHROUGH;

	/* PCI config read/write passthrough. */
	case PCI_IOC_CFGREAD:
	case PCI_IOC_CFGWRITE:
		return pci_devioctl(sc->sc_pc, sc->sc_pt, cmd, d, flag, l);

	case WSDISPLAYIO_GET_BUSID:
		return wsdisplayio_busid_pci(sc->sc_dev, sc->sc_pc,
		    sc->sc_pt, d);

	case WSDISPLAYIO_GET_EDID: {
		struct wsdisplayio_edid_info *ei = d;
		return wsdisplayio_get_edid(sc->sc_dev, ei);
	}

	case WSDISPLAYIO_GET_FBINFO: {
		struct wsdisplayio_fbinfo *fbi = d;
		return wsdisplayio_get_fbinfo(&ms->scr_ri, fbi);
	}

	default:
		return EPASSTHROUGH;
	}
}

paddr_t
radeonfb_mmap(void *v, void *vs, off_t offset, int prot)
{
	struct vcons_data	*vd;
	struct radeonfb_display	*dp;
	struct radeonfb_softc	*sc;
	paddr_t			pa;

	vd = (struct vcons_data *)v;
	dp = (struct radeonfb_display *)vd->cookie;
	sc = dp->rd_softc;

	if ((offset >= 0) && (offset < (dp->rd_virty * dp->rd_stride))) {
		pa = bus_space_mmap(sc->sc_memt,
		    sc->sc_memaddr + dp->rd_offset + offset, 0,
		    prot, BUS_SPACE_MAP_LINEAR);
		return pa;
	}

	/*
	 * restrict all other mappings to processes with superuser privileges
	 * or the kernel itself
	 */
	if (kauth_authorize_machdep(kauth_cred_get(), KAUTH_MACHDEP_UNMANAGEDMEM,
	    NULL, NULL, NULL, NULL) != 0) {
		aprint_error_dev(sc->sc_dev, "mmap() rejected.\n");
		return -1;
	}

	if ((offset >= sc->sc_regaddr) && 
	    (offset < sc->sc_regaddr + sc->sc_regsz)) {
		return bus_space_mmap(sc->sc_regt, offset, 0, prot, 
		    BUS_SPACE_MAP_LINEAR);
	}

	if ((offset >= sc->sc_memaddr) && 
	    (offset < sc->sc_memaddr + sc->sc_memsz)) {
		return bus_space_mmap(sc->sc_memt, offset, 0, prot, 
		    BUS_SPACE_MAP_LINEAR);
	}

	if ((offset >= sc->sc_romaddr) && 
	    (offset < sc->sc_romaddr + sc->sc_romsz)) {
		return bus_space_mmap(sc->sc_memt, offset, 0, prot, 
		    BUS_SPACE_MAP_LINEAR);
	}

#ifdef PCI_MAGIC_IO_RANGE
	/* allow mapping of IO space */
	if ((offset >= PCI_MAGIC_IO_RANGE) &&
	    (offset < PCI_MAGIC_IO_RANGE + 0x10000)) {
		pa = bus_space_mmap(sc->sc_iot, offset - PCI_MAGIC_IO_RANGE,
		    0, prot, 0);	
		return pa;
	}	
#endif /* PCI_MAGIC_IO_RANGE */

	return -1;
}

static void
radeonfb_loadbios(struct radeonfb_softc *sc, const struct pci_attach_args *pa)
{
	bus_space_tag_t		romt;
	bus_space_handle_t	romh, biosh;
	bus_size_t		romsz;
	bus_addr_t		ptr;

	if (pci_mapreg_map(pa, PCI_MAPREG_ROM, PCI_MAPREG_TYPE_ROM,
		BUS_SPACE_MAP_PREFETCHABLE, &romt, &romh, NULL, &romsz) != 0) {
		aprint_verbose("%s: unable to map BIOS!\n", XNAME(sc));
		return;
	}

	pci_find_rom(pa, romt, romh, romsz, PCI_ROM_CODE_TYPE_X86, &biosh,
	    &sc->sc_biossz);
	if (sc->sc_biossz == 0) {
		aprint_verbose("%s: Video BIOS not present\n", XNAME(sc));
		return;
	}

	sc->sc_bios = malloc(sc->sc_biossz, M_DEVBUF, M_WAITOK);
	bus_space_read_region_1(romt, biosh, 0, sc->sc_bios, sc->sc_biossz);

	/* unmap the PCI expansion rom */
	bus_space_unmap(romt, romh, romsz);

	/* turn off rom decoder now */
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_MAPREG_ROM,
	    pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_MAPREG_ROM) &
	    ~PCI_MAPREG_ROM_ENABLE);

	ptr = GETBIOS16(sc, 0x48);
	if ((GETBIOS32(sc, ptr + 4) == 0x41544f4d /* "ATOM" */) ||
	    (GETBIOS32(sc, ptr + 4) == 0x4d4f5441 /* "MOTA" */)) {
		sc->sc_flags |= RFB_ATOM;
	}

	aprint_verbose("%s: Found %d KB %s BIOS\n", XNAME(sc),
	    (unsigned)sc->sc_biossz >> 10, IS_ATOM(sc) ? "ATOM" : "Legacy");
}


uint32_t
radeonfb_get32(struct radeonfb_softc *sc, uint32_t reg)
{

	return bus_space_read_4(sc->sc_regt, sc->sc_regh, reg);
}

void
radeonfb_put32(struct radeonfb_softc *sc, uint32_t reg, uint32_t val)
{

	bus_space_write_4(sc->sc_regt, sc->sc_regh, reg, val);
}

void
radeonfb_put32s(struct radeonfb_softc *sc, uint32_t reg, uint32_t val)
{

	bus_space_write_stream_4(sc->sc_regt, sc->sc_regh, reg, val);
}

void
radeonfb_mask32(struct radeonfb_softc *sc, uint32_t reg,
    uint32_t andmask, uint32_t ormask)
{
	int		s;
	uint32_t	val;

	s = splhigh();
	val = radeonfb_get32(sc, reg);
	val = (val & andmask) | ormask;
	radeonfb_put32(sc, reg, val);
	splx(s);
}

uint32_t
radeonfb_getindex(struct radeonfb_softc *sc, uint32_t idx)
{
	int		s;
	uint32_t	val;

	s = splhigh();
	radeonfb_put32(sc, RADEON_MM_INDEX, idx);
	val = radeonfb_get32(sc, RADEON_MM_DATA);
	splx(s);

	return (val);
}

void
radeonfb_putindex(struct radeonfb_softc *sc, uint32_t idx, uint32_t val)
{
	int	s;

	s = splhigh();
	radeonfb_put32(sc, RADEON_MM_INDEX, idx);
	radeonfb_put32(sc, RADEON_MM_DATA, val);
	splx(s);
}

void
radeonfb_maskindex(struct radeonfb_softc *sc, uint32_t idx,
    uint32_t andmask, uint32_t ormask)
{
	int		s;
	uint32_t	val;

	s = splhigh();
	radeonfb_put32(sc, RADEON_MM_INDEX, idx);
	val = radeonfb_get32(sc, RADEON_MM_DATA);
	val = (val & andmask) | ormask;
	radeonfb_put32(sc, RADEON_MM_DATA, val);
	splx(s);
}

uint32_t
radeonfb_getpll(struct radeonfb_softc *sc, uint32_t idx)
{
	int		s;
	uint32_t	val;

	s = splhigh();
	radeonfb_put32(sc, RADEON_CLOCK_CNTL_INDEX, (idx & 0x3f));
	val = radeonfb_get32(sc, RADEON_CLOCK_CNTL_DATA);
	if (HAS_R300CG(sc))
		radeonfb_r300cg_workaround(sc);
	splx(s);

	return (val);
}

void
radeonfb_putpll(struct radeonfb_softc *sc, uint32_t idx, uint32_t val)
{
	int	s;

	s = splhigh();
	radeonfb_put32(sc, RADEON_CLOCK_CNTL_INDEX, (idx & 0x3f) |
	    RADEON_PLL_WR_EN);
	radeonfb_put32(sc, RADEON_CLOCK_CNTL_DATA, val);
	radeonfb_put32(sc, RADEON_CLOCK_CNTL_INDEX, 0);
	splx(s);
}

void
radeonfb_maskpll(struct radeonfb_softc *sc, uint32_t idx,
    uint32_t andmask, uint32_t ormask)
{
	int		s;
	uint32_t	val;

	s = splhigh();
	radeonfb_put32(sc, RADEON_CLOCK_CNTL_INDEX, (idx & 0x3f) |
		RADEON_PLL_WR_EN);
	val = radeonfb_get32(sc, RADEON_CLOCK_CNTL_DATA);
	val = (val & andmask) | ormask;
	radeonfb_put32(sc, RADEON_CLOCK_CNTL_DATA, val);
	radeonfb_put32(sc, RADEON_CLOCK_CNTL_INDEX, 0);
	splx(s);
}

int
radeonfb_scratch_test(struct radeonfb_softc *sc, int reg, uint32_t v)
{
	uint32_t	saved;

	saved = GET32(sc, reg);
	PUT32(sc, reg, v);
	if (GET32(sc, reg) != v) {
		return -1;
	}
	PUT32(sc, reg, saved);
	return 0;
}

uintmax_t
radeonfb_getprop_num(struct radeonfb_softc *sc, const char *name,
    uintmax_t defval)
{
	prop_number_t	pn;
	pn = prop_dictionary_get(device_properties(sc->sc_dev), name);
	if (pn == NULL) {
		return defval;
	}
	KASSERT(prop_object_type(pn) == PROP_TYPE_NUMBER);
	return (prop_number_integer_value(pn));
}

int
radeonfb_getclocks(struct radeonfb_softc *sc)
{
	bus_addr_t	ptr;
	int		refclk = 0;
	int		refdiv = 0;
	int		minpll = 0;
	int		maxpll = 0;

	/* load initial property values if port/board provides them */
	refclk = radeonfb_getprop_num(sc, "refclk", 0) & 0xffff;
	refdiv = radeonfb_getprop_num(sc, "refdiv", 0) & 0xffff;
	minpll = radeonfb_getprop_num(sc, "minpll", 0) & 0xffffffffU;
	maxpll = radeonfb_getprop_num(sc, "maxpll", 0) & 0xffffffffU;

	PRINTPLL(RADEON_PPLL_REF_DIV);
	PRINTPLL(RADEON_PPLL_DIV_0);
	PRINTPLL(RADEON_PPLL_DIV_1);
	PRINTPLL(RADEON_PPLL_DIV_2);
	PRINTPLL(RADEON_PPLL_DIV_3);
	PRINTREG(RADEON_CLOCK_CNTL_INDEX);
	PRINTPLL(RADEON_P2PLL_REF_DIV);
	PRINTPLL(RADEON_P2PLL_DIV_0);

	if (refclk && refdiv && minpll && maxpll)
		goto dontprobe;

	if (!sc->sc_biossz) {
		/* no BIOS */
		aprint_verbose("%s: No video BIOS, using default clocks\n",
		    XNAME(sc));
		if (IS_IGP(sc))
			refclk = refclk ? refclk : 1432;
		else
			refclk = refclk ? refclk : 2700;
		refdiv = refdiv ? refdiv : 12;
		minpll = minpll ? minpll : 12500;
		/* XXX
		 * Need to check if the firmware or something programmed a
		 * higher value than this, and if so, bump it.
		 * The RV280 in my iBook is unhappy if the PLL input is less
		 * than 360MHz
		 */
		maxpll = maxpll ? maxpll : 40000/*35000*/;
	} else if (IS_ATOM(sc)) {
		/* ATOM BIOS */
		ptr = GETBIOS16(sc, 0x48);
		ptr = GETBIOS16(sc, ptr + 32);	/* aka MasterDataStart */
		ptr = GETBIOS16(sc, ptr + 12);	/* pll info block */
		refclk = refclk ? refclk : GETBIOS16(sc, ptr + 82);
		minpll = minpll ? minpll : GETBIOS16(sc, ptr + 78);
		maxpll = maxpll ? maxpll : GETBIOS16(sc, ptr + 32);
		/*
		 * ATOM BIOS doesn't supply a reference divider, so we
		 * have to probe for it.
		 */
		if (refdiv < 2)
			refdiv = GETPLL(sc, RADEON_PPLL_REF_DIV) &
			    RADEON_PPLL_REF_DIV_MASK;
		/*
		 * if probe is zero, just assume one that should work
		 * for most parts
		 */
		if (refdiv < 2)
			refdiv = 12;
		
	} else {
		uint32_t tmp = GETPLL(sc, RADEON_PPLL_REF_DIV);
		/* Legacy BIOS */
		ptr = GETBIOS16(sc, 0x48);
		ptr = GETBIOS16(sc, ptr + 0x30);
		if (IS_R300(sc)) {
			refdiv = refdiv ? refdiv :
			    (tmp & R300_PPLL_REF_DIV_ACC_MASK) >>
			    R300_PPLL_REF_DIV_ACC_SHIFT;
		} else {
			refdiv = refdiv ? refdiv :
			    tmp & RADEON_PPLL_REF_DIV_MASK;
		}
		refclk = refclk ? refclk : GETBIOS16(sc, ptr + 0x0E);
		refdiv = refdiv ? refdiv : GETBIOS16(sc, ptr + 0x10);
		minpll = minpll ? minpll : GETBIOS32(sc, ptr + 0x12);
		maxpll = maxpll ? maxpll : GETBIOS32(sc, ptr + 0x16);
	}


dontprobe:
	sc->sc_refclk = refclk * 10;
	sc->sc_refdiv = refdiv;
	sc->sc_minpll = minpll * 10;
	sc->sc_maxpll = maxpll * 10;
	return 0;
}

int
radeonfb_calc_dividers(struct radeonfb_softc *sc, uint32_t dotclock,
    uint32_t *postdivbit, uint32_t *feedbackdiv)
{
	int		i;
	uint32_t	outfreq;
	int		div;

	DPRINTF(("dot clock: %u\n", dotclock));
	for (i = 0; (div = radeonfb_dividers[i].divider) != 0; i++) {
		outfreq = div * dotclock;
		if ((outfreq >= sc->sc_minpll) &&
		    (outfreq <= sc->sc_maxpll)) {
			DPRINTF(("outfreq: %u\n", outfreq));
			*postdivbit =
			    ((uint32_t)radeonfb_dividers[i].mask << 16);
			DPRINTF(("post divider: %d (mask %x)\n", div,
				    *postdivbit));
			break;
		}
	}

	if (div == 0)
		return 1;

	*feedbackdiv = DIVIDE(sc->sc_refdiv * outfreq, sc->sc_refclk);
	DPRINTF(("feedback divider: %d\n", *feedbackdiv));
	return 0;
}

#if 0
#ifdef RADEONFB_DEBUG
static void
dump_buffer(const char *pfx, void *buffer, unsigned int size)
{
	char		asc[17];
	unsigned	ptr = (unsigned)buffer;
	char		*start = (char *)(ptr & ~0xf);
	char		*end = (char *)(ptr + size);

	end = (char *)(((unsigned)end + 0xf) & ~0xf);

	if (pfx == NULL) {
		pfx = "";
	}

	while (start < end) {
		unsigned offset = (unsigned)start & 0xf;
		if (offset == 0) {
			printf("%s%x: ", pfx, (unsigned)start);
		}
		if (((unsigned)start < ptr) ||
		    ((unsigned)start >= (ptr + size))) {
			printf("  ");
			asc[offset] = ' ';
		} else {
			printf("%02x", *(unsigned char *)start);
			if ((*start >= ' ') && (*start <= '~')) {
				asc[offset] = *start;
			} else {
				asc[offset] = '.';
			}
		}
		asc[offset + 1] = 0;
		if (offset % 2) {
			printf(" ");
		}
		if (offset == 15) {
			printf(" %s\n", asc);
		}
		start++;
	}
}
#endif
#endif

int
radeonfb_getconnectors(struct radeonfb_softc *sc)
{
	int	i;
	int	found = 0;

	for (i = 0; i < 2; i++) {
		sc->sc_ports[i].rp_mon_type = RADEON_MT_UNKNOWN;
		sc->sc_ports[i].rp_ddc_type = RADEON_DDC_NONE;
		sc->sc_ports[i].rp_dac_type = RADEON_DAC_UNKNOWN;
		sc->sc_ports[i].rp_conn_type = RADEON_CONN_NONE;
		sc->sc_ports[i].rp_tmds_type = RADEON_TMDS_UNKNOWN;
	}

	/*
	 * This logic is borrowed from Xorg's radeon driver.
	 */
	if (!sc->sc_biossz)
		goto nobios;

	if (IS_ATOM(sc)) {
		/* not done yet */
	} else {
		uint16_t	ptr;
		int		port = 0;

		ptr = GETBIOS16(sc, 0x48);
		ptr = GETBIOS16(sc, ptr + 0x50);
		for (i = 1; i < 4; i++) {
			uint16_t	entry;
			uint8_t		conn, ddc, dac, tmds;

			/*
			 * Parse the connector table.  From reading the code,
			 * it appears to made up of 16-bit entries for each
			 * connector.  The 16-bits are defined as:
			 *
			 * bits 12-15	- connector type (0 == end of table)
			 * bits 8-11	- DDC type
			 * bits 5-7	- ???
			 * bit 4	- TMDS type (1 = EXT, 0 = INT)
			 * bits 1-3	- ???
			 * bit 0	- DAC, 1 = TVDAC, 0 = primary
			 */
			if (!GETBIOS8(sc, ptr + i * 2) && i > 1)
				break;
			entry = GETBIOS16(sc, ptr + i * 2);

			conn = (entry >> 12) & 0xf;
			ddc = (entry >> 8) & 0xf;
			dac = (entry & 0x1) ? RADEON_DAC_TVDAC :
			    RADEON_DAC_PRIMARY;
			tmds = ((entry >> 4) & 0x1) ? RADEON_TMDS_EXT :
			    RADEON_TMDS_INT;

			if (conn == RADEON_CONN_NONE)
				continue;	/* no connector */

			if ((found > 0) &&
			    (sc->sc_ports[port].rp_ddc_type == ddc)) {
				/* duplicate entry for same connector */
				continue;
			}

			/* internal DDC_DVI port gets priority */
			if ((ddc == RADEON_DDC_DVI) || (port == 1))
				port = 0;
			else
				port = 1;

			sc->sc_ports[port].rp_ddc_type =
			    ddc > RADEON_DDC_CRT2 ? RADEON_DDC_NONE : ddc;
			sc->sc_ports[port].rp_dac_type = dac;
			sc->sc_ports[port].rp_conn_type =
			    min(conn, RADEON_CONN_UNSUPPORTED) ;

			sc->sc_ports[port].rp_tmds_type = tmds;

			if ((conn != RADEON_CONN_DVI_I) &&
			    (conn != RADEON_CONN_DVI_D) &&
			    (tmds == RADEON_TMDS_INT))
				sc->sc_ports[port].rp_tmds_type =
				    RADEON_TMDS_UNKNOWN;
			sc->sc_ports[port].rp_number = i - 1;

			found += (port + 1);
		}
	}

nobios:
	if (!found) {
		DPRINTF(("No connector info in BIOS!\n"));
		if IS_MOBILITY(sc) {
			/* default, port 0 = internal TMDS, port 1 = CRT */
			sc->sc_ports[0].rp_mon_type = RADEON_MT_UNKNOWN;
			sc->sc_ports[0].rp_ddc_type = RADEON_DDC_DVI;
			sc->sc_ports[0].rp_dac_type = RADEON_DAC_TVDAC;
			sc->sc_ports[0].rp_conn_type = RADEON_CONN_DVI_D;
			sc->sc_ports[0].rp_tmds_type = RADEON_TMDS_INT;
			sc->sc_ports[0].rp_number = 0;

			sc->sc_ports[1].rp_mon_type = RADEON_MT_UNKNOWN;
			sc->sc_ports[1].rp_ddc_type = RADEON_DDC_VGA;
			sc->sc_ports[1].rp_dac_type = RADEON_DAC_PRIMARY;
			sc->sc_ports[1].rp_conn_type = RADEON_CONN_CRT;
			sc->sc_ports[1].rp_tmds_type = RADEON_TMDS_EXT;
			sc->sc_ports[1].rp_number = 1;
		} else {
			/* default, port 0 = DVI, port 1 = CRT */
			sc->sc_ports[0].rp_mon_type = RADEON_MT_UNKNOWN;
			sc->sc_ports[0].rp_ddc_type = RADEON_DDC_DVI;
			sc->sc_ports[0].rp_dac_type = RADEON_DAC_TVDAC;
			sc->sc_ports[0].rp_conn_type = RADEON_CONN_DVI_D;
			sc->sc_ports[0].rp_tmds_type = RADEON_TMDS_INT;
			sc->sc_ports[0].rp_number = 1;

			sc->sc_ports[1].rp_mon_type = RADEON_MT_UNKNOWN;
			sc->sc_ports[1].rp_ddc_type = RADEON_DDC_VGA;
			sc->sc_ports[1].rp_dac_type = RADEON_DAC_PRIMARY;
			sc->sc_ports[1].rp_conn_type = RADEON_CONN_CRT;
			sc->sc_ports[1].rp_tmds_type = RADEON_TMDS_UNKNOWN;
			sc->sc_ports[1].rp_number = 0;
		}
	}

	/*
	 * Fixup for RS300/RS350/RS400 chips, that lack a primary DAC.
	 * these chips should use TVDAC for the VGA port.
	 */
	if (HAS_SDAC(sc)) {
		if (sc->sc_ports[0].rp_conn_type == RADEON_CONN_CRT) {
			sc->sc_ports[0].rp_dac_type = RADEON_DAC_TVDAC;
			sc->sc_ports[1].rp_dac_type = RADEON_DAC_PRIMARY;
		} else {
			sc->sc_ports[1].rp_dac_type = RADEON_DAC_TVDAC;
			sc->sc_ports[0].rp_dac_type = RADEON_DAC_PRIMARY;
		}
	} else if (!HAS_CRTC2(sc)) {
		sc->sc_ports[0].rp_dac_type = RADEON_DAC_PRIMARY;
	}

	for (i = 0; i < 2; i++) {
		char	edid[128];
		uint8_t	ddc;
		struct edid_info *eip = &sc->sc_ports[i].rp_edid;
		prop_data_t edid_data;

		DPRINTF(("Port #%d:\n", i));
		DPRINTF(("   conn = %d\n", sc->sc_ports[i].rp_conn_type));
		DPRINTF(("    ddc = %d\n", sc->sc_ports[i].rp_ddc_type));
		DPRINTF(("    dac = %d\n", sc->sc_ports[i].rp_dac_type));
		DPRINTF(("   tmds = %d\n", sc->sc_ports[i].rp_tmds_type));
		DPRINTF(("   crtc = %d\n", sc->sc_ports[i].rp_number));

		sc->sc_ports[i].rp_edid_valid = 0;
		/* first look for static EDID data */
		if ((edid_data = prop_dictionary_get(device_properties(
		    sc->sc_dev), "EDID")) != NULL) {

			aprint_debug_dev(sc->sc_dev, "using static EDID\n");
			memcpy(edid, prop_data_data_nocopy(edid_data), 128);
			if (edid_parse(edid, eip) == 0) {

				sc->sc_ports[i].rp_edid_valid = 1;
			}
		}
		/* if we didn't find any we'll try to talk to the monitor */
		if (sc->sc_ports[i].rp_edid_valid != 1) {

			ddc = sc->sc_ports[i].rp_ddc_type;
			if (ddc != RADEON_DDC_NONE) {
				if ((radeonfb_i2c_read_edid(sc, ddc, edid)
				    == 0) && (edid_parse(edid, eip) == 0)) {

					sc->sc_ports[i].rp_edid_valid = 1;
#ifdef RADEONFB_DEBUG
					edid_print(eip);
#endif
				}
			}
		}
	}

	return found;
}

int
radeonfb_gettmds(struct radeonfb_softc *sc)
{
	int	i;

	if (!sc->sc_biossz) {
		goto nobios;
	}

	if (IS_ATOM(sc)) {
		/* XXX: not done yet */
	} else {
		uint16_t	ptr;
		int		n;

		ptr = GETBIOS16(sc, 0x48);
		ptr = GETBIOS16(sc, ptr + 0x34);
		DPRINTF(("DFP table revision %d\n", GETBIOS8(sc, ptr)));
		if (GETBIOS8(sc, ptr) == 3) {	
			/* revision three table */
			n = GETBIOS8(sc, ptr + 5) + 1;
			n = min(n, 4);

			memset(sc->sc_tmds_pll, 0, sizeof (sc->sc_tmds_pll));
			for (i = 0; i < n; i++) {
				sc->sc_tmds_pll[i].rtp_pll = GETBIOS32(sc,
				    ptr + i * 10 + 8);
				sc->sc_tmds_pll[i].rtp_freq = GETBIOS16(sc,
				    ptr + i * 10 + 0x10);
				DPRINTF(("TMDS_PLL dot clock %d pll %x\n",
					    sc->sc_tmds_pll[i].rtp_freq,
					    sc->sc_tmds_pll[i].rtp_pll));
			}
			return 0;
		}
	}

nobios:
	DPRINTF(("no suitable DFP table present\n"));
	for (i = 0;
	     i < sizeof (radeonfb_tmds_pll) / sizeof (radeonfb_tmds_pll[0]);
	     i++) {
		int	j;

		if (radeonfb_tmds_pll[i].family != sc->sc_family)
			continue;

		for (j = 0; j < 4; j++) {
			sc->sc_tmds_pll[j] = radeonfb_tmds_pll[i].plls[j];
			DPRINTF(("TMDS_PLL dot clock %d pll %x\n",
				    sc->sc_tmds_pll[j].rtp_freq,
				    sc->sc_tmds_pll[j].rtp_pll));
		}
		return 0;
	}

	return -1;
}

const struct videomode *
radeonfb_modelookup(const char *name)
{
	int	i;

	for (i = 0; i < videomode_count; i++)
		if (!strcmp(name, videomode_list[i].name))
			return &videomode_list[i];

	return NULL;
}

void
radeonfb_pllwriteupdate(struct radeonfb_softc *sc, int crtc)
{
	if (crtc) {
		while (GETPLL(sc, RADEON_P2PLL_REF_DIV) &
		    RADEON_P2PLL_ATOMIC_UPDATE_R);
		SETPLL(sc, RADEON_P2PLL_REF_DIV, RADEON_P2PLL_ATOMIC_UPDATE_W);
	} else {
		while (GETPLL(sc, RADEON_PPLL_REF_DIV) &
		    RADEON_PPLL_ATOMIC_UPDATE_R);
		SETPLL(sc, RADEON_PPLL_REF_DIV, RADEON_PPLL_ATOMIC_UPDATE_W);
	}
}

void
radeonfb_pllwaitatomicread(struct radeonfb_softc *sc, int crtc)
{
	int	i;

	for (i = 10000; i; i--) {
		if (crtc) {
			if (GETPLL(sc, RADEON_P2PLL_REF_DIV) &
			    RADEON_P2PLL_ATOMIC_UPDATE_R)
				break;
		} else {
			if (GETPLL(sc, RADEON_PPLL_REF_DIV) &
			    RADEON_PPLL_ATOMIC_UPDATE_R)
				break;
		}
	}
}

void
radeonfb_program_vclk(struct radeonfb_softc *sc, int dotclock, int crtc)
{
	uint32_t	pbit = 0;
	uint32_t	feed = 0;
	uint32_t	data, refdiv, div0;

	radeonfb_calc_dividers(sc, dotclock, &pbit, &feed);

	if (crtc == 0) {

		refdiv = GETPLL(sc, RADEON_PPLL_REF_DIV);
		if (IS_R300(sc)) {
			refdiv = (refdiv & ~R300_PPLL_REF_DIV_ACC_MASK) |
			    (sc->sc_refdiv << R300_PPLL_REF_DIV_ACC_SHIFT);
		} else {
			refdiv = (refdiv & ~RADEON_PPLL_REF_DIV_MASK) |
			    sc->sc_refdiv;
		}
		div0 = GETPLL(sc, RADEON_PPLL_DIV_0);
		div0 &= ~(RADEON_PPLL_FB3_DIV_MASK |
		    RADEON_PPLL_POST3_DIV_MASK);
		div0 |= pbit;
		div0 |= (feed & RADEON_PPLL_FB3_DIV_MASK);

		if ((refdiv == GETPLL(sc, RADEON_PPLL_REF_DIV)) &&
		    (div0 == GETPLL(sc, RADEON_PPLL_DIV_0))) {
			/* 
			 * nothing to do here, the PLL is already where we
			 * want it
			 */
			PATCH32(sc, RADEON_CLOCK_CNTL_INDEX, 0,
			    ~RADEON_PLL_DIV_SEL);
			aprint_debug_dev(sc->sc_dev, "no need to touch the PLL\n");
			return;
		}

		/* alright, we do need to reprogram stuff */	 
		PATCHPLL(sc, RADEON_VCLK_ECP_CNTL,
		    RADEON_VCLK_SRC_SEL_CPUCLK,
		    ~RADEON_VCLK_SRC_SEL_MASK);
	    
		/* put vclk into reset, use atomic updates */
		SETPLL(sc, RADEON_PPLL_CNTL,
		    RADEON_PPLL_REFCLK_SEL |
		    RADEON_PPLL_FBCLK_SEL |
		    RADEON_PPLL_RESET |
		    RADEON_PPLL_ATOMIC_UPDATE_EN |
		    RADEON_PPLL_VGA_ATOMIC_UPDATE_EN);

		/* select clock 0 */
		PATCH32(sc, RADEON_CLOCK_CNTL_INDEX, 0,
		    ~RADEON_PLL_DIV_SEL);
		
		PUTPLL(sc, RADEON_PPLL_REF_DIV, refdiv);

		/* xf86-video-radeon does this, not sure why */
		PUTPLL(sc, RADEON_PPLL_DIV_0, div0);
		PUTPLL(sc, RADEON_PPLL_DIV_0, div0);

		/* use the atomic update */
		radeonfb_pllwriteupdate(sc, crtc);

		/* and wait for it to complete */
		radeonfb_pllwaitatomicread(sc, crtc);

		/* program HTOTAL (why?) */
		PUTPLL(sc, RADEON_HTOTAL_CNTL, 0);

		/* drop reset */
		CLRPLL(sc, RADEON_PPLL_CNTL,
		    RADEON_PPLL_RESET | RADEON_PPLL_SLEEP |
		    RADEON_PPLL_ATOMIC_UPDATE_EN |
		    RADEON_PPLL_VGA_ATOMIC_UPDATE_EN);

		PRINTPLL(RADEON_PPLL_CNTL);
		PRINTPLL(RADEON_PPLL_REF_DIV);
		PRINTPLL(RADEON_PPLL_DIV_3);

		/* give clock time to lock */
		delay(50000);

		PATCHPLL(sc, RADEON_VCLK_ECP_CNTL,
		    RADEON_VCLK_SRC_SEL_PPLLCLK,
		    ~RADEON_VCLK_SRC_SEL_MASK);

	} else {

		PATCHPLL(sc, RADEON_PIXCLKS_CNTL,
		    RADEON_PIX2CLK_SRC_SEL_CPUCLK,
		    ~RADEON_PIX2CLK_SRC_SEL_MASK);

		/* put vclk into reset, use atomic updates */
		SETPLL(sc, RADEON_P2PLL_CNTL,
		    RADEON_P2PLL_RESET |
		    RADEON_P2PLL_ATOMIC_UPDATE_EN |
		    RADEON_P2PLL_VGA_ATOMIC_UPDATE_EN);

		/* program reference divider */
		PATCHPLL(sc, RADEON_P2PLL_REF_DIV, sc->sc_refdiv,
		    ~RADEON_P2PLL_REF_DIV_MASK);

		/* program feedback and post dividers */
		data = GETPLL(sc, RADEON_P2PLL_DIV_0);
		data &= ~(RADEON_P2PLL_FB0_DIV_MASK |
		    RADEON_P2PLL_POST0_DIV_MASK);
		data |= pbit;
		data |= (feed & RADEON_P2PLL_FB0_DIV_MASK);
		PUTPLL(sc, RADEON_P2PLL_DIV_0, data);
		PUTPLL(sc, RADEON_P2PLL_DIV_0, data);

		PRINTPLL(RADEON_P2PLL_REF_DIV);
		PRINTPLL(RADEON_P2PLL_DIV_0);

		/* use the atomic update */
		radeonfb_pllwriteupdate(sc, crtc);

		/* and wait for it to complete */
		radeonfb_pllwaitatomicread(sc, crtc);

		/* program HTOTAL (why?) */
		PUTPLL(sc, RADEON_HTOTAL2_CNTL, 0);

		/* drop reset */
		CLRPLL(sc, RADEON_P2PLL_CNTL,
		    RADEON_P2PLL_RESET | RADEON_P2PLL_SLEEP |
		    RADEON_P2PLL_ATOMIC_UPDATE_EN |
		    RADEON_P2PLL_VGA_ATOMIC_UPDATE_EN);

		/* allow time for clock to lock */
		delay(50000);

		PATCHPLL(sc, RADEON_PIXCLKS_CNTL,
		    RADEON_PIX2CLK_SRC_SEL_P2PLLCLK,
		    ~RADEON_PIX2CLK_SRC_SEL_MASK);
	}
	PRINTREG(RADEON_CRTC_MORE_CNTL);
}

void
radeonfb_modeswitch(struct radeonfb_display *dp)
{
	struct radeonfb_softc	*sc = dp->rd_softc;
	int			i;

	/* blank the display while we switch modes */
	radeonfb_blank(dp, 1);

#if 0
	SET32(sc, RADEON_CRTC_EXT_CNTL,
	    RADEON_CRTC_VSYNC_DIS | RADEON_CRTC_HSYNC_DIS |
	    RADEON_CRTC_DISPLAY_DIS /* | RADEON_CRTC_DISP_REQ_EN_B */);
#endif

	/* these registers might get in the way... */
	PUT32(sc, RADEON_OVR_CLR, 0);
	PUT32(sc, RADEON_OVR_WID_LEFT_RIGHT, 0);
	PUT32(sc, RADEON_OVR_WID_TOP_BOTTOM, 0);
	PUT32(sc, RADEON_OV0_SCALE_CNTL, 0);
	PUT32(sc, RADEON_SUBPIC_CNTL, 0);
	PUT32(sc, RADEON_VIPH_CONTROL, 0);
	PUT32(sc, RADEON_I2C_CNTL_1, 0);
	PUT32(sc, RADEON_GEN_INT_CNTL, 0);
	PUT32(sc, RADEON_CAP0_TRIG_CNTL, 0);
	PUT32(sc, RADEON_CAP1_TRIG_CNTL, 0);
	PUT32(sc, RADEON_SURFACE_CNTL, 0);

	for (i = 0; i < dp->rd_ncrtcs; i++)
		radeonfb_setcrtc(dp, i);

	/* activate the display */
	radeonfb_blank(dp, 0);
}

void
radeonfb_setcrtc(struct radeonfb_display *dp, int index)
{
	int			crtc;
	struct videomode	*mode;
	struct radeonfb_softc	*sc;
	struct radeonfb_crtc	*cp;
	uint32_t		v;
	uint32_t		gencntl;
	uint32_t		htotaldisp;
	uint32_t		hsyncstrt;
	uint32_t		vtotaldisp;
	uint32_t		vsyncstrt;
	uint32_t		fphsyncstrt;
	uint32_t		fpvsyncstrt;
	uint32_t		fphtotaldisp;
	uint32_t		fpvtotaldisp;
	uint32_t		pitch;

	sc = dp->rd_softc;
	cp = &dp->rd_crtcs[index];
	crtc = cp->rc_number;
	mode = &cp->rc_videomode;

#if 1
	pitch = dp->rd_stride / dp->rd_bpp;
#else
	pitch = (((sc->sc_maxx * sc->sc_maxbpp) + ((sc->sc_maxbpp * 8) - 1)) /
	    (sc->sc_maxbpp * 8));
#endif
	switch (crtc) {
	case 0:
		gencntl = RADEON_CRTC_GEN_CNTL;
		htotaldisp = RADEON_CRTC_H_TOTAL_DISP;
		hsyncstrt = RADEON_CRTC_H_SYNC_STRT_WID;
		vtotaldisp = RADEON_CRTC_V_TOTAL_DISP;
		vsyncstrt = RADEON_CRTC_V_SYNC_STRT_WID;
		fpvsyncstrt = RADEON_FP_V_SYNC_STRT_WID;
		fphsyncstrt = RADEON_FP_H_SYNC_STRT_WID;
		fpvtotaldisp = RADEON_FP_CRTC_V_TOTAL_DISP;
		fphtotaldisp = RADEON_FP_CRTC_H_TOTAL_DISP;
		break;
	case 1:
		gencntl = RADEON_CRTC2_GEN_CNTL;
		htotaldisp = RADEON_CRTC2_H_TOTAL_DISP;
		hsyncstrt = RADEON_CRTC2_H_SYNC_STRT_WID;
		vtotaldisp = RADEON_CRTC2_V_TOTAL_DISP;
		vsyncstrt = RADEON_CRTC2_V_SYNC_STRT_WID;
		fpvsyncstrt = RADEON_FP_V2_SYNC_STRT_WID;
		fphsyncstrt = RADEON_FP_H2_SYNC_STRT_WID;
		fpvtotaldisp = RADEON_FP_CRTC2_V_TOTAL_DISP;
		fphtotaldisp = RADEON_FP_CRTC2_H_TOTAL_DISP;
		break;
	default:
		panic("Bad CRTC!");
		break;
	}

	/*
	 * CRTC_GEN_CNTL - depth, accelerator mode, etc.
	 */
	/* only bother with 32bpp and 8bpp */
	v = dp->rd_format << RADEON_CRTC_PIX_WIDTH_SHIFT;

	if (crtc == 1) {
		v |= RADEON_CRTC2_CRT2_ON | RADEON_CRTC2_EN;
	} else {
		v |= RADEON_CRTC_EXT_DISP_EN | RADEON_CRTC_EN;
	}

	if (mode->flags & VID_DBLSCAN)
		v |= RADEON_CRTC2_DBL_SCAN_EN;

	if (mode->flags & VID_INTERLACE)
		v |= RADEON_CRTC2_INTERLACE_EN;

	if (mode->flags & VID_CSYNC) {
		v |= RADEON_CRTC2_CSYNC_EN;
		if (crtc == 1)
			v |= RADEON_CRTC2_VSYNC_TRISTAT;
	}
	
	PUT32(sc, gencntl, v);
	DPRINTF(("CRTC%s_GEN_CNTL = %08x\n", crtc ? "2" : "", v));

	/*
	 * CRTC_EXT_CNTL - preserve disable flags, set ATI linear and EXT_CNT
	 */
	v = GET32(sc, RADEON_CRTC_EXT_CNTL);
	if (crtc == 0) {
		v &= (RADEON_CRTC_VSYNC_DIS | RADEON_CRTC_HSYNC_DIS |
		    RADEON_CRTC_DISPLAY_DIS);
		v |= RADEON_XCRT_CNT_EN | RADEON_VGA_ATI_LINEAR;
		if (mode->flags & VID_CSYNC)
			v |= RADEON_CRTC_VSYNC_TRISTAT;
	}
	/* unconditional turn on CRT, in case first CRTC is DFP */
	v |= RADEON_CRTC_CRT_ON;
	PUT32(sc, RADEON_CRTC_EXT_CNTL, v);
	PRINTREG(RADEON_CRTC_EXT_CNTL);

	/*
	 * H_TOTAL_DISP
	 */
	v = ((mode->hdisplay / 8) - 1) << 16;
	v |= (mode->htotal / 8) - 1;
	PUT32(sc, htotaldisp, v);
	DPRINTF(("CRTC%s_H_TOTAL_DISP = %08x\n", crtc ? "2" : "", v));
	PUT32(sc, fphtotaldisp, v);
	DPRINTF(("FP_H%s_TOTAL_DISP = %08x\n", crtc ? "2" : "", v));

	/*
	 * H_SYNC_STRT_WID
	 */
	v = (((mode->hsync_end - mode->hsync_start) / 8) << 16);
	v |= (mode->hsync_start - 8);	/* match xf86-video-radeon */
	if (mode->flags & VID_NHSYNC)
		v |= RADEON_CRTC_H_SYNC_POL;
	PUT32(sc, hsyncstrt, v);
	DPRINTF(("CRTC%s_H_SYNC_STRT_WID = %08x\n", crtc ? "2" : "", v));
	PUT32(sc, fphsyncstrt, v);
	DPRINTF(("FP_H%s_SYNC_STRT_WID = %08x\n", crtc ? "2" : "", v));

	/*
	 * V_TOTAL_DISP
	 */
	v = ((mode->vdisplay - 1) << 16);
	v |= (mode->vtotal - 1);
	PUT32(sc, vtotaldisp, v);
	DPRINTF(("CRTC%s_V_TOTAL_DISP = %08x\n", crtc ? "2" : "", v));
	PUT32(sc, fpvtotaldisp, v);
	DPRINTF(("FP_V%s_TOTAL_DISP = %08x\n", crtc ? "2" : "", v));

	/*
	 * V_SYNC_STRT_WID
	 */
	v = ((mode->vsync_end - mode->vsync_start) << 16);
	v |= (mode->vsync_start - 1);
	if (mode->flags & VID_NVSYNC)
		v |= RADEON_CRTC_V_SYNC_POL;
	PUT32(sc, vsyncstrt, v);
	DPRINTF(("CRTC%s_V_SYNC_STRT_WID = %08x\n", crtc ? "2" : "", v));
	PUT32(sc, fpvsyncstrt, v);
	DPRINTF(("FP_V%s_SYNC_STRT_WID = %08x\n", crtc ? "2" : "", v));

	radeonfb_program_vclk(sc, mode->dot_clock, crtc);

	switch (crtc) {
	case 0:
		PUT32(sc, RADEON_CRTC_OFFSET, 0);
		PUT32(sc, RADEON_CRTC_OFFSET_CNTL, 0);
		PUT32(sc, RADEON_CRTC_PITCH, pitch);
		CLR32(sc, RADEON_DISP_MERGE_CNTL, RADEON_DISP_RGB_OFFSET_EN);

		CLR32(sc, RADEON_CRTC_EXT_CNTL,
		    RADEON_CRTC_VSYNC_DIS | RADEON_CRTC_HSYNC_DIS |
		    RADEON_CRTC_DISPLAY_DIS /* | RADEON_CRTC_DISP_REQ_EN_B */);
		CLR32(sc, RADEON_CRTC_GEN_CNTL, RADEON_CRTC_DISP_REQ_EN_B);
		PRINTREG(RADEON_CRTC_EXT_CNTL);
		PRINTREG(RADEON_CRTC_GEN_CNTL);
		PRINTREG(RADEON_CLOCK_CNTL_INDEX);
		break;

	case 1:
		PUT32(sc, RADEON_CRTC2_OFFSET, 0);
		PUT32(sc, RADEON_CRTC2_OFFSET_CNTL, 0);
		PUT32(sc, RADEON_CRTC2_PITCH, pitch);
		CLR32(sc, RADEON_DISP2_MERGE_CNTL, RADEON_DISP2_RGB_OFFSET_EN);
		CLR32(sc, RADEON_CRTC2_GEN_CNTL,
		    RADEON_CRTC2_VSYNC_DIS |
		    RADEON_CRTC2_HSYNC_DIS |
		    RADEON_CRTC2_DISP_DIS | RADEON_CRTC2_DISP_REQ_EN_B); 
		PRINTREG(RADEON_CRTC2_GEN_CNTL);
		break;
	}
}

int
radeonfb_isblank(struct radeonfb_display *dp)
{
	uint32_t	reg, mask;

	if(!dp->rd_softc->sc_mapped)
		return 1;

	if (dp->rd_crtcs[0].rc_number) {
		reg = RADEON_CRTC2_GEN_CNTL;
		mask = RADEON_CRTC2_DISP_DIS;
	} else {
		reg = RADEON_CRTC_EXT_CNTL;
		mask = RADEON_CRTC_DISPLAY_DIS;
	}
	return ((GET32(dp->rd_softc, reg) & mask) ? 1 : 0);
}

void
radeonfb_blank(struct radeonfb_display *dp, int blank)
{
	struct radeonfb_softc	*sc = dp->rd_softc;
	uint32_t		reg, mask;
	uint32_t		fpreg, fpval;
	int			i;

	if (!sc->sc_mapped)
		return;

	for (i = 0; i < dp->rd_ncrtcs; i++) {

		if (dp->rd_crtcs[i].rc_number) {
			reg = RADEON_CRTC2_GEN_CNTL;
			mask = RADEON_CRTC2_DISP_DIS;
			fpreg = RADEON_FP2_GEN_CNTL;
			fpval = RADEON_FP2_ON;
		} else {
			reg = RADEON_CRTC_EXT_CNTL;
			mask = RADEON_CRTC_DISPLAY_DIS;
			fpreg = RADEON_FP_GEN_CNTL;
			fpval = RADEON_FP_FPON;
		}
	
		if (blank) {
			SET32(sc, reg, mask);
			CLR32(sc, fpreg, fpval);
		} else {
			CLR32(sc, reg, mask);
			SET32(sc, fpreg, fpval);
		}
	}
	PRINTREG(RADEON_FP_GEN_CNTL);
	PRINTREG(RADEON_FP2_GEN_CNTL);
}

void
radeonfb_init_screen(void *cookie, struct vcons_screen *scr, int existing,
    long *defattr)
{
	struct radeonfb_display *dp = cookie;
	struct rasops_info *ri = &scr->scr_ri;

	/* initialize font subsystem */
	wsfont_init();

	DPRINTF(("init screen called, existing %d\n", existing));

	ri->ri_depth = dp->rd_bpp;
	ri->ri_width = dp->rd_virtx;
	ri->ri_height = dp->rd_virty;
	ri->ri_stride = dp->rd_stride;
	ri->ri_flg = RI_CENTER;
	switch (ri->ri_depth) {
		case 8:
			ri->ri_flg |= RI_ENABLE_ALPHA | RI_8BIT_IS_RGB;
			break;
		case 32:
			ri->ri_flg |= RI_ENABLE_ALPHA;
			/* we run radeons in RGB even on SPARC hardware */
			ri->ri_rnum = 8;
			ri->ri_gnum = 8;
			ri->ri_bnum = 8;
			ri->ri_rpos = 16;
			ri->ri_gpos = 8;
			ri->ri_bpos = 0;
			break;
	}

	ri->ri_bits = (void *)dp->rd_fbptr;

#ifdef VCONS_DRAW_INTR
	scr->scr_flags |= VCONS_DONT_READ;
#endif

	if (existing) {
		ri->ri_flg |= RI_CLEAR;

		/* start a modeswitch now */
		radeonfb_modeswitch(dp);
	}

	/*
	 * XXX: font selection should be based on properties, with some
	 * normal/reasonable default.
	 */

	/* initialize and look for an initial font */
	rasops_init(ri, 0, 0);
	ri->ri_caps = WSSCREEN_UNDERLINE | WSSCREEN_HILIT |
		    WSSCREEN_WSCOLORS | WSSCREEN_REVERSE;

	rasops_reconfig(ri, dp->rd_virty / ri->ri_font->fontheight,
		    dp->rd_virtx / ri->ri_font->fontwidth);

	/* enable acceleration */
	dp->rd_putchar = ri->ri_ops.putchar;
	ri->ri_ops.copyrows = radeonfb_copyrows;
	ri->ri_ops.copycols = radeonfb_copycols;
	ri->ri_ops.eraserows = radeonfb_eraserows;
	ri->ri_ops.erasecols = radeonfb_erasecols;
	/* pick a putchar method based on font and Radeon model */
	if (ri->ri_font->stride < ri->ri_font->fontwidth) {
		/* got a bitmap font */
#ifndef RADEONFB_ALWAYS_ACCEL_PUTCHAR
		if (IS_R300(dp->rd_softc)) {
			/*
			 * radeonfb_putchar() doesn't work right on some R3xx
			 * so we use software drawing here, the wrapper just
			 *  makes sure the engine is idle before scribbling
			 * into vram
			 */
			ri->ri_ops.putchar = radeonfb_putchar_wrapper;
		} else
#endif
			ri->ri_ops.putchar = radeonfb_putchar;
	} else {
		/* got an alpha font */
		switch(ri->ri_depth) {
			case 32:
				ri->ri_ops.putchar = radeonfb_putchar_aa32;
				break;
			case 8:
				ri->ri_ops.putchar = radeonfb_putchar_aa8;
				break;
			default:
				/* XXX this should never happen */
				panic("%s: depth is not 8 or 32 but we got an" \
					 " alpha font?!", __func__);
		}
	}
	ri->ri_ops.cursor = radeonfb_cursor;
}

void
radeonfb_set_fbloc(struct radeonfb_softc *sc)
{
	uint32_t	gen, ext, gen2 = 0;
	uint32_t	agploc, aperbase, apersize, mcfbloc;

	gen = GET32(sc, RADEON_CRTC_GEN_CNTL);
	/* XXX */
	ext = GET32(sc, RADEON_CRTC_EXT_CNTL) & ~RADEON_CRTC_DISPLAY_DIS;
	agploc = GET32(sc, RADEON_MC_AGP_LOCATION);
	aperbase = GET32(sc, RADEON_CONFIG_APER_0_BASE);
	apersize = GET32(sc, RADEON_CONFIG_APER_SIZE);

	PUT32(sc, RADEON_CRTC_GEN_CNTL, gen | RADEON_CRTC_DISP_REQ_EN_B);
	PUT32(sc, RADEON_CRTC_EXT_CNTL, ext | RADEON_CRTC_DISPLAY_DIS);
#if 0
	PUT32(sc, RADEON_CRTC_GEN_CNTL, gen | RADEON_CRTC_DISPLAY_DIS);
	PUT32(sc, RADEON_CRTC_EXT_CNTL, ext | RADEON_CRTC_DISP_REQ_EN_B);
#endif

	if (HAS_CRTC2(sc)) {
		gen2 = GET32(sc, RADEON_CRTC2_GEN_CNTL);
		PUT32(sc, RADEON_CRTC2_GEN_CNTL, 
		    gen2 | RADEON_CRTC2_DISP_REQ_EN_B);
	}

	delay(100000);

	mcfbloc = (aperbase >> 16) |
	    ((aperbase + (apersize - 1)) & 0xffff0000);

	sc->sc_aperbase = (mcfbloc & 0xffff) << 16;
	sc->sc_memsz = apersize;

	if (((agploc & 0xffff) << 16) !=
	    ((mcfbloc & 0xffff0000U) + 0x10000)) {
		agploc = mcfbloc & 0xffff0000U;
		agploc |= ((agploc + 0x10000) >> 16);
	}

	PUT32(sc, RADEON_HOST_PATH_CNTL, 0);

	PUT32(sc, RADEON_MC_FB_LOCATION, mcfbloc);
	PUT32(sc, RADEON_MC_AGP_LOCATION, agploc);

	DPRINTF(("aperbase = %u\n", aperbase));
	PRINTREG(RADEON_MC_FB_LOCATION);
	PRINTREG(RADEON_MC_AGP_LOCATION);

	PUT32(sc, RADEON_DISPLAY_BASE_ADDR, sc->sc_aperbase);

	if (HAS_CRTC2(sc))
		PUT32(sc, RADEON_DISPLAY2_BASE_ADDR, sc->sc_aperbase);

	PUT32(sc, RADEON_OV0_BASE_ADDR, sc->sc_aperbase);

#if 0
	/* XXX: what is this AGP garbage? :-) */
	PUT32(sc, RADEON_AGP_CNTL, 0x00100000);
#endif

	delay(100000);

	PUT32(sc, RADEON_CRTC_GEN_CNTL, gen);
	PUT32(sc, RADEON_CRTC_EXT_CNTL, ext);

	if (HAS_CRTC2(sc))
		PUT32(sc, RADEON_CRTC2_GEN_CNTL, gen2);
}

void
radeonfb_init_misc(struct radeonfb_softc *sc)
{
	PUT32(sc, RADEON_BUS_CNTL,
	    RADEON_BUS_MASTER_DIS |
	    RADEON_BUS_PREFETCH_MODE_ACT |
	    RADEON_BUS_PCI_READ_RETRY_EN |
	    RADEON_BUS_PCI_WRT_RETRY_EN |
	    (3 << RADEON_BUS_RETRY_WS_SHIFT) |
	    RADEON_BUS_MSTR_RD_MULT |
	    RADEON_BUS_MSTR_RD_LINE |
	    RADEON_BUS_RD_DISCARD_EN |
	    RADEON_BUS_MSTR_DISCONNECT_EN |
	    RADEON_BUS_READ_BURST);

	PUT32(sc, RADEON_BUS_CNTL1, 0xf0);
	/* PUT32(sc, RADEON_SEPROM_CNTL1, 0x09ff0000); */
	PUT32(sc, RADEON_FCP_CNTL, RADEON_FCP0_SRC_GND);
	PUT32(sc, RADEON_RBBM_CNTL,
	    (3 << RADEON_RB_SETTLE_SHIFT) |
	    (4 << RADEON_ABORTCLKS_HI_SHIFT) |
	    (4 << RADEON_ABORTCLKS_CP_SHIFT) |
	    (4 << RADEON_ABORTCLKS_CFIFO_SHIFT));

	/* XXX: figure out what these mean! */
	PUT32(sc, RADEON_AGP_CNTL, 0x00100000);
	PUT32(sc, RADEON_HOST_PATH_CNTL, 0);
#if 0
	PUT32(sc, RADEON_DISP_MISC_CNTL, 0x5bb00400);
#endif

	PUT32(sc, RADEON_GEN_INT_CNTL, 0);
	PUT32(sc, RADEON_GEN_INT_STATUS, GET32(sc, RADEON_GEN_INT_STATUS));
}

static void
radeonfb_putpal(struct radeonfb_display *dp, int idx, int r, int g, int b)
{
	struct radeonfb_softc *sc = dp->rd_softc;
	int		crtc, cc;
	uint32_t	vclk;

	vclk = GETPLL(sc, RADEON_VCLK_ECP_CNTL);
	PUTPLL(sc, RADEON_VCLK_ECP_CNTL, vclk & ~RADEON_PIXCLK_DAC_ALWAYS_ONb);

	/* initialize the palette for every CRTC used by this display */
	for (cc = 0; cc < dp->rd_ncrtcs; cc++) {
		crtc = dp->rd_crtcs[cc].rc_number;
		DPRINTF(("%s: doing crtc %d %d\n", __func__, cc, crtc));

		if (crtc)
			SET32(sc, RADEON_DAC_CNTL2, RADEON_DAC2_PALETTE_ACC_CTL);
		else
			CLR32(sc, RADEON_DAC_CNTL2, RADEON_DAC2_PALETTE_ACC_CTL);

		PUT32(sc, RADEON_PALETTE_INDEX, idx);
            	PUT32(sc, RADEON_PALETTE_30_DATA,
            	    (r << 22) | (g << 12) | (b << 2));
	}

	PUTPLL(sc, RADEON_VCLK_ECP_CNTL, vclk);
}

/*
 * This loads a linear color map for true color.
 */
void
radeonfb_init_palette(struct radeonfb_display *dp)
{
	int		i;

#define	DAC_WIDTH ((1 << 10) - 1)
#define	CLUT_WIDTH ((1 << 8) - 1)
#define	CLUT_COLOR(i)      ((i * DAC_WIDTH * 2 / CLUT_WIDTH + 1) / 2)

	if (dp->rd_bpp == 8) {

		/* R3G3B2 palette */
		uint32_t tmp, r, g, b;
 
	        for (i = 0; i <= CLUT_WIDTH; ++i) {
			tmp = i & 0xe0;

			/*
			 * replicate bits so 0xe0 maps to a red value of 0xff
			 * in order to make white look actually white
			 */
			tmp |= (tmp >> 3) | (tmp >> 6);
			r = tmp;

			tmp = (i & 0x1c) << 3;
			tmp |= (tmp >> 3) | (tmp >> 6);
			g = tmp;

			tmp = (i & 0x03) << 6;
			tmp |= tmp >> 2;
			tmp |= tmp >> 4;
			b = tmp;

			radeonfb_putpal(dp, i, r, g, b);
		}
	} else {
		/* linear ramp */
		for (i = 0; i <= CLUT_WIDTH; ++i) {
			radeonfb_putpal(dp, i, i, i, i);
		}
	}
}

static int
radeonfb_putcmap(struct radeonfb_display *dp, struct wsdisplay_cmap *cm)
{
	u_char *r, *g, *b;
	u_int index = cm->index;
	u_int count = cm->count;
	int i, error;
	u_char rbuf[256], gbuf[256], bbuf[256];

#ifdef GENFB_DEBUG
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

	memcpy(&dp->rd_cmap_red[index], &rbuf[index], count);
	memcpy(&dp->rd_cmap_green[index], &gbuf[index], count);
	memcpy(&dp->rd_cmap_blue[index], &bbuf[index], count);

	r = &dp->rd_cmap_red[index];
	g = &dp->rd_cmap_green[index];
	b = &dp->rd_cmap_blue[index];

	for (i = 0; i < count; i++) {
		radeonfb_putpal(dp, index, *r, *g, *b);
		index++;
		r++, g++, b++;
	}
	return 0;
}

static int
radeonfb_getcmap(struct radeonfb_display *dp, struct wsdisplay_cmap *cm)
{
	u_int index = cm->index;
	u_int count = cm->count;
	int error;

	if (index >= 255 || count > 256 || index + count > 256)
		return EINVAL;

	error = copyout(&dp->rd_cmap_red[index],   cm->red,   count);
	if (error)
		return error;
	error = copyout(&dp->rd_cmap_green[index], cm->green, count);
	if (error)
		return error;
	error = copyout(&dp->rd_cmap_blue[index],  cm->blue,  count);
	if (error)
		return error;

	return 0;
}

/*
 * Bugs in some R300 hardware requires this when accessing CLOCK_CNTL_INDEX.
 */
void
radeonfb_r300cg_workaround(struct radeonfb_softc *sc)
{
	uint32_t	tmp, save;

	save = GET32(sc, RADEON_CLOCK_CNTL_INDEX);
	tmp = save & ~(0x3f | RADEON_PLL_WR_EN);
	PUT32(sc, RADEON_CLOCK_CNTL_INDEX, tmp);
	tmp = GET32(sc, RADEON_CLOCK_CNTL_DATA);
	PUT32(sc, RADEON_CLOCK_CNTL_INDEX, save);
}

/*
 * Acceleration entry points.
 */

/* this one draws characters using bitmap fonts */
static void
radeonfb_putchar(void *cookie, int row, int col, u_int c, long attr)
{
	struct rasops_info	*ri = cookie;
	struct vcons_screen	*scr = ri->ri_hw;
	struct radeonfb_display	*dp = scr->scr_cookie;
	struct radeonfb_softc	*sc = dp->rd_softc;
	struct wsdisplay_font	*font = PICK_FONT(ri, c);
	uint32_t		w, h;
	int			xd, yd, offset, i;
	uint32_t		bg, fg, gmc;
	uint32_t		reg;
	uint8_t			*data8;
	uint16_t		*data16;
	void			*data;

	if (dp->rd_wsmode != WSDISPLAYIO_MODE_EMUL)
		return;

	if (!CHAR_IN_FONT(c, font))
		return;

	w = font->fontwidth;
	h = font->fontheight;

	bg = ri->ri_devcmap[(attr >> 16) & 0xf];
	fg = ri->ri_devcmap[(attr >> 24) & 0xf];

	xd = ri->ri_xorigin + col * w;
	yd = ri->ri_yorigin + row * h;

	if (c == 0x20) {
		radeonfb_rectfill(dp, xd, yd, w, h, bg);
		return;
	}
	data = WSFONT_GLYPH(c, font);

	gmc = dp->rd_format << RADEON_GMC_DST_DATATYPE_SHIFT;

	radeonfb_wait_fifo(sc, 9);
	
	PUT32(sc, RADEON_DP_GUI_MASTER_CNTL,
	    RADEON_GMC_BRUSH_NONE |
	    RADEON_GMC_SRC_DATATYPE_MONO_FG_BG |
	    RADEON_GMC_DST_CLIPPING |
	    RADEON_ROP3_S |
	    RADEON_DP_SRC_SOURCE_HOST_DATA |
	    RADEON_GMC_CLR_CMP_CNTL_DIS |
	    RADEON_GMC_WR_MSK_DIS |
	    gmc);

	PUT32(sc, RADEON_SC_LEFT, xd);
	PUT32(sc, RADEON_SC_RIGHT, xd + w);
	PUT32(sc, RADEON_DP_SRC_FRGD_CLR, fg);
	PUT32(sc, RADEON_DP_SRC_BKGD_CLR, bg);
	PUT32(sc, RADEON_DP_CNTL,
	    RADEON_DST_X_LEFT_TO_RIGHT |
	    RADEON_DST_Y_TOP_TO_BOTTOM);

	PUT32(sc, RADEON_SRC_X_Y, 0);
	offset = 32 - (font->stride << 3);
	PUT32(sc, RADEON_DST_X_Y, ((xd - offset) << 16) | yd);
	PUT32(sc, RADEON_DST_WIDTH_HEIGHT, (32 << 16) | h);

	radeonfb_wait_fifo(sc, h);
	switch (font->stride) {
		case 1: {
			data8 = data;
			for (i = 0; i < h; i++) {
				reg = *data8;
#if BYTE_ORDER == LITTLE_ENDIAN
				reg = reg << 24;
#endif
				bus_space_write_stream_4(sc->sc_regt, 
				    sc->sc_regh, RADEON_HOST_DATA0, reg);
				data8++;
			}
			break;
		}
		case 2: {
			data16 = data;
			for (i = 0; i < h; i++) {
				reg = *data16;
#if BYTE_ORDER == LITTLE_ENDIAN
				reg = reg << 16;
#endif
				bus_space_write_stream_4(sc->sc_regt, 
				    sc->sc_regh, RADEON_HOST_DATA0, reg);
				data16++;
			}
			break;
		}
	}
	if (attr & 1)
		radeonfb_rectfill(dp, xd, yd + h - 2, w, 1, fg);
}

/* ... while this one is for anti-aliased ones */
static void
radeonfb_putchar_aa32(void *cookie, int row, int col, u_int c, long attr)
{
	struct rasops_info	*ri = cookie;
	struct vcons_screen	*scr = ri->ri_hw;
	struct radeonfb_display	*dp = scr->scr_cookie;
	struct radeonfb_softc	*sc = dp->rd_softc;
	struct wsdisplay_font	*font = PICK_FONT(ri, c);
	uint32_t		bg, fg, gmc;
	uint8_t			*data;
	int			w, h, xd, yd;
	int 			i, r, g, b, aval;
	int 			rf, gf, bf, rb, gb, bb;
	uint32_t 		pixel;
	int rv;

	if (dp->rd_wsmode != WSDISPLAYIO_MODE_EMUL)
		return;

	if (!CHAR_IN_FONT(c, font))
		return;

	w = font->fontwidth;
	h = font->fontheight;

	bg = ri->ri_devcmap[(attr >> 16) & 0xf];
	fg = ri->ri_devcmap[(attr >> 24) & 0xf];

	xd = ri->ri_xorigin + col * w;
	yd = ri->ri_yorigin + row * h;

	if (c == 0x20) {
		radeonfb_rectfill(dp, xd, yd, w, h, bg);
		if (attr & 1)
			radeonfb_rectfill(dp, xd, yd + h - 2, w, 1, fg);
		return;
	}
	rv = glyphcache_try(&dp->rd_gc, c, xd, yd, attr);
	if (rv == GC_OK)
		return;

	data = WSFONT_GLYPH(c, font);

	gmc = dp->rd_format << RADEON_GMC_DST_DATATYPE_SHIFT;

	radeonfb_wait_fifo(sc, 5);
	
	PUT32(sc, RADEON_DP_GUI_MASTER_CNTL,
	    RADEON_GMC_BRUSH_NONE |
	    RADEON_GMC_SRC_DATATYPE_COLOR |
	    RADEON_ROP3_S |
	    RADEON_DP_SRC_SOURCE_HOST_DATA |
	    RADEON_GMC_CLR_CMP_CNTL_DIS |
	    RADEON_GMC_WR_MSK_DIS |
	    gmc);

	PUT32(sc, RADEON_DP_CNTL,
	    RADEON_DST_X_LEFT_TO_RIGHT |
	    RADEON_DST_Y_TOP_TO_BOTTOM);

	PUT32(sc, RADEON_SRC_X_Y, 0);
	PUT32(sc, RADEON_DST_X_Y, (xd << 16) | yd);
	PUT32(sc, RADEON_DST_WIDTH_HEIGHT, (w << 16) | h);

	rf = (fg >> 16) & 0xff;
	rb = (bg >> 16) & 0xff;
	gf = (fg >> 8) & 0xff;
	gb = (bg >> 8) & 0xff;
	bf =  fg & 0xff;
	bb =  bg & 0xff;

	/*
	 * I doubt we can upload data faster than even the slowest Radeon
	 * could process them, especially when doing the alpha blending stuff
	 * along the way, so just make sure there's some room in the FIFO and
	 * then hammer away
	 * As it turns out we can, so make periodic stops to let the FIFO
	 * drain.
	 */
	radeonfb_wait_fifo(sc, 20);
	for (i = 0; i < ri->ri_fontscale; i++) {
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
		if (i & 16)
			radeonfb_wait_fifo(sc, 20);
		PUT32(sc, RADEON_HOST_DATA0, pixel);
	}
	if (rv == GC_ADD) {
		glyphcache_add(&dp->rd_gc, c, xd, yd);
	} else
		if (attr & 1)
			radeonfb_rectfill(dp, xd, yd + h - 2, w, 1, fg);

}

static void
radeonfb_putchar_aa8(void *cookie, int row, int col, u_int c, long attr)
{
	struct rasops_info	*ri = cookie;
	struct vcons_screen	*scr = ri->ri_hw;
	struct radeonfb_display	*dp = scr->scr_cookie;
	struct radeonfb_softc	*sc = dp->rd_softc;
	struct wsdisplay_font	*font = PICK_FONT(ri, c);
	uint32_t bg, fg, latch = 0, bg8, fg8, pixel, gmc;
	int i, x, y, wi, he, r, g, b, aval;
	int r1, g1, b1, r0, g0, b0, fgo, bgo;
	uint8_t *data8;
	int rv, cnt;

	if (dp->rd_wsmode != WSDISPLAYIO_MODE_EMUL)
		return;

	if (!CHAR_IN_FONT(c, font))
		return;

	wi = font->fontwidth;
	he = font->fontheight;

	bg = ri->ri_devcmap[(attr >> 16) & 0xf];
	fg = ri->ri_devcmap[(attr >> 24) & 0xf];

	x = ri->ri_xorigin + col * wi;
	y = ri->ri_yorigin + row * he;

	if (c == 0x20) {
		radeonfb_rectfill(dp, x, y, wi, he, bg);
		if (attr & 1)
			radeonfb_rectfill(dp, x, y + he - 2, wi, 1, fg);
		return;
	}
	rv = glyphcache_try(&dp->rd_gc, c, x, y, attr);
	if (rv == GC_OK)
		return;

	data8 = WSFONT_GLYPH(c, font);

	gmc = dp->rd_format << RADEON_GMC_DST_DATATYPE_SHIFT;

	radeonfb_wait_fifo(sc, 5);
	
	PUT32(sc, RADEON_DP_GUI_MASTER_CNTL,
	    RADEON_GMC_BRUSH_NONE |
	    RADEON_GMC_SRC_DATATYPE_COLOR |
	    RADEON_ROP3_S |
	    RADEON_DP_SRC_SOURCE_HOST_DATA |
	    RADEON_GMC_CLR_CMP_CNTL_DIS |
	    RADEON_GMC_WR_MSK_DIS |
	    gmc);

	PUT32(sc, RADEON_DP_CNTL,
	    RADEON_DST_X_LEFT_TO_RIGHT |
	    RADEON_DST_Y_TOP_TO_BOTTOM);

	PUT32(sc, RADEON_SRC_X_Y, 0);
	PUT32(sc, RADEON_DST_X_Y, (x << 16) | y);
	PUT32(sc, RADEON_DST_WIDTH_HEIGHT, (wi << 16) | he);

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

	radeonfb_wait_fifo(sc, 20);
	cnt = 0;
	for (i = 0; i < ri->ri_fontscale; i++) {
		aval = *data8;
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
		latch |= pixel << (8 * (i & 3));
		/* write in 32bit chunks */
		if ((i & 3) == 3) {
			PUT32(sc, RADEON_HOST_DATA0, latch);
			/*
			 * not strictly necessary, old data should be shifted 
			 * out 
			 */
			latch = 0;
			cnt++;
			if (cnt > 16) {
				cnt = 0;
				radeonfb_wait_fifo(sc, 20);
			}
		}
		data8++;
	}
	/* if we have pixels left in latch write them out */
	if ((i & 3) != 0) {
		/*
		 * radeon is weird - apparently leftover pixels are written
		 * from the middle, not from the left as everything else
		 */
		PUT32(sc, RADEON_HOST_DATA0, latch);
	}

	if (rv == GC_ADD) {
		glyphcache_add(&dp->rd_gc, c, x, y);
	} else
		if (attr & 1)
			radeonfb_rectfill(dp, x, y + he - 2, wi, 1, fg);

}

/*
 * wrapper for software character drawing
 * just sync the engine and call rasops*_putchar()
 */

#ifndef RADEONFB_ALWAYS_ACCEL_PUTCHAR
static void
radeonfb_putchar_wrapper(void *cookie, int row, int col, u_int c, long attr)
{
	struct rasops_info	*ri = cookie;
	struct vcons_screen	*scr = ri->ri_hw;
	struct radeonfb_display	*dp = scr->scr_cookie;

	radeonfb_engine_idle(dp->rd_softc);
	dp->rd_putchar(ri, row, col, c, attr);
}
#endif
	
static void
radeonfb_eraserows(void *cookie, int row, int nrows, long fillattr)
{
	struct rasops_info	*ri = cookie;
	struct vcons_screen	*scr = ri->ri_hw;
	struct radeonfb_display	*dp = scr->scr_cookie;
	uint32_t		x, y, w, h, fg, bg, ul;

	/* XXX: check for full emulation mode? */
	if (dp->rd_wsmode == WSDISPLAYIO_MODE_EMUL) {
		x = ri->ri_xorigin;
		y = ri->ri_yorigin + ri->ri_font->fontheight * row;
		w = ri->ri_emuwidth;
		h = ri->ri_font->fontheight * nrows;

		rasops_unpack_attr(fillattr, &fg, &bg, &ul);
		radeonfb_rectfill(dp, x, y, w, h, ri->ri_devcmap[bg & 0xf]);
	}
}

static void
radeonfb_copyrows(void *cookie, int srcrow, int dstrow, int nrows)
{
	struct rasops_info	*ri = cookie;
	struct vcons_screen	*scr = ri->ri_hw;
	struct radeonfb_display	*dp = scr->scr_cookie;
	uint32_t		x, ys, yd, w, h;

	if (dp->rd_wsmode == WSDISPLAYIO_MODE_EMUL) {
		x = ri->ri_xorigin;
		ys = ri->ri_yorigin + ri->ri_font->fontheight * srcrow;
		yd = ri->ri_yorigin + ri->ri_font->fontheight * dstrow;
		w = ri->ri_emuwidth;
		h = ri->ri_font->fontheight * nrows;
		radeonfb_bitblt(dp, x, ys, x, yd, w, h,
		    RADEON_ROP3_S);
	}
}

static void
radeonfb_copycols(void *cookie, int row, int srccol, int dstcol, int ncols)
{
	struct rasops_info	*ri = cookie;
	struct vcons_screen	*scr = ri->ri_hw;
	struct radeonfb_display	*dp = scr->scr_cookie;
	uint32_t		xs, xd, y, w, h;

	if (dp->rd_wsmode == WSDISPLAYIO_MODE_EMUL) {
		xs = ri->ri_xorigin + ri->ri_font->fontwidth * srccol;
		xd = ri->ri_xorigin + ri->ri_font->fontwidth * dstcol;
		y = ri->ri_yorigin + ri->ri_font->fontheight * row;
		w = ri->ri_font->fontwidth * ncols;
		h = ri->ri_font->fontheight;
		radeonfb_bitblt(dp, xs, y, xd, y, w, h,
		    RADEON_ROP3_S);
	}
}

static void
radeonfb_erasecols(void *cookie, int row, int startcol, int ncols,
    long fillattr)
{
	struct rasops_info	*ri = cookie;
	struct vcons_screen	*scr = ri->ri_hw;
	struct radeonfb_display	*dp = scr->scr_cookie;
	uint32_t		x, y, w, h, fg, bg, ul;

	if (dp->rd_wsmode == WSDISPLAYIO_MODE_EMUL) {
		x = ri->ri_xorigin + ri->ri_font->fontwidth * startcol;
		y = ri->ri_yorigin + ri->ri_font->fontheight * row;
		w = ri->ri_font->fontwidth * ncols;
		h = ri->ri_font->fontheight;

		rasops_unpack_attr(fillattr, &fg, &bg, &ul);
		radeonfb_rectfill(dp, x, y, w, h, ri->ri_devcmap[bg & 0xf]);
	}
}

static void
radeonfb_cursor(void *cookie, int on, int row, int col)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;
	struct radeonfb_display	*dp = scr->scr_cookie;
	int x, y, wi, he;
	
	wi = ri->ri_font->fontwidth;
	he = ri->ri_font->fontheight;
	
	if (dp->rd_wsmode == WSDISPLAYIO_MODE_EMUL) {
		x = ri->ri_ccol * wi + ri->ri_xorigin;
		y = ri->ri_crow * he + ri->ri_yorigin;
		/* first turn off the old cursor */
		if (ri->ri_flg & RI_CURSOR) {
			radeonfb_bitblt(dp, x, y, x, y, wi, he,
			    RADEON_ROP3_Dn);
			ri->ri_flg &= ~RI_CURSOR;
		}
		ri->ri_crow = row;
		ri->ri_ccol = col;
		/* then (possibly) turn on the new one */
		if (on) {
			x = ri->ri_ccol * wi + ri->ri_xorigin;
			y = ri->ri_crow * he + ri->ri_yorigin;
			radeonfb_bitblt(dp, x, y, x, y, wi, he,
			    RADEON_ROP3_Dn);
			ri->ri_flg |= RI_CURSOR;
		}
	} else {
		scr->scr_ri.ri_crow = row;
		scr->scr_ri.ri_ccol = col;
		scr->scr_ri.ri_flg &= ~RI_CURSOR;
	}
}

/*
 * Underlying acceleration support.
 */

static void
radeonfb_rectfill(struct radeonfb_display *dp, int dstx, int dsty,
    int width, int height, uint32_t color)
{
	struct radeonfb_softc	*sc = dp->rd_softc;
	uint32_t		gmc;

	gmc = dp->rd_format << RADEON_GMC_DST_DATATYPE_SHIFT;

	radeonfb_wait_fifo(sc, 6);

	PUT32(sc, RADEON_DP_GUI_MASTER_CNTL,
	    RADEON_GMC_BRUSH_SOLID_COLOR |
	    RADEON_GMC_SRC_DATATYPE_COLOR |
	    RADEON_GMC_CLR_CMP_CNTL_DIS |
	    RADEON_ROP3_P | gmc);

	PUT32(sc, RADEON_DP_BRUSH_FRGD_CLR, color);
	PUT32(sc, RADEON_DP_WRITE_MASK, 0xffffffff);
	PUT32(sc, RADEON_DP_CNTL,
	    RADEON_DST_X_LEFT_TO_RIGHT |
	    RADEON_DST_Y_TOP_TO_BOTTOM);
	PUT32(sc, RADEON_DST_Y_X, (dsty << 16) | dstx);
	PUT32(sc, RADEON_DST_WIDTH_HEIGHT, (width << 16) | (height));

}

static void
radeonfb_rectfill_a(void *cookie, int dstx, int dsty,
    int width, int height, long attr)
{
	struct radeonfb_display *dp = cookie;

	radeonfb_rectfill(dp, dstx, dsty, width, height,
	    dp->rd_vscreen.scr_ri.ri_devcmap[(attr >> 24 & 0xf)]);
}

static void
radeonfb_bitblt(void *cookie, int srcx, int srcy,
    int dstx, int dsty, int width, int height, int rop)
{
	struct radeonfb_display *dp = cookie;
	struct radeonfb_softc	*sc = dp->rd_softc;
	uint32_t		gmc;
	uint32_t		dir;

	if (dsty < srcy) {
		dir = RADEON_DST_Y_TOP_TO_BOTTOM;
	} else {
		srcy += height - 1;
		dsty += height - 1;
		dir = 0;
	}
	if (dstx < srcx) {
		dir |= RADEON_DST_X_LEFT_TO_RIGHT;
	} else {
		srcx += width - 1;
		dstx += width - 1;
	}

	gmc = dp->rd_format << RADEON_GMC_DST_DATATYPE_SHIFT;
	
	radeonfb_wait_fifo(sc, 6);

	PUT32(sc, RADEON_DP_GUI_MASTER_CNTL,
	    RADEON_GMC_BRUSH_SOLID_COLOR |
	    RADEON_GMC_SRC_DATATYPE_COLOR |
	    RADEON_GMC_CLR_CMP_CNTL_DIS |
	    RADEON_DP_SRC_SOURCE_MEMORY |
	    rop | gmc);

	PUT32(sc, RADEON_DP_WRITE_MASK, 0xffffffff);
	PUT32(sc, RADEON_DP_CNTL, dir);
	PUT32(sc, RADEON_SRC_Y_X, (srcy << 16) | srcx);
	PUT32(sc, RADEON_DST_Y_X, (dsty << 16) | dstx);
	PUT32(sc, RADEON_DST_WIDTH_HEIGHT, (width << 16) | (height));
}

static void
radeonfb_engine_idle(struct radeonfb_softc *sc)
{

	radeonfb_wait_fifo(sc, 64);
	while ((GET32(sc, RADEON_RBBM_STATUS) &
			RADEON_RBBM_ACTIVE) != 0);
	radeonfb_engine_flush(sc);
}

static inline void
radeonfb_wait_fifo(struct radeonfb_softc *sc, int n)
{
	int	i;

	for (i = RADEON_TIMEOUT; i; i--) {
		if ((GET32(sc, RADEON_RBBM_STATUS) &
			RADEON_RBBM_FIFOCNT_MASK) >= n)
			return;
	}
#ifdef	DIAGNOSTIC
	if (!i)
		printf("%s: timed out waiting for fifo (%x)\n",
		    XNAME(sc), GET32(sc, RADEON_RBBM_STATUS));
#endif
}

static void
radeonfb_engine_flush(struct radeonfb_softc *sc)
{
	int	i = 0;

	if (IS_R300(sc)) {
		SET32(sc, R300_DSTCACHE_CTLSTAT, R300_RB2D_DC_FLUSH_ALL);
		while (GET32(sc, R300_DSTCACHE_CTLSTAT) & R300_RB2D_DC_BUSY) {
			i++;
		}
	} else {
		SET32(sc, RADEON_RB2D_DSTCACHE_CTLSTAT, 
		    RADEON_RB2D_DC_FLUSH_ALL);
		while (GET32(sc, RADEON_RB2D_DSTCACHE_CTLSTAT) &
			RADEON_RB2D_DC_BUSY) {
			i++;
		}
	}
#ifdef DIAGNOSTIC
	if (i > RADEON_TIMEOUT)
		printf("%s: engine flush timed out!\n", XNAME(sc));
#endif
}

static inline void
radeonfb_unclip(struct radeonfb_softc *sc)
{

	radeonfb_wait_fifo(sc, 2);
	PUT32(sc, RADEON_SC_TOP_LEFT, 0);
	PUT32(sc, RADEON_SC_BOTTOM_RIGHT, 0x1fff1fff);
}

static void
radeonfb_engine_init(struct radeonfb_display *dp)
{
	struct radeonfb_softc	*sc = dp->rd_softc;
	uint32_t		pitch;

	/* no 3D */
	PUT32(sc, RADEON_RB3D_CNTL, 0);

	radeonfb_engine_reset(sc);
	pitch = ((dp->rd_virtx * (dp->rd_bpp / 8) + 0x3f)) >> 6;

	radeonfb_wait_fifo(sc, 1);
	if (!IS_R300(sc))
		PUT32(sc, RADEON_RB2D_DSTCACHE_MODE, 0);

	radeonfb_wait_fifo(sc, 3);
	PUT32(sc, RADEON_DEFAULT_PITCH_OFFSET,
	    (pitch << 22) | (sc->sc_aperbase >> 10));


	PUT32(sc, RADEON_DST_PITCH_OFFSET,
	    (pitch << 22) | (sc->sc_aperbase >> 10));
	PUT32(sc, RADEON_SRC_PITCH_OFFSET,
	    (pitch << 22) | (sc->sc_aperbase >> 10));

	(void)GET32(sc, RADEON_DP_DATATYPE);

	/* default scissors -- no clipping */
	radeonfb_wait_fifo(sc, 1);
	PUT32(sc, RADEON_DEFAULT_SC_BOTTOM_RIGHT,
	    RADEON_DEFAULT_SC_RIGHT_MAX | RADEON_DEFAULT_SC_BOTTOM_MAX);

	radeonfb_wait_fifo(sc, 1);
	PUT32(sc, RADEON_DP_GUI_MASTER_CNTL,
	    (dp->rd_format << RADEON_GMC_DST_DATATYPE_SHIFT) |
	    RADEON_GMC_CLR_CMP_CNTL_DIS |
	    RADEON_GMC_BRUSH_SOLID_COLOR |
	    RADEON_GMC_SRC_DATATYPE_COLOR);

	radeonfb_wait_fifo(sc, 10);
	PUT32(sc, RADEON_DST_LINE_START, 0);
	PUT32(sc, RADEON_DST_LINE_END, 0);
	PUT32(sc, RADEON_DP_BRUSH_FRGD_CLR, 0xffffffff);
	PUT32(sc, RADEON_DP_BRUSH_BKGD_CLR, 0);
	PUT32(sc, RADEON_DP_SRC_FRGD_CLR, 0xffffffff);
	PUT32(sc, RADEON_DP_SRC_BKGD_CLR, 0);
	PUT32(sc, RADEON_DP_WRITE_MASK, 0xffffffff);
	PUT32(sc, RADEON_SC_TOP_LEFT, 0);
	PUT32(sc, RADEON_SC_BOTTOM_RIGHT, 0x1fff1fff);
	PUT32(sc, RADEON_AUX_SC_CNTL, 0);
	radeonfb_engine_idle(sc);
}

static void
radeonfb_engine_reset(struct radeonfb_softc *sc)
{
	uint32_t	hpc, rbbm, mclkcntl, clkindex;

	radeonfb_engine_flush(sc);

	clkindex = GET32(sc, RADEON_CLOCK_CNTL_INDEX);
	if (HAS_R300CG(sc))
		radeonfb_r300cg_workaround(sc);
	mclkcntl = GETPLL(sc, RADEON_MCLK_CNTL);

	/*
	 * According to comments in XFree code, resetting the HDP via
	 * the RBBM_SOFT_RESET can cause bad behavior on some systems.
	 * So we use HOST_PATH_CNTL instead.
	 */

	hpc = GET32(sc, RADEON_HOST_PATH_CNTL);
	rbbm = GET32(sc, RADEON_RBBM_SOFT_RESET);
	if (IS_R300(sc)) {
		PUT32(sc, RADEON_RBBM_SOFT_RESET, rbbm |
		    RADEON_SOFT_RESET_CP |
		    RADEON_SOFT_RESET_HI |
		    RADEON_SOFT_RESET_E2);
		GET32(sc, RADEON_RBBM_SOFT_RESET);
		PUT32(sc, RADEON_RBBM_SOFT_RESET, 0);
		/*
		 * XXX: this bit is not defined in any ATI docs I have,
		 * nor in the XFree code, but XFree does it.  Why?
		 */
		SET32(sc, RADEON_RB2D_DSTCACHE_MODE, (1<<17));
	} else {
		PUT32(sc, RADEON_RBBM_SOFT_RESET, rbbm |
		    RADEON_SOFT_RESET_CP |
		    RADEON_SOFT_RESET_SE |
		    RADEON_SOFT_RESET_RE |
		    RADEON_SOFT_RESET_PP |
		    RADEON_SOFT_RESET_E2 |
		    RADEON_SOFT_RESET_RB);
		GET32(sc, RADEON_RBBM_SOFT_RESET);
		PUT32(sc, RADEON_RBBM_SOFT_RESET, rbbm &
		    ~(RADEON_SOFT_RESET_CP |
			RADEON_SOFT_RESET_SE |
			RADEON_SOFT_RESET_RE |
			RADEON_SOFT_RESET_PP |
			RADEON_SOFT_RESET_E2 |
			RADEON_SOFT_RESET_RB));
		GET32(sc, RADEON_RBBM_SOFT_RESET);
	}

	PUT32(sc, RADEON_HOST_PATH_CNTL, hpc | RADEON_HDP_SOFT_RESET);
	GET32(sc, RADEON_HOST_PATH_CNTL);
	PUT32(sc, RADEON_HOST_PATH_CNTL, hpc);

	if (IS_R300(sc))
		PUT32(sc, RADEON_RBBM_SOFT_RESET, rbbm);

	PUT32(sc, RADEON_CLOCK_CNTL_INDEX, clkindex);
	PRINTREG(RADEON_CLOCK_CNTL_INDEX);
	PUTPLL(sc, RADEON_MCLK_CNTL, mclkcntl);

	if (HAS_R300CG(sc))
		radeonfb_r300cg_workaround(sc);
}

static int
radeonfb_set_curpos(struct radeonfb_display *dp, struct wsdisplay_curpos *pos)
{
	int		x, y;

	x = pos->x;
	y = pos->y;

	/*
	 * This doesn't let a cursor move off the screen.  I'm not
	 * sure if this will have negative effects for e.g. Xinerama.
	 * I'd guess Xinerama handles it by changing the cursor shape,
	 * but that needs verification.
	 */
	if (x >= dp->rd_virtx)
		x = dp->rd_virtx - 1;
	if (x < 0)
		x = 0;
	if (y >= dp->rd_virty)
		y = dp->rd_virty - 1;
	if (y < 0)
		y = 0;

	dp->rd_cursor.rc_pos.x = x;
	dp->rd_cursor.rc_pos.y = y;

	radeonfb_cursor_position(dp);
	return 0;
}

static int
radeonfb_set_cursor(struct radeonfb_display *dp, struct wsdisplay_cursor *wc)
{
	unsigned	flags;

	uint8_t		r[2], g[2], b[2];
	unsigned	index, count;
	int		i, err;
	int		pitch, size;
	struct radeonfb_cursor	nc;

	flags = wc->which;

	/* copy old values */
	nc = dp->rd_cursor;

	if (flags & WSDISPLAY_CURSOR_DOCMAP) {
		index = wc->cmap.index;
		count = wc->cmap.count;
		
		if (index >= 2 || (index + count) > 2)
			return EINVAL;

		err = copyin(wc->cmap.red, &r[index], count);
		if (err)
			return err;
		err = copyin(wc->cmap.green, &g[index], count);
		if (err)
			return err;
		err = copyin(wc->cmap.blue, &b[index], count);
		if (err)
			return err;

		for (i = index; i < index + count; i++) {
			nc.rc_cmap[i] =
			    (r[i] << 16) + (g[i] << 8) + (b[i] << 0);
		}
	}

	if (flags & WSDISPLAY_CURSOR_DOSHAPE) {
		if ((wc->size.x > RADEON_CURSORMAXX) ||
		    (wc->size.y > RADEON_CURSORMAXY))
			return EINVAL;

		/* figure bytes per line */
		pitch = (wc->size.x + 7) / 8;
		size = pitch * wc->size.y;

		/* clear the old cursor and mask */
		memset(nc.rc_image, 0, 512);
		memset(nc.rc_mask, 0, 512);

		nc.rc_size = wc->size;

		if ((err = copyin(wc->image, nc.rc_image, size)) != 0)
			return err;

		if ((err = copyin(wc->mask, nc.rc_mask, size)) != 0)
			return err;
	}

	if (flags & WSDISPLAY_CURSOR_DOHOT) {
		nc.rc_hot = wc->hot;
		if (nc.rc_hot.x >= nc.rc_size.x)
			nc.rc_hot.x = nc.rc_size.x - 1;
		if (nc.rc_hot.y >= nc.rc_size.y)
			nc.rc_hot.y = nc.rc_size.y - 1;
	}

	if (flags & WSDISPLAY_CURSOR_DOPOS) {
		nc.rc_pos = wc->pos;
		if (nc.rc_pos.x >= dp->rd_virtx)
			nc.rc_pos.x = dp->rd_virtx - 1;
#if 0
		if (nc.rc_pos.x < 0)
			nc.rc_pos.x = 0;
#endif
		if (nc.rc_pos.y >= dp->rd_virty)
			nc.rc_pos.y = dp->rd_virty - 1;
#if 0
		if (nc.rc_pos.y < 0)
			nc.rc_pos.y = 0;
#endif
	}
	if (flags & WSDISPLAY_CURSOR_DOCUR) {
		nc.rc_visible = wc->enable;
	}

	dp->rd_cursor = nc;
	radeonfb_cursor_update(dp, wc->which);

	return 0;
}

static uint8_t
radeonfb_backwards(uint8_t d)
{
	uint8_t l;

	l = d << 7;
	l |= ((d & 0x02) << 5); 
	l |= ((d & 0x04) << 3); 
	l |= ((d & 0x08) << 1);
	l |= ((d & 0x10) >> 1); 
	l |= ((d & 0x20) >> 3); 
	l |= ((d & 0x40) >> 5); 
	l |= ((d & 0x80) >> 7);
	return l;
} 
 
/*
 * Change the cursor shape.  Call this with the cursor locked to avoid
 * flickering/tearing.
 */
static void
radeonfb_cursor_shape(struct radeonfb_display *dp)
{
	uint8_t	and[512], xor[512];
	int	i, j, src, dst /* , pitch */;
	const uint8_t	*msk = dp->rd_cursor.rc_mask;
	const uint8_t	*img = dp->rd_cursor.rc_image;

	/*
	 * Radeon cursor data interleaves one line of AND data followed
	 * by a line of XOR data.  (Each line corresponds to a whole hardware
	 * pitch - i.e. 64 pixels or 8 bytes.)
	 *
	 * The cursor is displayed using the following table:
	 *
	 * AND	XOR	Result
	 * ----------------------
	 *  0    0	Cursor color 0
	 *  0	 1	Cursor color 1
	 *  1	 0	Transparent
	 *  1	 1	Complement of background
	 *
	 * Our masks are therefore different from what we were passed.
	 * Passed in, I'm assuming the data represents either color 0 or 1,
	 * and a mask, so the passed in table looks like:
	 *
	 * IMG	Mask	Result
	 * -----------------------
	 *  0	 0	Transparent
	 *  0	 1	Cursor color 0
	 *  1	 0	Transparent
	 *  1	 1	Cursor color 1
	 *
	 * IF mask bit == 1, AND = 0, XOR = color.
	 * IF mask bit == 0, AND = 1, XOR = 0.
	 *
	 * hence:	AND = ~(mask);	XOR = color & ~(mask);
	 */

	/* pitch = ((dp->rd_cursor.rc_size.x + 7) / 8); */

	/* start by assuming all bits are transparent */
	memset(and, 0xff, 512);
	memset(xor, 0x00, 512);

	src = 0;
	dst = 0;
	for (i = 0; i < 64; i++) {
		for (j = 0; j < 64; j += 8) {
			if ((i < dp->rd_cursor.rc_size.y) &&
			    (j < dp->rd_cursor.rc_size.x)) {

				/* take care to leave odd bits alone */
				and[dst] &= ~(msk[src]);
				xor[dst] = img[src] & msk[src];
				src++;
			}
			dst++;
		}
	}

	for (i = 0; i < 512; i++) {
		and[i] = radeonfb_backwards(and[i]);
		xor[i] = radeonfb_backwards(xor[i]);
	}

	/* copy the image into place */
	for (i = 0; i < 64; i++) {
		memcpy((uint8_t *)dp->rd_curptr + (i * 16),
		    &and[i * 8], 8);
		memcpy((uint8_t *)dp->rd_curptr + (i * 16) + 8,
		    &xor[i * 8], 8);
	}
}

static void
radeonfb_cursor_position(struct radeonfb_display *dp)
{
	struct radeonfb_softc	*sc = dp->rd_softc;
	uint32_t		offset, hvoff, hvpos;	/* registers */
	uint32_t		coff;			/* cursor offset */
	int			i, x, y, xoff, yoff, crtcoff;

	/*
	 * XXX: this also needs to handle pan/scan
	 */
	for (i = 0; i < dp->rd_ncrtcs; i++) {

		struct radeonfb_crtc	*rcp = &dp->rd_crtcs[i];

		if (rcp->rc_number) {
			offset = RADEON_CUR2_OFFSET;
			hvoff = RADEON_CUR2_HORZ_VERT_OFF;
			hvpos = RADEON_CUR2_HORZ_VERT_POSN;
			crtcoff = RADEON_CRTC2_OFFSET;
		} else {
			offset = RADEON_CUR_OFFSET;
			hvoff = RADEON_CUR_HORZ_VERT_OFF;
			hvpos = RADEON_CUR_HORZ_VERT_POSN;
			crtcoff = RADEON_CRTC_OFFSET;
		}

		x = dp->rd_cursor.rc_pos.x;
		y = dp->rd_cursor.rc_pos.y;

		while (y < rcp->rc_yoffset) {
			rcp->rc_yoffset -= RADEON_PANINCREMENT;
		}
		while (y >= (rcp->rc_yoffset + rcp->rc_videomode.vdisplay)) {
			rcp->rc_yoffset += RADEON_PANINCREMENT;
		}
		while (x < rcp->rc_xoffset) {
			rcp->rc_xoffset -= RADEON_PANINCREMENT;
		}
		while (x >= (rcp->rc_xoffset + rcp->rc_videomode.hdisplay)) {
			rcp->rc_xoffset += RADEON_PANINCREMENT;
		}

		/* adjust for the cursor's hotspot */
		x -= dp->rd_cursor.rc_hot.x;
		y -= dp->rd_cursor.rc_hot.y;
		xoff = yoff = 0;

		if (x >= dp->rd_virtx)
			x = dp->rd_virtx - 1;
		if (y >= dp->rd_virty)
			y = dp->rd_virty - 1;

		/* now adjust cursor so it is relative to viewport */
		x -= rcp->rc_xoffset;
		y -= rcp->rc_yoffset;

		/*
		 * no need to check for fall off, because we should
		 * never move off the screen entirely!
		 */
		coff = 0;
		if (x < 0) {
			xoff = -x;
			x = 0;
		}
		if (y < 0) {
			yoff = -y;
			y = 0;
			coff = (yoff * 2) * 8;
		}

		/* pan the display */
		PUT32(sc, crtcoff, (rcp->rc_yoffset * dp->rd_stride) +
		    rcp->rc_xoffset);

		PUT32(sc, offset, (dp->rd_curoff + coff) | RADEON_CUR_LOCK);
		PUT32(sc, hvoff, (xoff << 16) | (yoff) | RADEON_CUR_LOCK);
		/* NB: this unlocks the cursor */
		PUT32(sc, hvpos, (x << 16) | y);
	}
}

static void
radeonfb_cursor_visible(struct radeonfb_display *dp)
{
	int		i;
	uint32_t	gencntl, bit;

	for (i = 0; i < dp->rd_ncrtcs; i++) {
		if (dp->rd_crtcs[i].rc_number) {
			gencntl = RADEON_CRTC2_GEN_CNTL;
			bit = RADEON_CRTC2_CUR_EN;
		} else {
			gencntl = RADEON_CRTC_GEN_CNTL;
			bit = RADEON_CRTC_CUR_EN;
		}
		
		if (dp->rd_cursor.rc_visible)
			SET32(dp->rd_softc, gencntl, bit);
		else
			CLR32(dp->rd_softc, gencntl, bit);
	}
}

static void
radeonfb_cursor_cmap(struct radeonfb_display *dp)
{
	int		i;
	uint32_t	c0reg, c1reg;
	struct radeonfb_softc	*sc = dp->rd_softc;

	for (i = 0; i < dp->rd_ncrtcs; i++) {
		if (dp->rd_crtcs[i].rc_number) {
			c0reg = RADEON_CUR2_CLR0;
			c1reg = RADEON_CUR2_CLR1;
		} else {
			c0reg = RADEON_CUR_CLR0;
			c1reg = RADEON_CUR_CLR1;
		}

		PUT32(sc, c0reg, dp->rd_cursor.rc_cmap[0]);
		PUT32(sc, c1reg, dp->rd_cursor.rc_cmap[1]);
	}
}

static void
radeonfb_cursor_update(struct radeonfb_display *dp, unsigned which)
{
	struct radeonfb_softc	*sc;
	int		i;

	sc = dp->rd_softc;
	for (i = 0; i < dp->rd_ncrtcs; i++) {
		if (dp->rd_crtcs[i].rc_number) {
			SET32(sc, RADEON_CUR2_OFFSET, RADEON_CUR_LOCK);
		} else {
			SET32(sc, RADEON_CUR_OFFSET,RADEON_CUR_LOCK);
		}
	}

	if (which & WSDISPLAY_CURSOR_DOCMAP)
		radeonfb_cursor_cmap(dp);

	if (which & WSDISPLAY_CURSOR_DOSHAPE)
		radeonfb_cursor_shape(dp);

	if (which & WSDISPLAY_CURSOR_DOCUR)
		radeonfb_cursor_visible(dp);

	/* this one is unconditional, because it updates other stuff */
	radeonfb_cursor_position(dp);
}

static struct videomode *
radeonfb_best_refresh(struct videomode *m1, struct videomode *m2)
{
	int	r1, r2;

	/* otherwise pick the higher refresh rate */
	r1 = DIVIDE(DIVIDE(m1->dot_clock, m1->htotal), m1->vtotal);
	r2 = DIVIDE(DIVIDE(m2->dot_clock, m2->htotal), m2->vtotal);

	return (r1 < r2 ? m2 : m1);
}

static const struct videomode *
radeonfb_port_mode(struct radeonfb_softc *sc, struct radeonfb_port *rp,
    int x, int y)
{
	struct edid_info	*ep = &rp->rp_edid;
	struct videomode	*vmp = NULL;
	int			i;

	if (!rp->rp_edid_valid) {
		/* fallback to safe mode */
		return radeonfb_modelookup(sc->sc_defaultmode);
	}
	
	/* always choose the preferred mode first! */
	if (ep->edid_preferred_mode) {

		/* XXX: add auto-stretching support for native mode */

		/* this may want panning to occur, btw */
		if ((ep->edid_preferred_mode->hdisplay <= x) &&
		    (ep->edid_preferred_mode->vdisplay <= y))
			return ep->edid_preferred_mode;
	}

	for (i = 0; i < ep->edid_nmodes; i++) {
		/*
		 * We elect to pick a resolution that is too large for
		 * the monitor than one that is too small.  This means
		 * that we will prefer to pan rather than to try to
		 * center a smaller display on a larger screen.  In
		 * practice, this shouldn't matter because if a
		 * monitor can support a larger resolution, it can
		 * probably also support the smaller.  A specific
		 * exception is fixed format panels, but hopefully
		 * they are properly dealt with by the "autostretch"
		 * logic above.
		 */
		if ((ep->edid_modes[i].hdisplay > x) ||
		    (ep->edid_modes[i].vdisplay > y)) {
			continue;
		}

		/*
		 * at this point, the display mode is no larger than
		 * what we've requested.
		 */
		if (vmp == NULL)
			vmp = &ep->edid_modes[i];

		/* eliminate smaller modes */
		if ((vmp->hdisplay >= ep->edid_modes[i].hdisplay) ||
		    (vmp->vdisplay >= ep->edid_modes[i].vdisplay))
			continue;

		if ((vmp->hdisplay < ep->edid_modes[i].hdisplay) ||
		    (vmp->vdisplay < ep->edid_modes[i].vdisplay)) {
			vmp = &ep->edid_modes[i];
			continue;
		}

		KASSERT(vmp->hdisplay == ep->edid_modes[i].hdisplay);
		KASSERT(vmp->vdisplay == ep->edid_modes[i].vdisplay);

		vmp = radeonfb_best_refresh(vmp, &ep->edid_modes[i]);
	}

	return (vmp ? vmp : radeonfb_modelookup(sc->sc_defaultmode));
}

static int
radeonfb_hasres(struct videomode *list, int nlist, int x, int y)
{
	int	i;

	for (i = 0; i < nlist; i++) {
		if ((x == list[i].hdisplay) &&
		    (y == list[i].vdisplay)) {
			return 1;
		}
	}
	return 0;
}

static void
radeonfb_pickres(struct radeonfb_display *dp, uint16_t *x, uint16_t *y,
    int pan)
{
	struct radeonfb_port	*rp;
	struct edid_info	*ep;
	int			i, j;

	*x = 0;
	*y = 0;

	if (pan) {
		for (i = 0; i < dp->rd_ncrtcs; i++) {
			rp = dp->rd_crtcs[i].rc_port;
			ep = &rp->rp_edid;
			if (!rp->rp_edid_valid) {
				/* monitor not present */
				continue;
			}

			/*
			 * For now we are ignoring "conflict" that
			 * could occur when mixing some modes like
			 * 1280x1024 and 1400x800.  It isn't clear
			 * which is better, so the first one wins.
			 */
			for (j = 0; j < ep->edid_nmodes; j++) {
				/*
				 * ignore resolutions that are too big for
				 * the radeon
				 */
				if (ep->edid_modes[j].hdisplay >
				    dp->rd_softc->sc_maxx)
					continue;
				if (ep->edid_modes[j].vdisplay >
				    dp->rd_softc->sc_maxy)
					continue;

				/*
				 * pick largest resolution, the
				 * smaller monitor will pan
				 */
				if ((ep->edid_modes[j].hdisplay >= *x) &&
				    (ep->edid_modes[j].vdisplay >= *y)) {
					*x = ep->edid_modes[j].hdisplay;
					*y = ep->edid_modes[j].vdisplay;
				}
			}
		}

	} else {
		struct videomode	modes[64];
		int			nmodes = 0;
		int			valid = 0;

		for (i = 0; i < dp->rd_ncrtcs; i++) {
			/*
			 * pick the largest resolution in common.
			 */
			rp = dp->rd_crtcs[i].rc_port;
			ep = &rp->rp_edid;

			if (!rp->rp_edid_valid)
				continue;

			if (!valid) {
				/*
				 * Pick the preferred mode for this port
				 * if available.
				 */
				if (ep->edid_preferred_mode) {
					struct videomode *vmp = 
						ep->edid_preferred_mode;

					if ((vmp->hdisplay <= 
					     dp->rd_softc->sc_maxx) && 
					    (vmp->vdisplay <= 
					     dp->rd_softc->sc_maxy))
						modes[nmodes++] = *vmp;
				} else {

					/* initialize starting list */
					for (j = 0; j < ep->edid_nmodes; j++) {
						/*
						 * ignore resolutions that are
						 * too big for the radeon
						 */
						if (ep->edid_modes[j].hdisplay >
						    dp->rd_softc->sc_maxx)
							continue;
						if (ep->edid_modes[j].vdisplay >
						    dp->rd_softc->sc_maxy)
							continue;

						modes[nmodes] = 
							ep->edid_modes[j];
						nmodes++;
					}
				}
				valid = 1;
			} else {
				/* merge into preexisting list */
				for (j = 0; j < nmodes; j++) {
					if (!radeonfb_hasres(ep->edid_modes,
						ep->edid_nmodes,
						modes[j].hdisplay,
						modes[j].vdisplay)) {
						modes[j] = modes[nmodes];
						j--;
						nmodes--;
					}
				}
			}
		}

		/* now we have to pick from the merged list */
		for (i = 0; i < nmodes; i++) {
			if ((modes[i].hdisplay >= *x) &&
			    (modes[i].vdisplay >= *y)) {
				*x = modes[i].hdisplay;
				*y = modes[i].vdisplay;
			}
		}
	}

	if ((*x == 0) || (*y == 0)) {
		/* fallback to safe mode */
		*x = 640;
		*y = 480;
	}
}

/*
 * backlight levels are linear on:
 * - RV200, RV250, RV280, RV350
 * - but NOT on PowerBook4,3 6,3 6,5
 * according to Linux' radeonfb
 */

/* Get the current backlight level for the display.  */

static int 
radeonfb_get_backlight(struct radeonfb_display *dp)
{
	int s;
	uint32_t level;

	s = spltty();

	level = radeonfb_get32(dp->rd_softc, RADEON_LVDS_GEN_CNTL);
	level &= RADEON_LVDS_BL_MOD_LEV_MASK;
	level >>= RADEON_LVDS_BL_MOD_LEV_SHIFT;

	/* 
	 * On some chips, we should negate the backlight level. 
	 * XXX Find out on which chips. 
	 */
	if (dp->rd_softc->sc_flags & RFB_INV_BLIGHT)
	level = RADEONFB_BACKLIGHT_MAX - level;

	splx(s);

	return level;
}	

/* Set the backlight to the given level for the display.  */
static void 
radeonfb_switch_backlight(struct radeonfb_display *dp, int on)
{
	if (dp->rd_bl_on == on)
		return;
	dp->rd_bl_on = on;
	radeonfb_set_backlight(dp, dp->rd_bl_level);
}

static int 
radeonfb_set_backlight(struct radeonfb_display *dp, int level)
{
	struct radeonfb_softc *sc = dp->rd_softc;;
	int rlevel, s;
	uint32_t lvds;

	if(!sc->sc_mapped)
		return 0;

	s = spltty();

	dp->rd_bl_level = level;
	if (dp->rd_bl_on == 0)
		level = 0;

	if (level < 0)
		level = 0;
	else if (level >= RADEONFB_BACKLIGHT_MAX)
		level = RADEONFB_BACKLIGHT_MAX;

	/* On some chips, we should negate the backlight level. */
	if (dp->rd_softc->sc_flags & RFB_INV_BLIGHT) {
		rlevel = RADEONFB_BACKLIGHT_MAX - level;
	} else
	rlevel = level;

	callout_stop(&dp->rd_bl_lvds_co);
	radeonfb_engine_idle(sc);

	/* 
	 * Turn off the display if the backlight is set to 0, since the
	 * display is useless without backlight anyway. 
	 */
	if (level == 0)
		radeonfb_blank(dp, 1);
	else if (radeonfb_get_backlight(dp) == 0)
		radeonfb_blank(dp, 0);
	
	lvds = radeonfb_get32(sc, RADEON_LVDS_GEN_CNTL);
	lvds &= ~RADEON_LVDS_DISPLAY_DIS;
	if (!(lvds & RADEON_LVDS_BLON) || !(lvds & RADEON_LVDS_ON)) {
		lvds |= dp->rd_bl_lvds_val & RADEON_LVDS_DIGON;
		lvds |= RADEON_LVDS_BLON | RADEON_LVDS_EN;
		radeonfb_put32(sc, RADEON_LVDS_GEN_CNTL, lvds);
		lvds &= ~RADEON_LVDS_BL_MOD_LEV_MASK;
		lvds |= rlevel << RADEON_LVDS_BL_MOD_LEV_SHIFT;
		lvds |= RADEON_LVDS_ON;
		lvds |= dp->rd_bl_lvds_val & RADEON_LVDS_BL_MOD_EN;
	} else {
		lvds &= ~RADEON_LVDS_BL_MOD_LEV_MASK;
		lvds |= rlevel << RADEON_LVDS_BL_MOD_LEV_SHIFT;
		radeonfb_put32(sc, RADEON_LVDS_GEN_CNTL, lvds);
	}
	
	dp->rd_bl_lvds_val &= ~RADEON_LVDS_STATE_MASK;
	dp->rd_bl_lvds_val |= lvds & RADEON_LVDS_STATE_MASK;
	/* XXX What is the correct delay? */
	callout_schedule(&dp->rd_bl_lvds_co, 200 * hz); 

	splx(s);

	return 0;
}

/* 
 * Callout function for delayed operations on the LVDS_GEN_CNTL register. 
 * Set the delayed bits in the register, and clear the stored delayed
 * value.
 */

static void radeonfb_lvds_callout(void *arg)
{
	struct radeonfb_display *dp = arg;
	int s;

	s = splhigh();

	radeonfb_mask32(dp->rd_softc, RADEON_LVDS_GEN_CNTL, ~0, 
			dp->rd_bl_lvds_val);
	dp->rd_bl_lvds_val = 0;

	splx(s);
}

static void
radeonfb_brightness_up(device_t dev)
{
	struct radeonfb_softc *sc = device_private(dev);
	struct radeonfb_display *dp = &sc->sc_displays[0];
	int level;

	/* we assume the main display is the first one - need a better way */
	if (sc->sc_ndisplays < 1) return;
	/* make sure pushing the hotkeys always has an effect */
	dp->rd_bl_on = 1;
	level = dp->rd_bl_level;
	level = min(RADEONFB_BACKLIGHT_MAX, level + 5);
	radeonfb_set_backlight(dp, level);
}

static void
radeonfb_brightness_down(device_t dev)
{
	struct radeonfb_softc *sc = device_private(dev);
	struct radeonfb_display *dp = &sc->sc_displays[0];
	int level;

	/* we assume the main display is the first one - need a better way */
	if (sc->sc_ndisplays < 1) return;
	/* make sure pushing the hotkeys always has an effect */
	dp->rd_bl_on = 1;
	level = dp->rd_bl_level;
	level = max(0, level - 5);
	radeonfb_set_backlight(dp, level);
}
