/*	 $NetBSD: rasops.c,v 1.101.2.1 2019/08/15 12:21:27 martin Exp $	*/

/*-
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: rasops.c,v 1.101.2.1 2019/08/15 12:21:27 martin Exp $");

//#ifdef _KERNEL_OPT
#include "opt_rasops.h"
//#include "opt_wsmsgattrs.h"
//#include "rasops_glue.h"
//#endif

#include <sys/param.h>
#include <sys/bswap.h>
//#include <sys/kmem.h>
#include <malloc.h>
#include <assert.h>
#include <sys/proc.h>
#include <sys/ioctl.h>
#include <minix/sysutil.h>

#include <machine/endian.h>

#include "../wscons/wsconsio.h"
#include "../wscons/wsdisplayvar.h"
#include "../wsfont/wsfont.h"

#define	_RASOPS_PRIVATE
#include "rasops.h"
#include "rasops_masks.h"	/* XXX for MBE */

#ifndef _KERNEL
#include <errno.h>
#endif

#ifdef RASOPS_DEBUG
#define DPRINTF(...) aprint_error(...)
#else
#define DPRINTF(...) __nothing
#endif

struct rasops_matchdata {
	struct rasops_info *ri;
	int wantcols, wantrows;
	int bestscore;
	struct wsdisplay_font *pick;
	int ident;
};	

static const uint32_t rasops_lmask32[4 + 1] = {
	MBE(0x00000000), MBE(0x00ffffff), MBE(0x0000ffff), MBE(0x000000ff),
	MBE(0x00000000),
};

static const uint32_t rasops_rmask32[4 + 1] = {
	MBE(0x00000000), MBE(0xff000000), MBE(0xffff0000), MBE(0xffffff00),
	MBE(0xffffffff),
};

/* ANSI colormap (R,G,B). Upper 8 are high-intensity */
const uint8_t rasops_cmap[256 * 3] = {
	0x00, 0x00, 0x00, /* black */
	0x7f, 0x00, 0x00, /* red */
	0x00, 0x7f, 0x00, /* green */
	0x7f, 0x7f, 0x00, /* brown */
	0x00, 0x00, 0x7f, /* blue */
	0x7f, 0x00, 0x7f, /* magenta */
	0x00, 0x7f, 0x7f, /* cyan */
	0xc7, 0xc7, 0xc7, /* white - XXX too dim? */

	0x7f, 0x7f, 0x7f, /* black */
	0xff, 0x00, 0x00, /* red */
	0x00, 0xff, 0x00, /* green */
	0xff, 0xff, 0x00, /* brown */
	0x00, 0x00, 0xff, /* blue */
	0xff, 0x00, 0xff, /* magenta */
	0x00, 0xff, 0xff, /* cyan */
	0xff, 0xff, 0xff, /* white */

	/*
	 * For the cursor, we need at least the last (255th)
	 * color to be white. Fill up white completely for
	 * simplicity.
	 */
#define _CMWHITE 0xff, 0xff, 0xff,
#define _CMWHITE16	_CMWHITE _CMWHITE _CMWHITE _CMWHITE \
			_CMWHITE _CMWHITE _CMWHITE _CMWHITE \
			_CMWHITE _CMWHITE _CMWHITE _CMWHITE \
			_CMWHITE _CMWHITE _CMWHITE _CMWHITE
	_CMWHITE16 _CMWHITE16 _CMWHITE16 _CMWHITE16 _CMWHITE16
	_CMWHITE16 _CMWHITE16 _CMWHITE16 _CMWHITE16 _CMWHITE16
	_CMWHITE16 _CMWHITE16 _CMWHITE16 _CMWHITE16 /* but not the last one */
#undef _CMWHITE16
#undef _CMWHITE

	/*
	 * For the cursor the fg/bg indices are bit inverted, so
	 * provide complimentary colors in the upper 16 entries.
	 */
	0x7f, 0x7f, 0x7f, /* black */
	0xff, 0x00, 0x00, /* red */
	0x00, 0xff, 0x00, /* green */
	0xff, 0xff, 0x00, /* brown */
	0x00, 0x00, 0xff, /* blue */
	0xff, 0x00, 0xff, /* magenta */
	0x00, 0xff, 0xff, /* cyan */
	0xff, 0xff, 0xff, /* white */

	0x00, 0x00, 0x00, /* black */
	0x7f, 0x00, 0x00, /* red */
	0x00, 0x7f, 0x00, /* green */
	0x7f, 0x7f, 0x00, /* brown */
	0x00, 0x00, 0x7f, /* blue */
	0x7f, 0x00, 0x7f, /* magenta */
	0x00, 0x7f, 0x7f, /* cyan */
	0xc7, 0xc7, 0xc7, /* white - XXX too dim? */
};

/* True if color is gray */
static const uint8_t rasops_isgray[16] = {
	1, 0, 0, 0, 0, 0, 0, 1,
	1, 0, 0, 0, 0, 0, 0, 1,
};

#ifdef RASOPS_APPLE_PALETTE
/*
 * Approximate ANSI colormap for legacy Apple color palettes
 */
static const uint8_t apple8_devcmap[16] = {
	0xff,	/* black	0x00, 0x00, 0x00 */
	0x6b,	/* red		0x99, 0x00, 0x00 */
	0xc5,	/* green	0x00, 0x99, 0x00 */
	0x59,	/* yellow	0x99, 0x99, 0x00 */
	0xd4,	/* blue		0x00, 0x00, 0x99 */
	0x68,	/* magenta	0x99, 0x00, 0x99 */
	0xc2,	/* cyan		0x00, 0x99, 0x99 */
	0x2b,	/* white	0xcc, 0xcc, 0xcc */

	0x56,	/* black	0x99, 0x99, 0x99 */
	0x23,	/* red		0xff, 0x00, 0x00 */
	0xb9,	/* green	0x00, 0xff, 0x00 */
	0x05,	/* yellow	0xff, 0xff, 0x00 */
	0xd2,	/* blue		0x00, 0x00, 0xff */
	0x1e,	/* magenta	0xff, 0x00, 0xff */
	0xb4,	/* cyan		0x00, 0xff, 0xff */
	0x00,	/* white	0xff, 0xff, 0xff */
};

static const uint8_t apple4_devcmap[16] = {
	15,	/* black	*/
	 3,	/* red		*/
	 9,	/* dark green	*/
	 1,	/* yellow	*/
	 6,	/* blue		*/
	 4,	/* magenta	*/
	 7,	/* cyan		*/
	12,	/* light grey	*/

	13,	/* medium grey	*/
	 3,	/* red		*/
	 9,	/* dark green	*/
	 1,	/* yellow	*/
	 6,	/* blue		*/
	 4,	/* magenta	*/
	 7,	/* cyan		*/
	 0,	/* white	*/
};
#endif

/* Generic functions */
static void	rasops_copyrows(void *, int, int, int);
static void	rasops_copycols(void *, int, int, int, int);
static int	rasops_mapchar(void *, int, u_int *);
static void	rasops_cursor(void *, int, int, int);
static int	rasops_allocattr_color(void *, int, int, int, long *);
static int	rasops_allocattr_mono(void *, int, int, int, long *);
static void	rasops_do_cursor(struct rasops_info *);
static void	rasops_init_devcmap(struct rasops_info *);
static void	rasops_make_box_chars_8(struct rasops_info *);
static void	rasops_make_box_chars_16(struct rasops_info *);
static void	rasops_make_box_chars_32(struct rasops_info *);
static void	rasops_make_box_chars_alpha(struct rasops_info *);

#if NRASOPS_ROTATION > 0
static void	rasops_rotate_font(int *, int);
static void	rasops_copychar(void *, int, int, int, int);

/* rotate clockwise */
static void	rasops_copycols_rotated_cw(void *, int, int, int, int);
static void	rasops_copyrows_rotated_cw(void *, int, int, int);
static void	rasops_erasecols_rotated_cw(void *, int, int, int, long);
static void	rasops_eraserows_rotated_cw(void *, int, int, long);
static void	rasops_putchar_rotated_cw(void *, int, int, u_int, long);

/* rotate counter-clockwise */
static void	rasops_copychar_ccw(void *, int, int, int, int);
static void	rasops_copycols_rotated_ccw(void *, int, int, int, int);
static void	rasops_copyrows_rotated_ccw(void *, int, int, int);
#define rasops_erasecols_rotated_ccw rasops_erasecols_rotated_cw
#define rasops_eraserows_rotated_ccw rasops_eraserows_rotated_cw
static void	rasops_putchar_rotated_ccw(void *, int, int, u_int, long);

/*
 * List of all rotated fonts
 */
SLIST_HEAD(, rotatedfont) rotatedfonts = SLIST_HEAD_INITIALIZER(rotatedfonts);
struct rotatedfont {
	SLIST_ENTRY(rotatedfont) rf_next;
	int rf_cookie;
	int rf_rotated;
};
#endif	/* NRASOPS_ROTATION > 0 */

/*
 * Initialize a 'rasops_info' descriptor.
 */
int rasops_init(struct rasops_info *ri, int wantrows, int wantcols)
{
	//ser_puts("rasops_init 1\n");
	memset(&ri->ri_optfont, 0, sizeof(ri->ri_optfont));
	//ser_puts("rasops_init 2\n");
	/* Select a font if the caller doesn't care */
	if (ri->ri_font == NULL) {
		int cookie = -1;
		int flags;

		wsfont_init();

		/*
		 * first, try to find something that's as close as possible
		 * to the caller's requested terminal size
		 */ 
		if (wantrows == 0)
			wantrows = RASOPS_DEFAULT_HEIGHT;
		if (wantcols == 0)
			wantcols = RASOPS_DEFAULT_WIDTH;

		flags = WSFONT_FIND_BESTWIDTH | WSFONT_FIND_BITMAP;
		if ((ri->ri_flg & RI_ENABLE_ALPHA) != 0)
			flags |= WSFONT_FIND_ALPHA;
		if ((ri->ri_flg & RI_PREFER_ALPHA) != 0)
			flags |= WSFONT_PREFER_ALPHA;
		cookie = wsfont_find(NULL,
			ri->ri_width / wantcols,
			0,
			0,
			WSDISPLAY_FONTORDER_L2R,
			WSDISPLAY_FONTORDER_L2R,
			flags);

		/*
		 * this means there is no supported font in the list
		 */
		if (cookie <= 0) {
			printf("%s: font table is empty\n", __func__);
			return -1;
		}

#if NRASOPS_ROTATION > 0
		/*
		 * Pick the rotated version of this font. This will create it
		 * if necessary.
		 */
		if (ri->ri_flg & RI_ROTATE_MASK) {
			if (ri->ri_flg & RI_ROTATE_CW)
				rasops_rotate_font(&cookie, WSFONT_ROTATE_CW);
			else if (ri->ri_flg & RI_ROTATE_CCW)
				rasops_rotate_font(&cookie, WSFONT_ROTATE_CCW);
		}
#endif

		if (wsfont_lock(cookie, &ri->ri_font)) {
			printf("%s: couldn't lock font\n", __func__);
			return -1;
		}

		ri->ri_wsfcookie = cookie;
	}

	/* This should never happen in reality... */
	if ((uintptr_t)ri->ri_bits & 3) {
		printf("%s: bits not aligned on 32-bit boundary\n",
		    __func__);
		return -1;
	}

	if (ri->ri_stride & 3) {
		printf("%s: stride not aligned on 32-bit boundary\n",
		    __func__);
		return -1;
	}
	//ser_puts("rasops_init 3\n");

	if (rasops_reconfig(ri, wantrows, wantcols))
		return -1;

	rasops_init_devcmap(ri);
	return 0;
}

/*
 * Reconfigure (because parameters have changed in some way).
 */
int rasops_reconfig(struct rasops_info *ri, int wantrows, int wantcols)
{
	int bpp, height, s;
	size_t len;

	//ser_puts("rasops_reconfig 1\n");
	//s = splhigh();

	if (wantrows == 0)
		wantrows = RASOPS_DEFAULT_HEIGHT;
	if (wantcols == 0)
		wantcols = RASOPS_DEFAULT_WIDTH;

	/* throw away old line drawing character bitmaps, if we have any */
	if (ri->ri_optfont.data != NULL) {
		free(ri->ri_optfont.data);
		ri->ri_optfont.data = NULL;
	}

	/* autogenerate box drawing characters */
	ri->ri_optfont.firstchar = WSFONT_FLAG_OPT;
	ri->ri_optfont.numchars = 16;
	ri->ri_optfont.fontwidth = ri->ri_font->fontwidth;
	ri->ri_optfont.fontheight = ri->ri_font->fontheight;
	ri->ri_optfont.stride = ri->ri_font->stride;
	len = ri->ri_optfont.fontheight * ri->ri_optfont.stride *
	    ri->ri_optfont.numchars; 

	if (ri->ri_font->fontwidth > 32 || ri->ri_font->fontwidth < 4) {
		printf("%s: fontwidth assumptions botched", __func__);
		//splx(s);
		return -1;
	}
	//ser_puts("rasops_reconfig 2\n");

	if ((ri->ri_flg & RI_NO_AUTO) == 0) {
		ri->ri_optfont.data = malloc(len);
		if (FONT_IS_ALPHA(&ri->ri_optfont))
			rasops_make_box_chars_alpha(ri);
		else {
			switch (ri->ri_optfont.stride) {
			case 1:
				rasops_make_box_chars_8(ri);
				break;
			case 2:
				rasops_make_box_chars_16(ri);
				break;
			case 4:
				rasops_make_box_chars_32(ri);
				break;
			default:
				printf(
				    "%s: font stride assumptions botched",
				    __func__);
				//splx(s);
				return -1;
			}
		}
	} else
		memset(&ri->ri_optfont, 0, sizeof(ri->ri_optfont));

	//ser_puts("rasops_reconfig 3\n");
	/* Need this to frob the setup below */
	bpp = (ri->ri_depth == 15 ? 16 : ri->ri_depth);

	if ((ri->ri_flg & RI_CFGDONE) != 0) {
		ri->ri_bits = ri->ri_origbits;
		ri->ri_hwbits = ri->ri_hworigbits;
	}

	/* Don't care if the caller wants a hideously small console */
	if (wantrows < 10)
		wantrows = 10;

	if (wantcols < 20)
		wantcols = 20;

	/* Now constrain what they get */
	ri->ri_emuwidth = ri->ri_font->fontwidth * wantcols;
	ri->ri_emuheight = ri->ri_font->fontheight * wantrows;

	if (ri->ri_emuwidth > ri->ri_width)
		ri->ri_emuwidth = ri->ri_width;

	if (ri->ri_emuheight > ri->ri_height)
		ri->ri_emuheight = ri->ri_height;

	/* Reduce width until aligned on a 32-bit boundary */
	while ((ri->ri_emuwidth * bpp & 31) != 0)
		ri->ri_emuwidth--;

#if NRASOPS_ROTATION > 0
	if (ri->ri_flg & (RI_ROTATE_CW|RI_ROTATE_CCW)) {
		ri->ri_rows = ri->ri_emuwidth / ri->ri_font->fontwidth;
		ri->ri_cols = ri->ri_emuheight / ri->ri_font->fontheight;
	} else
#endif
	{
		ri->ri_cols = ri->ri_emuwidth / ri->ri_font->fontwidth;
		ri->ri_rows = ri->ri_emuheight / ri->ri_font->fontheight;
	}
	ri->ri_emustride = ri->ri_emuwidth * bpp >> 3;
	ri->ri_ccol = 0;
	ri->ri_crow = 0;
	ri->ri_pelbytes = bpp >> 3;

	ri->ri_xscale = (ri->ri_font->fontwidth * bpp) >> 3;
	ri->ri_yscale = ri->ri_font->fontheight * ri->ri_stride;
	ri->ri_fontscale = ri->ri_font->fontheight * ri->ri_font->stride;

	ri->ri_origbits = ri->ri_bits;
	ri->ri_hworigbits = ri->ri_hwbits;

	/* Clear the entire display */
	if ((ri->ri_flg & RI_CLEAR) != 0) {
		rasops_memset32(ri->ri_bits, 0, ri->ri_stride * ri->ri_height);
		if (ri->ri_hwbits)
			rasops_memset32(ri->ri_hwbits, 0,
			    ri->ri_stride * ri->ri_height);
	}

	//ser_puts("rasops_reconfig 4\n");
	/* Now centre our window if needs be */
	if ((ri->ri_flg & RI_CENTER) != 0) {
		uint32_t xoff, yoff;

		xoff = ((ri->ri_width * bpp >> 3) - ri->ri_emustride) >> 1;
		if (ri->ri_depth != 24) {
			/*
			 * Truncate to word boundary.
			 */
			xoff &= ~3;
		} else {
			/*
			 * Truncate to both word and 24-bit color boundary.
			 */
			xoff -= xoff % 12;
		}

		yoff = ((ri->ri_height - ri->ri_emuheight) >> 1) *
		    ri->ri_stride;

		ri->ri_bits += xoff;
		ri->ri_bits += yoff;
		if (ri->ri_hwbits != NULL) {
			ri->ri_hwbits += xoff;
			ri->ri_hwbits += yoff;
		}

		ri->ri_yorigin = (int)(ri->ri_bits - ri->ri_origbits) /
		    ri->ri_stride;
		ri->ri_xorigin = (((int)(ri->ri_bits - ri->ri_origbits) %
		    ri->ri_stride) * 8 / bpp);
	} else
		ri->ri_xorigin = ri->ri_yorigin = 0;

	/* Scaling underline by font height */
	height = ri->ri_font->fontheight;
	ri->ri_ul.off = rounddown(height, 16) / 16;	/* offset from bottom */
	ri->ri_ul.height = roundup(height, 16) / 16;	/* height */

	//ser_puts("rasops_reconfig 5\n");
	/*
	 * Fill in defaults for operations set.  XXX this nukes private
	 * routines used by accelerated fb drivers.
	 */
	ri->ri_ops.mapchar = rasops_mapchar;
	ri->ri_ops.copyrows = rasops_copyrows;
	ri->ri_ops.copycols = rasops_copycols;
	ri->ri_ops.erasecols = rasops_erasecols;
	ri->ri_ops.eraserows = rasops_eraserows;
	ri->ri_ops.cursor = rasops_cursor;
	ri->ri_do_cursor = rasops_do_cursor;

	ri->ri_caps &= ~(WSSCREEN_UNDERLINE | WSSCREEN_HILIT |
		    WSSCREEN_WSCOLORS | WSSCREEN_REVERSE);

	if ((ri->ri_flg & RI_FORCEMONO) != 0 ||
#ifndef RASOPS_APPLE_PALETTE
	    ri->ri_depth < 8
#else
	    ri->ri_depth < 4
#endif
	) {
		ri->ri_ops.allocattr = rasops_allocattr_mono;
		ri->ri_caps |= WSSCREEN_UNDERLINE | WSSCREEN_REVERSE;
	} else {
		ri->ri_ops.allocattr = rasops_allocattr_color;
		ri->ri_caps |= WSSCREEN_UNDERLINE | WSSCREEN_HILIT |
		    WSSCREEN_WSCOLORS | WSSCREEN_REVERSE;
	}

	switch (ri->ri_depth) {
#if NRASOPS1 > 0
	case 1:
		rasops1_init(ri);
		break;
#endif
#if NRASOPS2 > 0
	case 2:
		rasops2_init(ri);
		break;
#endif
#if NRASOPS4 > 0
	case 4:
		rasops4_init(ri);
		break;
#endif
#if NRASOPS8 > 0
	case 8:
		rasops8_init(ri);
		break;
#endif
#if (NRASOPS15 + NRASOPS16) > 0
	case 15:
	case 16:
		rasops15_init(ri);
		break;
#endif
#if NRASOPS24 > 0
	case 24:
		rasops24_init(ri);
		break;
#endif
#if NRASOPS32 > 0
	case 32:
		rasops32_init(ri);
		break;
#endif
	default:
		ri->ri_flg &= ~RI_CFGDONE;
		printf("%s: depth not supported\n", __func__);
		//splx(s);
		return -1;
	}
	//ser_puts("rasops_reconfig 6\n");

#if NRASOPS_ROTATION > 0
	if (ri->ri_flg & RI_ROTATE_MASK) {
		if (ri->ri_flg & RI_ROTATE_CW) {
			ri->ri_real_ops = ri->ri_ops;
			ri->ri_ops.copycols = rasops_copycols_rotated_cw;
			ri->ri_ops.copyrows = rasops_copyrows_rotated_cw;
			ri->ri_ops.erasecols = rasops_erasecols_rotated_cw;
			ri->ri_ops.eraserows = rasops_eraserows_rotated_cw;
			ri->ri_ops.putchar = rasops_putchar_rotated_cw;
		} else if (ri->ri_flg & RI_ROTATE_CCW) {
			ri->ri_real_ops = ri->ri_ops;
			ri->ri_ops.copycols = rasops_copycols_rotated_ccw;
			ri->ri_ops.copyrows = rasops_copyrows_rotated_ccw;
			ri->ri_ops.erasecols = rasops_erasecols_rotated_ccw;
			ri->ri_ops.eraserows = rasops_eraserows_rotated_ccw;
			ri->ri_ops.putchar = rasops_putchar_rotated_ccw;
		}
	}
#endif

	ri->ri_flg |= RI_CFGDONE;
	//splx(s);
	return 0;
}

/*
 * Map a character.
 */
static int rasops_mapchar(void *cookie, int c, u_int *cp)
{
	struct rasops_info *ri = (struct rasops_info *)cookie;

	assert(ri->ri_font != NULL);

	if ((c = wsfont_map_unichar(ri->ri_font, c)) < 0 ||
	    !CHAR_IN_FONT(c, ri->ri_font)) {
		*cp = ' ';
		return 0;
	}

	*cp = c;
	return 5;
}

/*
 * Allocate a color attribute.
 */
static int rasops_allocattr_color(void *cookie, int fg0, int bg0, int flg, long *attr)
{
	uint32_t fg = fg0, bg = bg0;

	if (__predict_false(fg >= sizeof(rasops_isgray) ||
	    bg >= sizeof(rasops_isgray)))
		return EINVAL;

#ifdef RASOPS_CLIPPING
	fg &= 7;
	bg &= 7;
#endif

	if ((flg & WSATTR_BLINK) != 0)
		return EINVAL;

	if ((flg & WSATTR_WSCOLORS) == 0) {
#ifdef WS_DEFAULT_FG
		fg = WS_DEFAULT_FG;
#else
		fg = WSCOL_WHITE;
#endif
#ifdef WS_DEFAULT_BG
		bg = WS_DEFAULT_BG;
#else	
		bg = WSCOL_BLACK;
#endif
	}

	if ((flg & WSATTR_HILIT) != 0 && fg < 8)
		fg += 8;

	if ((flg & WSATTR_REVERSE) != 0) {
		uint32_t swap = fg;
		fg = bg;
		bg = swap;
	}

	flg &= WSATTR_USERMASK;

	if (rasops_isgray[fg])
		flg |= WSATTR_PRIVATE1;

	if (rasops_isgray[bg])
		flg |= WSATTR_PRIVATE2;

	*attr = (bg << 16) | (fg << 24) | flg;
	return 0;
}

/*
 * Allocate a mono attribute.
 */
static int rasops_allocattr_mono(void *cookie, int fg0, int bg0, int flg, long *attr)
{
	uint32_t fg = fg0, bg = bg0;

	if ((flg & (WSATTR_BLINK | WSATTR_HILIT | WSATTR_WSCOLORS)) != 0)
		return EINVAL;

	fg = 0xff;
	bg = 0;

	if ((flg & WSATTR_REVERSE) != 0) {
		uint32_t swap = fg;
		fg = bg;
		bg = swap;
	}

	*attr = (bg << 16) | (fg << 24) | flg;
	return 0;
}

/*
 * Copy rows.
 */
static void rasops_copyrows(void *cookie, int src, int dst, int num)
{
	struct rasops_info *ri = (struct rasops_info *)cookie;
	int stride;
	uint8_t *sp, *dp, *hp;

	hp = NULL;	/* XXX GCC */

	if (__predict_false(dst == src))
		return;

#ifdef RASOPS_CLIPPING
	if (src < 0) {
		num += src;
		src = 0;
	}

	if (src + num > ri->ri_rows)
		num = ri->ri_rows - src;

	if (dst < 0) {
		num += dst;
		dst = 0;
	}

	if (dst + num > ri->ri_rows)
		num = ri->ri_rows - dst;

	if (num <= 0)
		return;
#endif

	src *= ri->ri_yscale;
	dst *= ri->ri_yscale;
	num *= ri->ri_font->fontheight;
	stride = ri->ri_stride;

	if (src < dst) {
		/* backward copy */
		src += (num - 1) * stride;
		dst += (num - 1) * stride;
		stride *= -1;
	}

	sp = ri->ri_bits + src;
	dp = ri->ri_bits + dst;
	if (ri->ri_hwbits)
		hp = ri->ri_hwbits + dst;

	while (num--) {
		memcpy(dp, sp, ri->ri_emustride);
		if (ri->ri_hwbits) {
			memcpy(hp, dp, ri->ri_emustride);
			hp += stride;
		}
		sp += stride;
		dp += stride;
	}
}

/*
 * Copy columns. This is slow, and hard to optimize due to alignment,
 * and the fact that we have to copy both left->right and right->left.
 * We simply cop-out here and use memmove(), since it handles all of
 * these cases anyway.
 */
static void rasops_copycols(void *cookie, int row, int src, int dst, int num)
{
	struct rasops_info *ri = (struct rasops_info *)cookie;
	int height;
	uint8_t *sp, *dp, *hp;

	hp = NULL;	/* XXX GCC */

	if (__predict_false(dst == src))
		return;

#ifdef RASOPS_CLIPPING
	/* Catches < 0 case too */
	if ((unsigned)row >= (unsigned)ri->ri_rows)
		return;

	if (src < 0) {
		num += src;
		src = 0;
	}

	if (src + num > ri->ri_cols)
		num = ri->ri_cols - src;

	if (dst < 0) {
		num += dst;
		dst = 0;
	}

	if (dst + num > ri->ri_cols)
		num = ri->ri_cols - dst;

	if (num <= 0)
		return;
#endif

	height = ri->ri_font->fontheight;
	row *= ri->ri_yscale;
	num *= ri->ri_xscale;

	sp = ri->ri_bits + row + src * ri->ri_xscale;
	dp = ri->ri_bits + row + dst * ri->ri_xscale;
	if (ri->ri_hwbits)
		hp = ri->ri_hwbits + row + dst * ri->ri_xscale;

	while (height--) {
		memmove(dp, sp, num);
		if (ri->ri_hwbits) {
			memcpy(hp, dp, num);
			hp += ri->ri_stride;
		}
		sp += ri->ri_stride;
		dp += ri->ri_stride;
	}
}

/*
 * Turn cursor off/on.
 */
static void rasops_cursor(void *cookie, int on, int row, int col)
{
	struct rasops_info *ri = (struct rasops_info *)cookie;

	/* Turn old cursor off */
	if ((ri->ri_flg & RI_CURSOR) != 0)
#ifdef RASOPS_CLIPPING
		if ((ri->ri_flg & RI_CURSORCLIP) == 0)
#endif
			ri->ri_do_cursor(ri);

	/* Select new cursor */
	ri->ri_crow = row;
	ri->ri_ccol = col;

#ifdef RASOPS_CLIPPING
	ri->ri_flg &= ~RI_CURSORCLIP;
	if (row < 0 || row >= ri->ri_rows)
		ri->ri_flg |= RI_CURSORCLIP;
	else if (col < 0 || col >= ri->ri_cols)
		ri->ri_flg |= RI_CURSORCLIP;
#endif

	if (on) {
		ri->ri_flg |= RI_CURSOR;
#ifdef RASOPS_CLIPPING
		if ((ri->ri_flg & RI_CURSORCLIP) == 0)
#endif
			ri->ri_do_cursor(ri);
	} else
		ri->ri_flg &= ~RI_CURSOR;
}

/*
 * Make the device colormap
 */
static void rasops_init_devcmap(struct rasops_info *ri)
{
	int i;
	uint32_t c;
	const uint8_t *p;

	switch (ri->ri_depth) {
	case 1:
		ri->ri_devcmap[0] = 0;
		for (i = 1; i < 16; i++)
			ri->ri_devcmap[i] = -1;
		return;

	case 2:
		for (i = 1; i < 15; i++)
			ri->ri_devcmap[i] = 0xaaaaaaaa;

		ri->ri_devcmap[0] = 0;
		ri->ri_devcmap[8] = 0x55555555;
		ri->ri_devcmap[15] = -1;
		return;

	case 4:
		for (i = 0; i < 16; i++) {
#ifdef RASOPS_APPLE_PALETTE
			c = apple4_devcmap[i];
#else
			c = i;
#endif
			ri->ri_devcmap[i] =
			    (c <<  0) | (c <<  4) | (c <<  8) | (c << 12) |
			    (c << 16) | (c << 20) | (c << 24) | (c << 28);
		}
		return;

	case 8:
		if ((ri->ri_flg & RI_8BIT_IS_RGB) == 0) {
			for (i = 0; i < 16; i++) {
#ifdef RASOPS_APPLE_PALETTE
				c = apple8_devcmap[i];
#else
				c = i;
#endif
				ri->ri_devcmap[i] =
				    c | (c << 8) | (c << 16) | (c << 24);
			}
			return;
		}
	}

	p = rasops_cmap;

	for (i = 0; i < 16; i++) {
		if (ri->ri_rnum <= 8)
			c = (uint32_t)(*p >> (8 - ri->ri_rnum)) << ri->ri_rpos;
		else
			c = (uint32_t)(*p << (ri->ri_rnum - 8)) << ri->ri_rpos;
		p++;

		if (ri->ri_gnum <= 8)
			c |= (uint32_t)(*p >> (8 - ri->ri_gnum)) << ri->ri_gpos;
		else
			c |= (uint32_t)(*p << (ri->ri_gnum - 8)) << ri->ri_gpos;
		p++;

		if (ri->ri_bnum <= 8)
			c |= (uint32_t)(*p >> (8 - ri->ri_bnum)) << ri->ri_bpos;
		else
			c |= (uint32_t)(*p << (ri->ri_bnum - 8)) << ri->ri_bpos;
		p++;

		/*
		 * Swap byte order if necessary. Then, fill the word for
		 * generic routines, which want this.
		 */
		switch (ri->ri_depth) {
		case 8:
			c |= c << 8;
			c |= c << 16;
			break;
		case 15:
		case 16:
			if ((ri->ri_flg & RI_BSWAP) != 0)
				c = bswap16(c);
			c |= c << 16;
			break;
		case 24:
#if BYTE_ORDER == LITTLE_ENDIAN
			if ((ri->ri_flg & RI_BSWAP) == 0)
#else
			if ((ri->ri_flg & RI_BSWAP) != 0)
#endif
			{
				/*
				 * Convert to ``big endian'' if not RI_BSWAP.
				 */
				c = (c & 0x0000ff) << 16|
				    (c & 0x00ff00) |
				    (c & 0xff0000) >> 16;
			}
			/*
			 * No worries, we use generic routines only for
			 * gray colors, where all 3 bytes are same.
			 */
			c |= (c & 0xff) << 24;
			break;
		case 32:
			if ((ri->ri_flg & RI_BSWAP) != 0)
				c = bswap32(c);
			break;
		}

		ri->ri_devcmap[i] = c;
	}
}

/*
 * Unpack a rasops attribute
 */
void rasops_unpack_attr(long attr, int *fg, int *bg, int *underline)
{

	*fg = ((uint32_t)attr >> 24) & 0xf;
	*bg = ((uint32_t)attr >> 16) & 0xf;
	if (underline != NULL)
		*underline = (uint32_t)attr & WSATTR_UNDERLINE;
}

/*
 * Erase rows. This isn't static, since 24-bpp uses it in special cases.
 */
void rasops_eraserows(void *cookie, int row, int num, long attr)
{
	struct rasops_info *ri = (struct rasops_info *)cookie;
	int bytes;
	uint32_t bg, *rp, *hp;

	hp = NULL;	/* XXX GCC */

#ifdef RASOPS_CLIPPING
	if (row < 0) {
		num += row;
		row = 0;
	}

	if (row + num > ri->ri_rows)
		num = ri->ri_rows - row;

	if (num <= 0)
		return;
#endif

	/*
	 * XXX The wsdisplay_emulops interface seems a little deficient in
	 * that there is no way to clear the *entire* screen. We provide a
	 * workaround here: if the entire console area is being cleared, and
	 * the RI_FULLCLEAR flag is set, clear the entire display.
	 */
	if (num == ri->ri_rows && (ri->ri_flg & RI_FULLCLEAR) != 0) {
		bytes = ri->ri_stride;
		num = ri->ri_height;
		rp = (uint32_t *)ri->ri_origbits;
		if (ri->ri_hwbits)
			hp = (uint32_t *)ri->ri_hworigbits;
	} else {
		bytes = ri->ri_emustride;
		num *= ri->ri_font->fontheight;
		rp = (uint32_t *)(ri->ri_bits + row * ri->ri_yscale);
		if (ri->ri_hwbits)
			hp = (uint32_t *)(ri->ri_hwbits + row * ri->ri_yscale);
	}

	bg = ATTR_BG(ri, attr);

	while (num--) {
		rasops_memset32(rp, bg, bytes);
		if (ri->ri_hwbits) {
			memcpy(hp, rp, bytes);
			DELTA(hp, ri->ri_stride, uint32_t *);
		}
		DELTA(rp, ri->ri_stride, uint32_t *);
	}
}

/*
 * Actually turn the cursor on or off. This does the dirty work for
 * rasops_cursor().
 */
static void rasops_do_cursor(struct rasops_info *ri)
{
	int row, col, height, slop1, slop2, full, cnt;
	uint32_t mask1, mask2, *dp;
	uint8_t tmp, *rp, *hp;

	hp = NULL;	/* XXX GCC */

#if NRASOPS_ROTATION > 0
	if (ri->ri_flg & RI_ROTATE_MASK) {
		if (ri->ri_flg & RI_ROTATE_CW) {
			/* Rotate rows/columns */
			row = ri->ri_ccol;
			col = ri->ri_rows - ri->ri_crow - 1;
		} else if (ri->ri_flg & RI_ROTATE_CCW) {
			/* Rotate rows/columns */
			row = ri->ri_cols - ri->ri_ccol - 1;
			col = ri->ri_crow;
		} else {	/* upside-down */
			row = ri->ri_crow;
			col = ri->ri_ccol;
		}
	} else
#endif
	{
		row = ri->ri_crow;
		col = ri->ri_ccol;
	}

	height = ri->ri_font->fontheight;

	rp = ri->ri_bits + FBOFFSET(ri, row, col);
	if (ri->ri_hwbits)
		hp = ri->ri_hwbits + FBOFFSET(ri, row, col);

	/*
	 * For ri_xscale = 1:
	 *
	 * Logic below does not work for ri_xscale = 1, e.g.,
	 * fontwidth = 8 and bpp = 1. So we take care of it.
	 */
	if (ri->ri_xscale == 1) {
		while (height--) {
			tmp = ~*rp;
			*rp = tmp;
			if (ri->ri_hwbits) {
				*hp = tmp;
				hp += ri->ri_stride;
			}
			rp += ri->ri_stride;
		}
		return;
	}

	/*
	 * For ri_xscale = 2, 3, 4, ...:
	 *
	 * Note that siop1 <= ri_xscale even for ri_xscale = 2,
	 * since rp % 3 = 0 or 2 (ri_stride % 4 = 0).
	 */
	slop1 = (4 - ((uintptr_t)rp & 3)) & 3;
	slop2 = (ri->ri_xscale - slop1) & 3;
	full = (ri->ri_xscale - slop1 /* - slop2 */) >> 2;

	rp = (uint8_t *)((uintptr_t)rp & ~3);
	hp = (uint8_t *)((uintptr_t)hp & ~3);

	mask1 = rasops_lmask32[4 - slop1];
	mask2 = rasops_rmask32[slop2];

	while (height--) {
		dp = (uint32_t *)rp;

		if (slop1) {
			*dp = *dp ^ mask1;
			dp++;
		}

		for (cnt = full; cnt; cnt--) {
			*dp = ~*(uint32_t *)dp;
			dp++;
		}

		if (slop2)
			*dp = *dp ^ mask2;

		if (ri->ri_hwbits) {
			memcpy(hp, rp, ((slop1 != 0) + full +
			    (slop2 != 0)) << 2);
			hp += ri->ri_stride;
		}
		rp += ri->ri_stride;
	}
}

/*
 * Erase columns.
 */
void rasops_erasecols(void *cookie, int row, int col, int num, long attr)
{
	struct rasops_info *ri = (struct rasops_info *)cookie;
	int height;
	uint32_t bg, *rp, *hp;

	hp = NULL;	/* XXX GCC */

#ifdef RASOPS_CLIPPING
	if ((unsigned)row >= (unsigned)ri->ri_rows)
		return;

	if (col < 0) {
		num += col;
		col = 0;
	}

	if (col + num > ri->ri_cols)
		num = ri->ri_cols - col;

	if (num <= 0)
		return;
#endif

	height = ri->ri_font->fontheight;
	num *= ri->ri_xscale;

	rp = (uint32_t *)(ri->ri_bits + FBOFFSET(ri, row, col));
	if (ri->ri_hwbits)
		hp = (uint32_t *)(ri->ri_hwbits + FBOFFSET(ri, row, col));

	bg = ATTR_BG(ri, attr);

	while (height--) {
		rasops_memset32(rp, bg, num);
		if (ri->ri_hwbits) {
			memcpy(hp, rp, num);
			DELTA(hp, ri->ri_stride, uint32_t *);
		}
		DELTA(rp, ri->ri_stride, uint32_t *);
	}
}

void rasops_make_box_chars_16(struct rasops_info *ri)
{
	int c, i, mid;
	uint16_t vert_mask, hmask_left, hmask_right;
	uint16_t *data = (uint16_t *)ri->ri_optfont.data;

	vert_mask = 0xc000U >> ((ri->ri_font->fontwidth >> 1) - 1);
	hmask_left = 0xff00U << (8 - (ri->ri_font->fontwidth >> 1));
	hmask_right = hmask_left >> ((ri->ri_font->fontwidth + 1) >> 1);
	mid = (ri->ri_font->fontheight + 1) >> 1;

	/* 0x00 would be empty anyway so don't bother */
	for (c = 1; c < 16; c++) {
		data += ri->ri_font->fontheight;
		if (c & 1) {
			/* upper segment */
			for (i = 0; i < mid; i++)
				data[i] = vert_mask;
		}
		if (c & 4) {
			/* lower segment */
			for (i = mid; i < ri->ri_font->fontheight; i++)
				data[i] = vert_mask;
		}
		if (c & 2) {
			/* right segment */
			i = ri->ri_font->fontheight >> 1;
			data[mid - 1] |= hmask_right;
			data[mid] |= hmask_right;
		}
		if (c & 8) {
			/* left segment */
			data[mid - 1] |= hmask_left;
			data[mid] |= hmask_left;
		}
	}
}

void rasops_make_box_chars_8(struct rasops_info *ri)
{
	int c, i, mid;
	uint8_t vert_mask, hmask_left, hmask_right;
	uint8_t *data = (uint8_t *)ri->ri_optfont.data;

	vert_mask = 0xc0U >> ((ri->ri_font->fontwidth >> 1) - 1);
	hmask_left = 0xf0U << (4 - (ri->ri_font->fontwidth >> 1));
	hmask_right = hmask_left >> ((ri->ri_font->fontwidth + 1) >> 1);
	mid = (ri->ri_font->fontheight + 1) >> 1;

	/* 0x00 would be empty anyway so don't bother */
	for (c = 1; c < 16; c++) {
		data += ri->ri_font->fontheight;
		if (c & 1) {
			/* upper segment */
			for (i = 0; i < mid; i++)
				data[i] = vert_mask;
		}
		if (c & 4) {
			/* lower segment */
			for (i = mid; i < ri->ri_font->fontheight; i++)
				data[i] = vert_mask;
		}
		if (c & 2) {
			/* right segment */
			i = ri->ri_font->fontheight >> 1;
			data[mid - 1] |= hmask_right;
			data[mid] |= hmask_right;
		}
		if (c & 8) {
			/* left segment */
			data[mid - 1] |= hmask_left;
			data[mid] |= hmask_left;
		}
	}
}

void rasops_make_box_chars_32(struct rasops_info *ri)
{
	int c, i, mid;
	uint32_t vert_mask, hmask_left, hmask_right;
	uint32_t *data = (uint32_t *)ri->ri_optfont.data;

	vert_mask = 0xc0000000U >> ((ri->ri_font->fontwidth >> 1) - 1);
	hmask_left = 0xffff0000U << (16 - (ri->ri_font->fontwidth >> 1));
	hmask_right = hmask_left >> ((ri->ri_font->fontwidth + 1) >> 1);
	mid = (ri->ri_font->fontheight + 1) >> 1;

	/* 0x00 would be empty anyway so don't bother */
	for (c = 1; c < 16; c++) {
		data += ri->ri_font->fontheight;
		if (c & 1) {
			/* upper segment */
			for (i = 0; i < mid; i++)
				data[i] = vert_mask;
		}
		if (c & 4) {
			/* lower segment */
			for (i = mid; i < ri->ri_font->fontheight; i++)
				data[i] = vert_mask;
		}
		if (c & 2) {
			/* right segment */
			i = ri->ri_font->fontheight >> 1;
			data[mid - 1] |= hmask_right;
			data[mid] |= hmask_right;
		}
		if (c & 8) {
			/* left segment */
			data[mid - 1] |= hmask_left;
			data[mid] |= hmask_left;
		}
	}
}

void rasops_make_box_chars_alpha(struct rasops_info *ri)
{
	int c, i, hmid, vmid, wi, he;
	uint8_t *data = (uint8_t *)ri->ri_optfont.data;
	uint8_t *ddata;

	he = ri->ri_font->fontheight;
	wi = ri->ri_font->fontwidth;
	
	vmid = (he + 1) >> 1;
	hmid = (wi + 1) >> 1;

	/* 0x00 would be empty anyway so don't bother */
	for (c = 1; c < 16; c++) {
		data += ri->ri_fontscale;
		if (c & 1) {
			/* upper segment */
			ddata = data + hmid;
			for (i = 0; i <= vmid; i++) {
				*ddata = 0xff;
				ddata += wi;
			}
		}
		if (c & 4) {
			/* lower segment */
			ddata = data + wi * vmid + hmid;
			for (i = vmid; i < he; i++) {
				*ddata = 0xff;
				ddata += wi;
			}
		}
		if (c & 2) {
			/* right segment */
			ddata = data + wi * vmid + hmid;
			for (i = hmid; i < wi; i++) {
				*ddata = 0xff;
				ddata++;
			}
		}
		if (c & 8) {
			/* left segment */
			ddata = data + wi * vmid;
			for (i = 0; i <= hmid; i++) {
				*ddata = 0xff;
				ddata++;
			}
		}
	}
}
 
/*
 * Return a colour map appropriate for the given struct rasops_info in the
 * same form used by rasops_cmap[]
 * For now this is either a copy of rasops_cmap[] or an R3G3B2 map, it should
 * probably be a linear ( or gamma corrected? ) ramp for higher depths.
 */
int rasops_get_cmap(struct rasops_info *ri, uint8_t *palette, size_t bytes)
{

	if ((ri->ri_depth == 8) && ((ri->ri_flg & RI_8BIT_IS_RGB) != 0)) {
		/* generate an R3G3B2 palette */
		int i, idx = 0;
		uint8_t tmp;

		if (bytes < 256 * 3)
			return EINVAL;
		for (i = 0; i < 256; i++) {
			tmp = i & 0xe0;
			/*
			 * replicate bits so 0xe0 maps to a red value of 0xff
			 * in order to make white look actually white
			 */
			tmp |= (tmp >> 3) | (tmp >> 6);
			palette[idx] = tmp;
			idx++;

			tmp = (i & 0x1c) << 3;
			tmp |= (tmp >> 3) | (tmp >> 6);
			palette[idx] = tmp;
			idx++;

			tmp = (i & 0x03) << 6;
			tmp |= tmp >> 2;
			tmp |= tmp >> 4;
			palette[idx] = tmp;
			idx++;
		}
	} else
		memcpy(palette, rasops_cmap, uimin(bytes, sizeof(rasops_cmap)));

	return 0;
}

#if NRASOPS_ROTATION > 0
/*
 * Quarter clockwise rotation routines (originally intended for the
 * built-in Zaurus C3x00 display in 16bpp).
 */

static void rasops_rotate_font(int *cookie, int rotate)
{
	struct rotatedfont *f;
	int ncookie;

	SLIST_FOREACH(f, &rotatedfonts, rf_next) {
		if (f->rf_cookie == *cookie) {
			*cookie = f->rf_rotated;
			return;
		}
	}

	/*
	 * We did not find a rotated version of this font. Ask the wsfont
	 * code to compute one for us.
	 */

	f = kmem_alloc(sizeof(*f), KM_SLEEP);

	if ((ncookie = wsfont_rotate(*cookie, rotate)) == -1)
		goto fail;

	f->rf_cookie = *cookie;
	f->rf_rotated = ncookie;
	SLIST_INSERT_HEAD(&rotatedfonts, f, rf_next);

	*cookie = ncookie;
	return;

fail:	kmem_free(f, sizeof(*f));
	return;
}

static void rasops_copychar(void *cookie, int srcrow, int dstrow, int srccol, int dstcol)
{
	struct rasops_info *ri = (struct rasops_info *)cookie;
	int r_srcrow, r_dstrow, r_srccol, r_dstcol, height;
	uint8_t *sp, *dp;

	r_srcrow = srccol;
	r_dstrow = dstcol;
	r_srccol = ri->ri_rows - srcrow - 1;
	r_dstcol = ri->ri_rows - dstrow - 1;

	r_srcrow *= ri->ri_yscale;
	r_dstrow *= ri->ri_yscale;
	height = ri->ri_font->fontheight;

	sp = ri->ri_bits + r_srcrow + r_srccol * ri->ri_xscale;
	dp = ri->ri_bits + r_dstrow + r_dstcol * ri->ri_xscale;

	while (height--) {
		memmove(dp, sp, ri->ri_xscale);
		dp += ri->ri_stride;
		sp += ri->ri_stride;
	}
}

static void rasops_putchar_rotated_cw(void *cookie, int row, int col, u_int uc, long attr)
{
	struct rasops_info *ri = (struct rasops_info *)cookie;
	int height;
	uint16_t fg, *rp;

	if (__predict_false((unsigned int)row > ri->ri_rows ||
	    (unsigned int)col > ri->ri_cols))
		return;

	/* Avoid underflow */
	if (ri->ri_rows - row - 1 < 0)
		return;

	/* Do rotated char sans (side)underline */
	ri->ri_real_ops.putchar(cookie, col, ri->ri_rows - row - 1, uc,
	    attr & ~WSATTR_UNDERLINE);

	/*
	 * Do rotated underline
	 * XXX this assumes 16-bit color depth
	 */
	if ((attr & WSATTR_UNDERLINE) != 0) {
		height = ri->ri_font->fontheight;

		rp = (uint16_t *)(ri->ri_bits + col * ri->ri_yscale +
		    (ri->ri_rows - row - 1) * ri->ri_xscale);

		fg = (uint16_t)ATTR_FG(ri, attr);

		while (height--) {
			*rp = fg;
			DELTA(rp, ri->ri_stride, uint16_t *);
		}
	}
}

static void rasops_erasecols_rotated_cw(void *cookie, int row, int col, int num, long attr)
{
	struct rasops_info *ri = (struct rasops_info *)cookie;
	int i;

	for (i = col; i < col + num; i++)
		ri->ri_ops.putchar(cookie, row, i, ' ', attr);
}

/* XXX: these could likely be optimised somewhat. */
static void rasops_copyrows_rotated_cw(void *cookie, int src, int dst, int num)
{
	struct rasops_info *ri = (struct rasops_info *)cookie;
	int col, roff;

	if (src > dst)
		for (roff = 0; roff < num; roff++)
			for (col = 0; col < ri->ri_cols; col++)
				rasops_copychar(cookie, src + roff, dst + roff,
				    col, col);
	else
		for (roff = num - 1; roff >= 0; roff--)
			for (col = 0; col < ri->ri_cols; col++)
				rasops_copychar(cookie, src + roff, dst + roff,
				    col, col);
}

static void rasops_copycols_rotated_cw(void *cookie, int row, int src, int dst, int num)
{
	int coff;

	if (src > dst)
		for (coff = 0; coff < num; coff++)
			rasops_copychar(cookie, row, row, src + coff,
			    dst + coff);
	else
		for (coff = num - 1; coff >= 0; coff--)
			rasops_copychar(cookie, row, row, src + coff,
			    dst + coff);
}

static void rasops_eraserows_rotated_cw(void *cookie, int row, int num, long attr)
{
	struct rasops_info *ri = (struct rasops_info *)cookie;
	int col, rn;

	for (rn = row; rn < row + num; rn++)
		for (col = 0; col < ri->ri_cols; col++)
			ri->ri_ops.putchar(cookie, rn, col, ' ', attr);
}

/*
 * Quarter counter-clockwise rotation routines (originally intended for the
 * built-in Sharp W-ZERO3 display in 16bpp).
 */
static void rasops_copychar_ccw(void *cookie, int srcrow, int dstrow, int srccol,
    int dstcol)
{
	struct rasops_info *ri = (struct rasops_info *)cookie;
	int r_srcrow, r_dstrow, r_srccol, r_dstcol, height;
	uint8_t *sp, *dp;

	r_srcrow = ri->ri_cols - srccol - 1;
	r_dstrow = ri->ri_cols - dstcol - 1;
	r_srccol = srcrow;
	r_dstcol = dstrow;

	r_srcrow *= ri->ri_yscale;
	r_dstrow *= ri->ri_yscale;
	height = ri->ri_font->fontheight;

	sp = ri->ri_bits + r_srcrow + r_srccol * ri->ri_xscale;
	dp = ri->ri_bits + r_dstrow + r_dstcol * ri->ri_xscale;

	while (height--) {
		memmove(dp, sp, ri->ri_xscale);
		dp += ri->ri_stride;
		sp += ri->ri_stride;
	}
}

static void rasops_putchar_rotated_ccw(void *cookie, int row, int col, u_int uc, long attr)
{
	struct rasops_info *ri = (struct rasops_info *)cookie;
	int height;
	uint16_t fg, *rp;

	if (__predict_false((unsigned int)row > ri->ri_rows ||
	    (unsigned int)col > ri->ri_cols))
		return;

	/* Avoid underflow */
	if (ri->ri_cols - col - 1 < 0)
		return;

	/* Do rotated char sans (side)underline */
	ri->ri_real_ops.putchar(cookie, ri->ri_cols - col - 1, row, uc,
	    attr & ~WSATTR_UNDERLINE);

	/*
	 * Do rotated underline
	 * XXX this assumes 16-bit color depth
	 */
	if ((attr & WSATTR_UNDERLINE) != 0) {
		height = ri->ri_font->fontheight;

		rp = (uint16_t *)(ri->ri_bits +
		    (ri->ri_cols - col - 1) * ri->ri_yscale +
		    row * ri->ri_xscale +
		    (ri->ri_font->fontwidth - 1) * ri->ri_pelbytes);

		fg = (uint16_t)ATTR_FG(ri, attr);

		while (height--) {
			*rp = fg;
			DELTA(rp, ri->ri_stride, uint16_t *);
		}
	}
}

/* XXX: these could likely be optimised somewhat. */
static void rasops_copyrows_rotated_ccw(void *cookie, int src, int dst, int num)
{
	struct rasops_info *ri = (struct rasops_info *)cookie;
	int col, roff;

	if (src > dst)
		for (roff = 0; roff < num; roff++)
			for (col = 0; col < ri->ri_cols; col++)
				rasops_copychar_ccw(cookie,
				    src + roff, dst + roff, col, col);
	else
		for (roff = num - 1; roff >= 0; roff--)
			for (col = 0; col < ri->ri_cols; col++)
				rasops_copychar_ccw(cookie,
				    src + roff, dst + roff, col, col);
}

static void rasops_copycols_rotated_ccw(void *cookie, int row, int src, int dst, int num)
{
	int coff;

	if (src > dst)
		for (coff = 0; coff < num; coff++)
			rasops_copychar_ccw(cookie, row, row,
			    src + coff, dst + coff);
	else
		for (coff = num - 1; coff >= 0; coff--)
			rasops_copychar_ccw(cookie, row, row,
			    src + coff, dst + coff);
}
#endif	/* NRASOPS_ROTATION */
