/*	$NetBSD: stic.c,v 1.51 2014/07/25 08:10:39 dholland Exp $	*/

/*-
 * Copyright (c) 1999, 2000, 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Andrew Doran.
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

/*
 * Driver for the DEC PixelStamp interface chip (STIC).
 *
 * XXX The bt459 interface shouldn't be replicated here.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: stic.c,v 1.51 2014/07/25 08:10:39 dholland Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/ioctl.h>
#include <sys/callout.h>
#include <sys/conf.h>
#include <sys/kauth.h>
#include <sys/lwp.h>
#include <sys/event.h>

#if defined(pmax)
#include <mips/cpuregs.h>
#elif defined(alpha)
#include <alpha/alpha_cpu.h>
#endif

#include <machine/vmparam.h>
#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>

#include <dev/wsfont/wsfont.h>

#include <dev/ic/bt459reg.h>

#include <dev/tc/tcvar.h>
#include <dev/tc/sticreg.h>
#include <dev/tc/sticio.h>
#include <dev/tc/sticvar.h>

#define DUPBYTE0(x) ((((x)&0xff)<<16) | (((x)&0xff)<<8) | ((x)&0xff))
#define DUPBYTE1(x) ((((x)<<8)&0xff0000) | ((x)&0xff00) | (((x)>>8)&0xff))
#define DUPBYTE2(x) (((x)&0xff0000) | (((x)>>8)&0xff00) | (((x)>>16)&0xff))

#define PACK(p, o) ((p)[(o)] | ((p)[(o)+1] << 16))

#if defined(pmax)
#define	machine_btop(x)		mips_btop(x)
#elif defined(alpha)
#define machine_btop(x)		alpha_btop(x)
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
 *		struct {
 *
 * Although HX has single Bt459, 32bit R/W can be done w/o any trouble.
 *	struct bt459reg {
 *		   uint32_t	   bt_lo;
 *		   uint32_t	   bt_hi;
 *		   uint32_t	   bt_reg;
 *		   uint32_t	   bt_cmap;
 *	};
 *
 */

/* Bt459 hardware registers */
#define bt_lo	0
#define bt_hi	1
#define bt_reg	2
#define bt_cmap 3

#define REG(base, index)	*((volatile uint32_t *)(base) + (index))
#define SELECT(vdac, regno) do {		\
	REG(vdac, bt_lo) = DUPBYTE0(regno);	\
	REG(vdac, bt_hi) = DUPBYTE1(regno);	\
	tc_wmb();				\
   } while (0)

static int	sticioctl(void *, void *, u_long, void *, int, struct lwp *);
static int	stic_alloc_screen(void *, const struct wsscreen_descr *,
				  void **, int *, int *, long *);
static void	stic_free_screen(void *, void *);
static int	stic_show_screen(void *, void *, int,
				 void (*)(void *, int, int), void *);

static void	stic_do_switch(void *);
static void	stic_setup_backing(struct stic_info *, struct stic_screen *);
static void	stic_setup_vdac(struct stic_info *);
static void	stic_clear_screen(struct stic_info *);

static int	stic_get_cmap(struct stic_info *, struct wsdisplay_cmap *);
static int	stic_set_cmap(struct stic_info *, struct wsdisplay_cmap *);
static int	stic_set_cursor(struct stic_info *, struct wsdisplay_cursor *);
static int	stic_get_cursor(struct stic_info *, struct wsdisplay_cursor *);
static void	stic_set_curpos(struct stic_info *, struct wsdisplay_curpos *);
static void	stic_set_hwcurpos(struct stic_info *);

static void	stic_cursor(void *, int, int, int);
static void	stic_copycols(void *, int, int, int, int);
static void	stic_copyrows(void *, int, int, int);
static void	stic_erasecols(void *, int, int, int, long);
static void	stic_eraserows(void *, int, int, long);
static int	stic_mapchar(void *, int, u_int *);
static void	stic_putchar(void *, int, int, u_int, long);
static int	stic_allocattr(void *, int, int, int, long *);

static dev_type_open(sticopen);
static dev_type_close(sticclose);
static dev_type_mmap(sticmmap);

const struct cdevsw stic_cdevsw = {
	.d_open = sticopen,
	.d_close = sticclose,
	.d_read = noread,
	.d_write = nowrite,
	.d_ioctl = noioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = nopoll,
	.d_mmap = sticmmap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = 0
};

/* Colormap for wscons, matching WSCOL_*. Upper 8 are high-intensity. */
static const uint8_t stic_cmap[16*3] = {
	0x00, 0x00, 0x00, /* black */
	0x7f, 0x00, 0x00, /* red */
	0x00, 0x7f, 0x00, /* green */
	0x7f, 0x7f, 0x00, /* brown */
	0x00, 0x00, 0x7f, /* blue */
	0x7f, 0x00, 0x7f, /* magenta */
	0x00, 0x7f, 0x7f, /* cyan */
	0xc7, 0xc7, 0xc7, /* white */

	0x7f, 0x7f, 0x7f, /* black */
	0xff, 0x00, 0x00, /* red */
	0x00, 0xff, 0x00, /* green */
	0xff, 0xff, 0x00, /* brown */
	0x00, 0x00, 0xff, /* blue */
	0xff, 0x00, 0xff, /* magenta */
	0x00, 0xff, 0xff, /* cyan */
	0xff, 0xff, 0xff, /* white */
};

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

static const struct wsdisplay_accessops stic_accessops = {
	sticioctl,
	NULL,			/* mmap */
	stic_alloc_screen,
	stic_free_screen,
	stic_show_screen,
	NULL,			/* load_font */
};

static const struct wsdisplay_emulops stic_emulops = {
	stic_cursor,
	stic_mapchar,
	stic_putchar,
	stic_copycols,
	stic_erasecols,
	stic_copyrows,
	stic_eraserows,
	stic_allocattr
};

static struct wsscreen_descr stic_stdscreen = {
	"std",
	0, 0,
	&stic_emulops,
	0, 0,
	WSSCREEN_WSCOLORS | WSSCREEN_HILIT
};

static const struct wsscreen_descr *_stic_scrlist[] = {
	&stic_stdscreen,
};

static const struct wsscreen_list stic_screenlist = {
	sizeof(_stic_scrlist) / sizeof(struct wsscreen_descr *), _stic_scrlist
};

struct	stic_info stic_consinfo;
static struct	stic_screen stic_consscr;
static struct	stic_info *stic_info[STIC_MAXDV];
static int	stic_unit;

void
stic_init(struct stic_info *si)
{
	volatile uint32_t *vdac;
	int i, cookie;

	/* Reset the STIC & stamp(s). */
	stic_reset(si);
	vdac = si->si_vdac;

	/* Hit it... */
	SELECT(vdac, BT459_IREG_COMMAND_0);
	REG(vdac, bt_reg) = 0x00c0c0c0; tc_wmb();

	/* Now reset the VDAC. */
	*si->si_vdac_reset = 0;
	tc_wmb();
	tc_syncbus();
	DELAY(1000);

	/* Finish the initialization. */
	SELECT(vdac, BT459_IREG_COMMAND_1);
	REG(vdac, bt_reg) = 0x00000000; tc_wmb();
	REG(vdac, bt_reg) = 0x00c2c2c2; tc_wmb();
	REG(vdac, bt_reg) = 0x00ffffff; tc_wmb();

	for (i = 0; i < 7; i++) {
		REG(vdac, bt_reg) = 0x00000000;
		tc_wmb();
	}

	/* Set cursor colormap. */
	SELECT(vdac, BT459_IREG_CCOLOR_1);
	REG(vdac, bt_reg) = 0x00ffffff; tc_wmb();
	REG(vdac, bt_reg) = 0x00ffffff; tc_wmb();
	REG(vdac, bt_reg) = 0x00ffffff; tc_wmb();
	REG(vdac, bt_reg) = 0x00000000; tc_wmb();
	REG(vdac, bt_reg) = 0x00000000; tc_wmb();
	REG(vdac, bt_reg) = 0x00000000; tc_wmb();
	REG(vdac, bt_reg) = 0x00ffffff; tc_wmb();
	REG(vdac, bt_reg) = 0x00ffffff; tc_wmb();
	REG(vdac, bt_reg) = 0x00ffffff; tc_wmb();

	/* Get a font and set up screen metrics. */
	wsfont_init();

	cookie = wsfont_find(NULL, 12, 0, 2, WSDISPLAY_FONTORDER_R2L,
	    WSDISPLAY_FONTORDER_L2R, WSFONT_FIND_BITMAP);
	if (cookie <= 0)
		cookie = wsfont_find(NULL, 0, 0, 2, WSDISPLAY_FONTORDER_R2L,
		    WSDISPLAY_FONTORDER_L2R, WSFONT_FIND_BITMAP);
	if (cookie <= 0)
		panic("stic_init: font table is empty");

	if (wsfont_lock(cookie, &si->si_font))
		panic("stic_init: couldn't lock font");

	si->si_fontw = si->si_font->fontwidth;
	si->si_fonth = si->si_font->fontheight;
	si->si_consw = (1280 / si->si_fontw) & ~1;
	si->si_consh = 1024 / si->si_fonth;
	stic_stdscreen.ncols = si->si_consw;
	stic_stdscreen.nrows = si->si_consh;

#ifdef DIAGNOSTIC
	if ((u_int)si->si_fonth > 32 || (u_int)si->si_fontw > 16)
		panic("stic_init: unusable font");
#endif

	stic_setup_vdac(si);
	stic_clear_screen(si);
	si->si_dispmode = WSDISPLAYIO_MODE_EMUL;
}

void
stic_reset(struct stic_info *si)
{
	int modtype, xconfig, yconfig, config;
	volatile struct stic_regs *sr;

	sr = si->si_stic;

	/*
	 * Initialize the interface chip registers.
	 */
	sr->sr_sticsr = 0x00000030;	/* Get the STIC's attention. */
	tc_wmb();
	tc_syncbus();
	DELAY(2000);			/* wait 2ms for STIC to respond. */
	sr->sr_sticsr = 0x00000000;	/* Hit the STIC's csr again... */
	tc_wmb();
	sr->sr_buscsr = 0xffffffff;	/* and bash its bus-acess csr. */
	tc_wmb();
	tc_syncbus();			/* Blam! */
	DELAY(20000);			/* wait until the stic recovers... */

	modtype = sr->sr_modcl;
	xconfig = (modtype & 0x800) >> 11;
	yconfig = (modtype & 0x600) >> 9;
	config = (yconfig << 1) | xconfig;
	si->si_stampw = (xconfig ? 5 : 4);
	si->si_stamph = (1 << yconfig);
	si->si_stamphm = si->si_stamph - 1;
#ifdef notyet
	si->si_option = (char)((modtype >> 12) & 3);
#endif

	/* First PixelStamp */
	si->si_stamp[0x000b0] = config;
	si->si_stamp[0x000b4] = 0x0;

	/* Second PixelStamp */
	if (yconfig > 0) {
		si->si_stamp[0x100b0] = config | 8;
		si->si_stamp[0x100b4] = 0;
	}

	/*
	 * Initialize STIC video registers.  Enable error and vertical
	 * retrace interrupts.  Set the packet done flag so the Xserver will
	 * not time-out on the first packet submitted.
	 */
	sr->sr_vblank = (1024 << 16) | 1063;
	sr->sr_vsync = (1027 << 16) | 1030;
	sr->sr_hblank = (255 << 16) | 340;
	sr->sr_hsync2 = 245;
	sr->sr_hsync = (261 << 16) | 293;
	sr->sr_ipdvint =
	    STIC_INT_WE | STIC_INT_P | STIC_INT_E_EN | STIC_INT_V_EN;
	sr->sr_sticsr = 8;
	tc_wmb();
	tc_syncbus();
}

void
stic_attach(device_t self, struct stic_info *si, int console)
{
	struct wsemuldisplaydev_attach_args waa;

	if (stic_unit < STIC_MAXDV) {
		stic_info[stic_unit] = si;
		si->si_unit = stic_unit++;
	} else
		si->si_unit = -1;

	callout_init(&si->si_switch_callout, 0);

	/*
	 * Allocate backing for the console.  We could trawl back through
	 * msgbuf and and fill the backing, but it's not worth the hassle.
	 * We could also grab backing using pmap_steal_memory() early on,
	 * but that's a little ugly.
	 */
	if (console)
		stic_setup_backing(si, &stic_consscr);

	waa.console = console;
	waa.scrdata = &stic_screenlist;
	waa.accessops = &stic_accessops;
	waa.accesscookie = si;

	config_found(self, &waa, wsemuldisplaydevprint);
}

void
stic_cnattach(struct stic_info *si)
{
	struct stic_screen *ss;
	long defattr;

	ss = &stic_consscr;
	si->si_curscreen = ss;
	ss->ss_flags = SS_ALLOCED | SS_ACTIVE | SS_CURENB;
	ss->ss_si = si;

	si->si_flags |= SI_CURENB_CHANGED;
	stic_flush(si);

	stic_allocattr(ss, 0, 0, 0, &defattr);
	stic_eraserows(ss, 0, si->si_consh, 0);
	wsdisplay_cnattach(&stic_stdscreen, ss, 0, 0, defattr);
}

static void
stic_setup_vdac(struct stic_info *si)
{
	uint8_t *ip, *mp;
	int r, c, o, b, i, s;

	s = spltty();

	ip = (uint8_t *)si->si_cursor.cc_image;
	mp = (uint8_t *)si->si_cursor.cc_mask;
	memset(ip, 0, sizeof(si->si_cursor.cc_image));
	memset(mp, 0, sizeof(si->si_cursor.cc_mask));

	for (r = 0; r < si->si_fonth; r++) {
		for (c = r & 1; c < si->si_fontw; c += 2) {
			o = c >> 3;
			b = 1 << (c & 7);
			ip[o] |= b;
			mp[o] |= b;
		}

		ip += 8;
		mp += 8;
	}

	si->si_cursor.cc_size.x = 64;
	si->si_cursor.cc_size.y = si->si_fonth;
	si->si_cursor.cc_hot.x = 0;
	si->si_cursor.cc_hot.y = 0;

	si->si_cursor.cc_color[0] = 0xff;
	si->si_cursor.cc_color[2] = 0xff;
	si->si_cursor.cc_color[4] = 0xff;
	si->si_cursor.cc_color[1] = 0x00;
	si->si_cursor.cc_color[3] = 0x00;
	si->si_cursor.cc_color[5] = 0x00;

	memset(&si->si_cmap, 0, sizeof(si->si_cmap));
	for (i = 0; i < 16; i++) {
		si->si_cmap.r[i] = stic_cmap[i*3 + 0];
		si->si_cmap.g[i] = stic_cmap[i*3 + 1];
		si->si_cmap.b[i] = stic_cmap[i*3 + 2];
	}

	si->si_flags |= SI_CMAP_CHANGED | SI_CURSHAPE_CHANGED |
	    SI_CURCMAP_CHANGED;

	splx(s);
}

static void
stic_clear_screen(struct stic_info *si)
{
	uint32_t *pb;
	int i;

	/*
	 * Do this twice, since the first packet after a reset may be
	 * silently ignored.
	 */
	for (i = 0; i < 2; i++) {
		pb = (*si->si_pbuf_get)(si);

		pb[0] = STAMP_CMD_LINES | STAMP_RGB_CONST | STAMP_LW_PERPACKET;
		pb[1] = 0x01ffffff;
		pb[2] = 0;
		pb[3] = STAMP_UPDATE_ENABLE | STAMP_METHOD_COPY;
		pb[4] = (1024 << 2) - 1;
		pb[5] = 0;
		pb[6] = 0;
		pb[7] = (1280 << 19) | ((1024 << 3) + pb[4]);

		(*si->si_pbuf_post)(si, pb);
	}
}

static int
sticioctl(void *v, void *vs, u_long cmd, void *data, int flag, struct lwp *l)
{
	struct stic_info *si;
	int s;

	si = v;

	switch (cmd) {
	case WSDISPLAYIO_GTYPE:
		*(u_int *)data = si->si_disptype;
		return (0);

	case WSDISPLAYIO_GINFO:
#define	wsd_fbip ((struct wsdisplay_fbinfo *)data)
		wsd_fbip->height = 1024;
		wsd_fbip->width = 1280;
		wsd_fbip->depth = si->si_depth == 8 ? 8 : 32;
		wsd_fbip->cmsize = CMAP_SIZE;
#undef fbt
		return (0);

	case WSDISPLAYIO_GETCMAP:
		return (stic_get_cmap(si, (struct wsdisplay_cmap *)data));

	case WSDISPLAYIO_PUTCMAP:
		return (stic_set_cmap(si, (struct wsdisplay_cmap *)data));

	case WSDISPLAYIO_SVIDEO:
#if 0 /* XXX later */
		turnoff = *(int *)data == WSDISPLAYIO_VIDEO_OFF;
		if ((si->si_blanked == 0) ^ turnoff)
			si->si_blanked = turnoff;
#endif
		return (0);

	case WSDISPLAYIO_GVIDEO:
#if 0 /* XXX later */
		*(u_int *)data = si->si_blanked ?
		    WSDISPLAYIO_VIDEO_OFF : WSDISPLAYIO_VIDEO_ON;
#endif
		return (0);

	case WSDISPLAYIO_GCURPOS:
		*(struct wsdisplay_curpos *)data = si->si_cursor.cc_pos;
		return (0);

	case WSDISPLAYIO_SCURPOS:
		stic_set_curpos(si, (struct wsdisplay_curpos *)data);
		return (0);

	case WSDISPLAYIO_GCURMAX:
		((struct wsdisplay_curpos *)data)->x =
		((struct wsdisplay_curpos *)data)->y = CURSOR_MAX_SIZE;
		return (0);

	case WSDISPLAYIO_GCURSOR:
		return (stic_get_cursor(si, (struct wsdisplay_cursor *)data));

	case WSDISPLAYIO_SCURSOR:
		return (stic_set_cursor(si, (struct wsdisplay_cursor *)data));

	case WSDISPLAYIO_SMODE:
		si->si_dispmode = *(int *)data;
		if (si->si_dispmode == WSDISPLAYIO_MODE_EMUL) {
			(*si->si_ioctl)(si, STICIO_STOPQ, NULL, flag, l);
			stic_setup_vdac(si);
			s = spltty();
			stic_flush(si);
			splx(s);
			stic_clear_screen(si);
			stic_do_switch(si->si_curscreen);
		}
		return (0);

	case STICIO_RESET:
		stic_reset(si);
		return (0);
	}

	if (si->si_ioctl != NULL)
		return ((*si->si_ioctl)(si, cmd, data, flag, l));

	return (EPASSTHROUGH);
}

static void
stic_setup_backing(struct stic_info *si, struct stic_screen *ss)
{
	int size;

	size = si->si_consw * si->si_consh * sizeof(*ss->ss_backing);
	ss->ss_backing = malloc(size, M_DEVBUF, M_NOWAIT|M_ZERO);
}

static int
stic_alloc_screen(void *v, const struct wsscreen_descr *type, void **cookiep,
		  int *curxp, int *curyp, long *attrp)
{
	struct stic_info *si;
	struct stic_screen *ss;

	si = (struct stic_info *)v;

	if ((stic_consscr.ss_flags & SS_ALLOCED) == 0)
		ss = &stic_consscr;
	else {
		ss = malloc(sizeof(*ss), M_DEVBUF, M_WAITOK|M_ZERO);
	}
	stic_setup_backing(si, ss);

	ss->ss_si = si;
	ss->ss_flags = SS_ALLOCED | SS_CURENB;

	*cookiep = ss;
	*curxp = 0;
	*curyp = 0;

	stic_allocattr(ss, 0, 0, 0, attrp);
	return (0);
}

static void
stic_free_screen(void *v, void *cookie)
{
	struct stic_screen *ss;

	ss = cookie;

#ifdef DIAGNOSTIC
	if (ss == &stic_consscr)
		panic("stic_free_screen: console");
	if (ss == ((struct stic_info *)v)->si_curscreen)
		panic("stic_free_screen: freeing current screen");
#endif

	free(ss->ss_backing, M_DEVBUF);
	free(ss, M_DEVBUF);
}

static int
stic_show_screen(void *v, void *cookie, int waitok,
		 void (*cb)(void *, int, int), void *cbarg)
{
	struct stic_info *si;

	si = (struct stic_info *)v;
	if (si->si_switchcbarg != NULL)
		return (EAGAIN);
	si->si_switchcb = cb;
	si->si_switchcbarg = cbarg;

	if (cb != NULL) {
		callout_reset(&si->si_switch_callout, 0, stic_do_switch,
		    cookie);
		return (EAGAIN);
	}

	stic_do_switch(cookie);
	return (0);
}

static void
stic_do_switch(void *cookie)
{
	struct stic_screen *ss;
	struct stic_info *si;
	u_int r, c, nr, nc;
	uint16_t *p, *sp;

	ss = cookie;
	si = ss->ss_si;

#ifdef DIAGNOSTIC
	if (ss->ss_backing == NULL)
		panic("stic_do_switch: screen not backed");
#endif

	/* Swap in the new screen, and temporarily disable its backing. */
	if (si->si_curscreen != NULL)
		si->si_curscreen->ss_flags ^= SS_ACTIVE;
	si->si_curscreen = ss;
	ss->ss_flags |= SS_ACTIVE;
	sp = ss->ss_backing;
	ss->ss_backing = NULL;

	/*
	 * We assume that most of the screen is blank and blast it with
	 * eraserows(), because eraserows() is cheap.
	 */
	nr = si->si_consh;
	stic_eraserows(ss, 0, nr, 0);

	nc = si->si_consw;
	p = sp;
	for (r = 0; r < nr; r++)
		for (c = 0; c < nc; c += 2, p += 2) {
			if ((p[0] & 0xfff0) != 0)
				stic_putchar(ss, r, c, p[0] >> 8,
				    p[0] & 0x00ff);
			if ((p[1] & 0xfff0) != 0)
				stic_putchar(ss, r, c + 1, p[1] >> 8,
				    p[1] & 0x00ff);
		}

	/*
	 * Re-enable the screen's backing, and move the cursor to the
	 * correct spot.
	 */
	ss->ss_backing = sp;
	si->si_cursor.cc_pos.x = ss->ss_curx;
	si->si_cursor.cc_pos.y = ss->ss_cury;
	stic_set_hwcurpos(si);
	si->si_flags |= SI_CURENB_CHANGED;

	/*
	 * XXX Since we don't yet receive vblank interrupts from the
	 * PXG, we must flush immediately.
	 */
	if (si->si_disptype == WSDISPLAY_TYPE_PXG)
		stic_flush(si);

	/* Tell wscons that we're done. */
	if (si->si_switchcbarg != NULL) {
		cookie = si->si_switchcbarg;
		si->si_switchcbarg = NULL;
		(*si->si_switchcb)(cookie, 0, 0);
	}
}

static int
stic_allocattr(void *cookie, int fg, int bg, int flags, long *attr)
{
	long tmp;

	if ((flags & (WSATTR_BLINK | WSATTR_UNDERLINE)) != 0)
		return (EINVAL);

	if ((flags & WSATTR_WSCOLORS) == 0) {
		fg = 7;
		bg = 0;
	}

	if ((flags & WSATTR_HILIT) != 0)
		fg += 8;

	tmp = fg | (bg << 4);
	*attr = tmp | (tmp << 16);
	return (0);
}

static void
stic_erasecols(void *cookie, int row, int col, int num, long attr)
{
	struct stic_info *si;
	struct stic_screen *ss;
	uint32_t *pb;
	u_int i, linewidth;
	uint16_t *p;

	ss = cookie;
	si = ss->ss_si;

	if (ss->ss_backing != NULL) {
		p = ss->ss_backing + row * si->si_consw + col;
		for (i = num; i != 0; i--)
			*p++ = (uint16_t)attr;
	}
	if ((ss->ss_flags & SS_ACTIVE) == 0)
		return;

	col = (col * si->si_fontw) << 19;
	num = (num * si->si_fontw) << 19;
	row = row * si->si_fonth;
	attr = (attr & 0xf0) >> 4;
	linewidth = (si->si_fonth << 2) - 1;
	row = (row << 3) + linewidth;

	pb = (*si->si_pbuf_get)(si);

	pb[0] = STAMP_CMD_LINES | STAMP_RGB_CONST | STAMP_LW_PERPACKET;
	pb[1] = 0x01ffffff;
	pb[2] = 0;
	pb[3] = STAMP_UPDATE_ENABLE | STAMP_METHOD_COPY;
	pb[4] = linewidth;
	pb[5] = DUPBYTE0(attr);
	pb[6] = col | row;
	pb[7] = (col + num) | row;

	(*si->si_pbuf_post)(si, pb);
}

static void
stic_eraserows(void *cookie, int row, int num, long attr)
{
	struct stic_info *si;
	struct stic_screen *ss;
	u_int linewidth, i;
	uint32_t *pb;

	ss = cookie;
	si = ss->ss_si;

	if (ss->ss_backing != NULL) {
		pb = (uint32_t *)(ss->ss_backing + row * si->si_consw);
		for (i = si->si_consw * num; i > 0; i -= 2)
			*pb++ = (uint32_t)attr;
	}
	if ((ss->ss_flags & SS_ACTIVE) == 0)
		return;

	row *= si->si_fonth;
	num *= si->si_fonth;
	attr = (attr & 0xf0) >> 4;
	linewidth = (num << 2) - 1;
	row = (row << 3) + linewidth;

	pb = (*si->si_pbuf_get)(si);

	pb[0] = STAMP_CMD_LINES | STAMP_RGB_CONST | STAMP_LW_PERPACKET;
	pb[1] = 0x01ffffff;
	pb[2] = 0;
	pb[3] = STAMP_UPDATE_ENABLE | STAMP_METHOD_COPY;
	pb[4] = linewidth;
	pb[5] = DUPBYTE0(attr);
	pb[6] = row;
	pb[7] = (1280 << 19) | row;

	(*si->si_pbuf_post)(si, pb);
}

static void
stic_copyrows(void *cookie, int src, int dst, int height)
{
	struct stic_info *si;
	struct stic_screen *ss;
	uint32_t *pb, *pbs;
	u_int num, inc, adj;

	ss = cookie;
	si = ss->ss_si;

	if (ss->ss_backing != NULL)
		bcopy(ss->ss_backing + src * si->si_consw,
		    ss->ss_backing + dst * si->si_consw,
		    si->si_consw * sizeof(*ss->ss_backing) * height);
	if ((ss->ss_flags & SS_ACTIVE) == 0)
		return;

	/*
	 * We need to do this in reverse if the destination row is below
	 * the source.
	 */
	if (dst > src) {
		src += height;
		dst += height;
		inc = -8;
		adj = -1;
	} else {
		inc = 8;
		adj = 0;
	}

	src = (src * si->si_fonth + adj) << 3;
	dst = (dst * si->si_fonth + adj) << 3;
	height *= si->si_fonth;

	while (height > 0) {
		num = (height < 255 ? height : 255);
		height -= num;

		pbs = (*si->si_pbuf_get)(si);
		pb = pbs;

		pb[0] = STAMP_CMD_COPYSPANS | STAMP_LW_PERPACKET;
		pb[1] = (num << 24) | 0xffffff;
		pb[2] = 0x0;
		pb[3] = STAMP_UPDATE_ENABLE | STAMP_METHOD_COPY | STAMP_SPAN |
		    STAMP_COPYSPAN_ALIGNED;
		pb[4] = 1; /* linewidth */

		for (; num != 0; num--, src += inc, dst += inc, pb += 3) {
			pb[5] = 1280 << 3;
			pb[6] = src;
			pb[7] = dst;
		}

	    	(*si->si_pbuf_post)(si, pbs);
	}
}

static void
stic_copycols(void *cookie, int row, int src, int dst, int num)
{
	struct stic_info *si;
	struct stic_screen *ss;
	u_int height, updword;
	uint32_t *pb, *pbs;

	ss = cookie;
	si = ss->ss_si;

	if (ss->ss_backing != NULL)
		bcopy(ss->ss_backing + row * si->si_consw + src,
		    ss->ss_backing + row * si->si_consw + dst,
		    num * sizeof(*ss->ss_backing));
	if ((ss->ss_flags & SS_ACTIVE) == 0)
		return;

	/*
	 * The stamp reads and writes left -> right only, so we need to
	 * buffer the span if the source and destination regions overlap
	 * and the source is left of the destination.
	 */
	updword = STAMP_UPDATE_ENABLE | STAMP_METHOD_COPY | STAMP_SPAN;

	if (src < dst && src + num > dst)
		updword |= STAMP_HALF_BUFF;

	row = (row * si->si_fonth) << 3;
	num = (num * si->si_fontw) << 3;
	src = row | ((src * si->si_fontw) << 19);
	dst = row | ((dst * si->si_fontw) << 19);
	height = si->si_fonth;

	pbs = (*si->si_pbuf_get)(si);
	pb = pbs;

	pb[0] = STAMP_CMD_COPYSPANS | STAMP_LW_PERPACKET;
	pb[1] = (height << 24) | 0xffffff;
	pb[2] = 0x0;
	pb[3] = updword;
	pb[4] = 1; /* linewidth */

	for ( ; height != 0; height--, src += 8, dst += 8, pb += 3) {
		pb[5] = num;
		pb[6] = src;
		pb[7] = dst;
	}

	(*si->si_pbuf_post)(si, pbs);
}

static void
stic_putchar(void *cookie, int r, int c, u_int uc, long attr)
{
	struct wsdisplay_font *font;
	struct stic_screen *ss;
	struct stic_info *si;
	u_int i, bgcolor, fgcolor;
	u_int *pb, v1, v2, xya;
	u_short *fr;

	ss = cookie;
	si = ss->ss_si;

	/* It's cheaper to use erasecols() to blit blanks. */
	if (uc == 0) {
		stic_erasecols(cookie, r, c, 1, attr);
		return;
	}

	if (ss->ss_backing != NULL)
		ss->ss_backing[r * si->si_consw + c] =
		    (u_short)((attr & 0xff) | (uc << 8));
	if ((ss->ss_flags & SS_ACTIVE) == 0)
		return;

	font = si->si_font;
	pb = (*si->si_pbuf_get)(si);

	/*
	 * Create a mask from the glyph.  Squeeze the foreground color
	 * through the mask, and then squeeze the background color through
	 * the inverted mask.  We may well read outside the glyph when
	 * creating the mask, but it's bounded by the hardware so it
	 * shouldn't matter a great deal...
	 */
	pb[0] = STAMP_CMD_LINES | STAMP_RGB_FLAT | STAMP_XY_PERPRIMATIVE |
	    STAMP_LW_PERPRIMATIVE;
	pb[1] = font->fontheight > 16 ? 0x04ffffff : 0x02ffffff;
	pb[2] = 0x0;
	pb[3] = STAMP_UPDATE_ENABLE | STAMP_WE_XYMASK | STAMP_METHOD_COPY;

	r *= font->fontheight;
	c *= font->fontwidth;
	uc = (uc - font->firstchar) * font->stride * font->fontheight;
	fr = (u_short *)((char *)font->data + uc);
	bgcolor = DUPBYTE0((attr & 0xf0) >> 4);
	fgcolor = DUPBYTE0(attr & 0x0f);

	i = ((font->fontheight > 16 ? 16 : font->fontheight) << 2) - 1;
	v1 = (c << 19) | ((r << 3) + i);
	v2 = ((c + font->fontwidth) << 19) | (v1 & 0xffff);
	xya = XYMASKADDR(si->si_stampw, si->si_stamphm, c, r, 0, 0);

	pb[4] = PACK(fr, 0);
	pb[5] = PACK(fr, 2);
	pb[6] = PACK(fr, 4);
	pb[7] = PACK(fr, 6);
	pb[8] = PACK(fr, 8);
	pb[9] = PACK(fr, 10);
	pb[10] = PACK(fr, 12);
	pb[11] = PACK(fr, 14);
	pb[12] = xya;
	pb[13] = v1;
	pb[14] = v2;
	pb[15] = i;
	pb[16] = fgcolor;

	pb[17] = ~pb[4];
	pb[18] = ~pb[5];
	pb[19] = ~pb[6];
	pb[20] = ~pb[7];
	pb[21] = ~pb[8];
	pb[22] = ~pb[9];
	pb[23] = ~pb[10];
	pb[24] = ~pb[11];
	pb[25] = xya;
	pb[26] = v1;
	pb[27] = v2;
	pb[28] = i;
	pb[29] = bgcolor;

	/* Two more squeezes for the lower part of the character. */
	if (font->fontheight > 16) {
		i = ((font->fontheight - 16) << 2) - 1;
		r += 16;
		v1 = (c << 19) | ((r << 3) + i);
		v2 = ((c + font->fontwidth) << 19) | (v1 & 0xffff);

		pb[30] = PACK(fr, 16);
		pb[31] = PACK(fr, 18);
		pb[32] = PACK(fr, 20);
		pb[33] = PACK(fr, 22);
		pb[34] = PACK(fr, 24);
		pb[35] = PACK(fr, 26);
		pb[36] = PACK(fr, 28);
		pb[37] = PACK(fr, 30);
		pb[38] = xya;
		pb[39] = v1;
		pb[40] = v2;
		pb[41] = i;
		pb[42] = fgcolor;

		pb[43] = ~pb[30];
		pb[44] = ~pb[31];
		pb[45] = ~pb[32];
		pb[46] = ~pb[33];
		pb[47] = ~pb[34];
		pb[48] = ~pb[35];
		pb[49] = ~pb[36];
		pb[50] = ~pb[37];
		pb[51] = xya;
		pb[52] = v1;
		pb[53] = v2;
		pb[54] = i;
		pb[55] = bgcolor;
	}

	(*si->si_pbuf_post)(si, pb);
}

static int
stic_mapchar(void *cookie, int c, u_int *cp)
{
	struct stic_info *si;

	si = ((struct stic_screen *)cookie)->ss_si;

	if (c < si->si_font->firstchar || c == ' ') {
		*cp = 0;
		return (0);
	}

	if (c - si->si_font->firstchar >= si->si_font->numchars) {
		*cp = 0;
		return (0);
	}

	*cp = c;
	return (5);
}

static void
stic_cursor(void *cookie, int on, int row, int col)
{
	struct stic_screen *ss;
	struct stic_info *si;
	int s;

	ss = cookie;
	si = ss->ss_si;

	ss->ss_curx = col * si->si_fontw;
	ss->ss_cury = row * si->si_fonth;

	s = spltty();

	if (on)
		ss->ss_flags |= SS_CURENB;
	else
		ss->ss_flags &= ~SS_CURENB;

	if ((ss->ss_flags & SS_ACTIVE) != 0) {
		si->si_cursor.cc_pos.x = ss->ss_curx;
		si->si_cursor.cc_pos.y = ss->ss_cury;
		si->si_flags |= SI_CURENB_CHANGED;
		stic_set_hwcurpos(si);

		/*
		 * XXX Since we don't yet receive vblank interrupts from the
		 * PXG, we must flush immediately.
		 */
		if (si->si_disptype == WSDISPLAY_TYPE_PXG)
			stic_flush(si);
	}

	splx(s);
}

void
stic_flush(struct stic_info *si)
{
	volatile uint32_t *vdac;
	int v;

	if ((si->si_flags & SI_ALL_CHANGED) == 0)
		return;

	vdac = si->si_vdac;
	v = si->si_flags;
	si->si_flags &= ~SI_ALL_CHANGED;

	if ((v & SI_CURENB_CHANGED) != 0) {
		SELECT(vdac, BT459_IREG_CCR);
		if ((si->si_curscreen->ss_flags & SS_CURENB) != 0)
			REG(vdac, bt_reg) = 0x00c0c0c0;
		else
			REG(vdac, bt_reg) = 0x00000000;
		tc_wmb();
	}

	if ((v & SI_CURCMAP_CHANGED) != 0) {
		uint8_t *cp;

		cp = si->si_cursor.cc_color;

		SELECT(vdac, BT459_IREG_CCOLOR_2);
		REG(vdac, bt_reg) = DUPBYTE0(cp[1]);	tc_wmb();
		REG(vdac, bt_reg) = DUPBYTE0(cp[3]);	tc_wmb();
		REG(vdac, bt_reg) = DUPBYTE0(cp[5]);	tc_wmb();
		REG(vdac, bt_reg) = DUPBYTE0(cp[0]);	tc_wmb();
		REG(vdac, bt_reg) = DUPBYTE0(cp[2]);	tc_wmb();
		REG(vdac, bt_reg) = DUPBYTE0(cp[4]);	tc_wmb();
	}

	if ((v & SI_CURSHAPE_CHANGED) != 0) {
		uint8_t *ip, *mp, img, msk;
		uint8_t u;
		int bcnt;

		ip = (uint8_t *)si->si_cursor.cc_image;
		mp = (uint8_t *)si->si_cursor.cc_mask;

		bcnt = 0;
		SELECT(vdac, BT459_IREG_CRAM_BASE);
		/* 64 pixel scan line is consisted with 16 byte cursor ram */
		while (bcnt < CURSOR_MAX_SIZE * 16) {
			img = *ip++;
			msk = *mp++;
			img &= msk;	/* cookie off image */
			u = (msk & 0x0f) << 4 | (img & 0x0f);
			REG(vdac, bt_reg) = DUPBYTE0(shuffle[u]);
			tc_wmb();
			u = (msk & 0xf0) | (img & 0xf0) >> 4;
			REG(vdac, bt_reg) = DUPBYTE0(shuffle[u]);
			tc_wmb();
			bcnt += 2;
		}
	}

	if ((v & SI_CMAP_CHANGED) != 0) {
		struct stic_hwcmap256 *cm;
		int index;

		cm = &si->si_cmap;

		SELECT(vdac, 0);
		SELECT(vdac, 0);
		for (index = 0; index < CMAP_SIZE; index++) {
			REG(vdac, bt_cmap) = DUPBYTE0(cm->r[index]);
			tc_wmb();
			REG(vdac, bt_cmap) = DUPBYTE0(cm->g[index]);
			tc_wmb();
			REG(vdac, bt_cmap) = DUPBYTE0(cm->b[index]);
			tc_wmb();
		}
	}
}

static int
stic_get_cmap(struct stic_info *si, struct wsdisplay_cmap *p)
{
	u_int index = p->index, count = p->count;
	int error;

	if (index >= CMAP_SIZE || count > CMAP_SIZE - index)
		return (EINVAL);

	error = copyout(&si->si_cmap.r[index], p->red, count);
	if (error)
		return error;
	error = copyout(&si->si_cmap.g[index], p->green, count);
	if (error)
		return error;
	error = copyout(&si->si_cmap.b[index], p->blue, count);
	return error;
}

static int
stic_set_cmap(struct stic_info *si, struct wsdisplay_cmap *p)
{
	struct stic_hwcmap256 cmap;
	u_int index, count;
	int s, error;

	index = p->index;
	count = p->count;

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
	memcpy(&si->si_cmap.r[index], &cmap.r[index], count);
	memcpy(&si->si_cmap.g[index], &cmap.g[index], count);
	memcpy(&si->si_cmap.b[index], &cmap.b[index], count);
	si->si_flags |= SI_CMAP_CHANGED;
	splx(s);

	/*
	 * XXX Since we don't yet receive vblank interrupts from the PXG, we
	 * must flush immediately.
	 */
	if (si->si_disptype == WSDISPLAY_TYPE_PXG)
		stic_flush(si);

	return (0);
}

static int
stic_set_cursor(struct stic_info *si, struct wsdisplay_cursor *p)
{
#define	cc (&si->si_cursor)
	u_int v, index = 0, count = 0, icount = 0;
	struct stic_screen *ss;
	uint8_t r[2], g[2], b[2], image[512], mask[512];
	int s, error;

	v = p->which;
	ss = si->si_curscreen;
	if ((v & WSDISPLAY_CURSOR_DOCMAP) != 0) {
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
	if ((v & WSDISPLAY_CURSOR_DOSHAPE) != 0) {
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
	if ((v & (WSDISPLAY_CURSOR_DOPOS | WSDISPLAY_CURSOR_DOCUR)) != 0) {
		if (v & WSDISPLAY_CURSOR_DOCUR)
			cc->cc_hot = p->hot;
		if (v & WSDISPLAY_CURSOR_DOPOS)
			stic_set_curpos(si, &p->pos);
	}

	s = spltty();
	if ((v & WSDISPLAY_CURSOR_DOCUR) != 0) {
		if (p->enable)
			ss->ss_flags |= SS_CURENB;
		else
			ss->ss_flags &= ~SS_CURENB;
		si->si_flags |= SI_CURENB_CHANGED;
	}
	if ((v & WSDISPLAY_CURSOR_DOCMAP) != 0) {
		memcpy(&cc->cc_color[index], &r[index], count);
		memcpy(&cc->cc_color[index + 2], &g[index], count);
		memcpy(&cc->cc_color[index + 4], &b[index], count);
		si->si_flags |= SI_CURCMAP_CHANGED;
	}
	if ((v & WSDISPLAY_CURSOR_DOSHAPE) != 0) {
		memset(cc->cc_image, 0, sizeof cc->cc_image);
		memcpy(cc->cc_image, image, icount);
		memset(cc->cc_mask, 0, sizeof cc->cc_mask);
		memcpy(cc->cc_mask, mask, icount);
		si->si_flags |= SI_CURSHAPE_CHANGED;
	}
	splx(s);

	/*
	 * XXX Since we don't yet receive vblank interrupts from the PXG, we
	 * must flush immediately.
	 */
	if (si->si_disptype == WSDISPLAY_TYPE_PXG)
		stic_flush(si);

	return (0);
#undef cc
}

static int
stic_get_cursor(struct stic_info *si, struct wsdisplay_cursor *p)
{

	/* XXX */
	return (EPASSTHROUGH);
}

static void
stic_set_curpos(struct stic_info *si, struct wsdisplay_curpos *curpos)
{
	int x, y;

	x = curpos->x;
	y = curpos->y;

	if (y < 0)
		y = 0;
	else if (y > 1023)
		y = 1023;
	if (x < 0)
		x = 0;
	else if (x > 1279)
		x = 1279;

	si->si_cursor.cc_pos.x = x;
	si->si_cursor.cc_pos.y = y;
	stic_set_hwcurpos(si);
}

static void
stic_set_hwcurpos(struct stic_info *si)
{
	volatile uint32_t *vdac;
	int x, y, s;

	vdac = si->si_vdac;

	x = si->si_cursor.cc_pos.x - si->si_cursor.cc_hot.x;
	y = si->si_cursor.cc_pos.y - si->si_cursor.cc_hot.y;
	x += STIC_MAGIC_X;
	y += STIC_MAGIC_Y;

	s = spltty();
	SELECT(vdac, BT459_IREG_CURSOR_X_LOW);
	REG(vdac, bt_reg) = DUPBYTE0(x); tc_wmb();
	REG(vdac, bt_reg) = DUPBYTE1(x); tc_wmb();
	REG(vdac, bt_reg) = DUPBYTE0(y); tc_wmb();
	REG(vdac, bt_reg) = DUPBYTE1(y); tc_wmb();
	splx(s);
}

/*
 * STIC control inteface.  We have a separate device for mapping the board,
 * because access to the DMA engine means that it's possible to circumvent
 * the securelevel mechanism.
 */
static int
sticopen(dev_t dev, int flag, int mode, struct lwp *l)
{
	struct stic_info *si;
	int s, error;

	error = kauth_authorize_device_passthru(l->l_cred, dev,
	    KAUTH_REQ_DEVICE_RAWIO_PASSTHRU_ALL, NULL);
	if (error)
		return (error);
	if (minor(dev) >= STIC_MAXDV)
		return (ENXIO);
	if ((si = stic_info[minor(dev)]) == NULL)
		return (ENXIO);

	s = spltty();
	if ((si->si_flags & SI_DVOPEN) != 0) {
		splx(s);
		return (EBUSY);
	}
	si->si_flags |= SI_DVOPEN;
	splx(s);

	return (0);
}

static int
sticclose(dev_t dev, int flag, int mode, struct lwp *l)
{
	struct stic_info *si;
	int s;

	si = stic_info[minor(dev)];
	s = spltty();
	si->si_flags &= ~SI_DVOPEN;
	splx(s);

	return (0);
}

static paddr_t
sticmmap(dev_t dev, off_t offset, int prot)
{
	struct stic_info *si;
	struct stic_xmap *sxm;
	paddr_t pa;

	si = stic_info[minor(dev)];
	sxm = NULL;

	if (si->si_dispmode != WSDISPLAYIO_MODE_MAPPED)
		return (-1L);

	if (offset < 0)
		return ((paddr_t)-1L);

	if (offset < sizeof(sxm->sxm_stic)) {
		pa = STIC_KSEG_TO_PHYS(si->si_stic);
		return (machine_btop(pa + offset));
	}
	offset -= sizeof(sxm->sxm_stic);

	if (offset < sizeof(sxm->sxm_poll)) {
		pa = STIC_KSEG_TO_PHYS(si->si_slotbase);
		return (machine_btop(pa + offset));
	}
	offset -= sizeof(sxm->sxm_poll);

	if (offset < si->si_buf_size)
		return (machine_btop(si->si_buf_phys + offset));

	return ((paddr_t)-1L);
}
