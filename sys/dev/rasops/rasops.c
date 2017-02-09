/*	 $NetBSD: rasops.c,v 1.73 2015/04/18 11:23:58 mlelstv Exp $	*/

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
__KERNEL_RCSID(0, "$NetBSD: rasops.c,v 1.73 2015/04/18 11:23:58 mlelstv Exp $");

#include "opt_rasops.h"
#include "rasops_glue.h"
#include "opt_wsmsgattrs.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/kmem.h>

#include <sys/bswap.h>
#include <machine/endian.h>

#include <dev/wscons/wsdisplayvar.h>
#include <dev/wscons/wsconsio.h>
#include <dev/wsfont/wsfont.h>
#include <dev/rasops/rasops.h>

#ifndef _KERNEL
#include <errno.h>
#endif

#ifdef RASOPS_DEBUG
#define DPRINTF aprint_error
#else
#define DPRINTF while (0) printf
#endif

struct rasops_matchdata {
	struct rasops_info *ri;
	int wantcols, wantrows;
	int bestscore;
	struct wsdisplay_font *pick;
	int ident;
};	

/* ANSI colormap (R,G,B). Upper 8 are high-intensity */
const u_char rasops_cmap[256*3] = {
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
const u_char rasops_isgray[16] = {
	1, 0, 0, 0,
	0, 0, 0, 1,
	1, 0, 0, 0,
	0, 0, 0, 1
};

/* Generic functions */
static void	rasops_copyrows(void *, int, int, int);
static int	rasops_mapchar(void *, int, u_int *);
static void	rasops_cursor(void *, int, int, int);
static int	rasops_allocattr_color(void *, int, int, int, long *);
static int	rasops_allocattr_mono(void *, int, int, int, long *);
static void	rasops_do_cursor(struct rasops_info *);
static void	rasops_init_devcmap(struct rasops_info *);

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

void	rasops_make_box_chars_8(struct rasops_info *);
void	rasops_make_box_chars_16(struct rasops_info *);
void	rasops_make_box_chars_32(struct rasops_info *);
void	rasops_make_box_chars_alpha(struct rasops_info *);

extern int cold;

/*
 * Initialize a 'rasops_info' descriptor.
 */
int
rasops_init(struct rasops_info *ri, int wantrows, int wantcols)
{

	memset (&ri->ri_optfont, 0, sizeof(ri->ri_optfont));
#ifdef _KERNEL
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
			aprint_error("rasops_init: font table is empty\n");
			return (-1);
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
			aprint_error("rasops_init: couldn't lock font\n");
			return (-1);
		}

		ri->ri_wsfcookie = cookie;
	}
#endif

	/* This should never happen in reality... */
#ifdef DEBUG
	if ((long)ri->ri_bits & 3) {
		aprint_error("rasops_init: bits not aligned on 32-bit boundary\n");
		return (-1);
	}

	if ((int)ri->ri_stride & 3) {
		aprint_error("rasops_init: stride not aligned on 32-bit boundary\n");
		return (-1);
	}
#endif

	if (rasops_reconfig(ri, wantrows, wantcols))
		return (-1);

	rasops_init_devcmap(ri);
	return (0);
}

/*
 * Reconfigure (because parameters have changed in some way).
 */
int
rasops_reconfig(struct rasops_info *ri, int wantrows, int wantcols)
{
	int bpp, s, len;

	s = splhigh();

	if (wantrows == 0)
		wantrows = RASOPS_DEFAULT_HEIGHT;
	if (wantcols == 0)
		wantcols = RASOPS_DEFAULT_WIDTH;

	/* throw away old line drawing character bitmaps, if we have any */
	if (ri->ri_optfont.data != NULL) {
		kmem_free(ri->ri_optfont.data, ri->ri_optfont.stride * 
		    ri->ri_optfont.fontheight * ri->ri_optfont.numchars);
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

	if (((ri->ri_flg & RI_NO_AUTO) == 0) && 
	  ((ri->ri_optfont.data = kmem_zalloc(len, KM_SLEEP)) != NULL)) {

		if (ri->ri_optfont.stride < ri->ri_optfont.fontwidth) {
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
			}
		} else {
			rasops_make_box_chars_alpha(ri);
		}
	} else
		memset(&ri->ri_optfont, 0, sizeof(ri->ri_optfont));

	if (ri->ri_font->fontwidth > 32 || ri->ri_font->fontwidth < 4)
		panic("rasops_init: fontwidth assumptions botched!");

	/* Need this to frob the setup below */
	bpp = (ri->ri_depth == 15 ? 16 : ri->ri_depth);

	if ((ri->ri_flg & RI_CFGDONE) != 0)
		ri->ri_bits = ri->ri_origbits;

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
	ri->ri_delta = ri->ri_stride - ri->ri_emustride;
	ri->ri_ccol = 0;
	ri->ri_crow = 0;
	ri->ri_pelbytes = bpp >> 3;

	ri->ri_xscale = (ri->ri_font->fontwidth * bpp) >> 3;
	ri->ri_yscale = ri->ri_font->fontheight * ri->ri_stride;
	ri->ri_fontscale = ri->ri_font->fontheight * ri->ri_font->stride;

#ifdef DEBUG
	if ((ri->ri_delta & 3) != 0)
		panic("rasops_init: ri_delta not aligned on 32-bit boundary");
#endif
	/* Clear the entire display */
	if ((ri->ri_flg & RI_CLEAR) != 0)
		memset(ri->ri_bits, 0, ri->ri_stride * ri->ri_height);

	/* Now centre our window if needs be */
	ri->ri_origbits = ri->ri_bits;
	ri->ri_hworigbits = ri->ri_hwbits;

	if ((ri->ri_flg & RI_CENTER) != 0) {
		ri->ri_bits += (((ri->ri_width * bpp >> 3) -
		    ri->ri_emustride) >> 1) & ~3;
		ri->ri_bits += ((ri->ri_height - ri->ri_emuheight) >> 1) *
		    ri->ri_stride;
		if (ri->ri_hwbits != NULL) {
			ri->ri_hwbits += (((ri->ri_width * bpp >> 3) -
			    ri->ri_emustride) >> 1) & ~3;
			ri->ri_hwbits += ((ri->ri_height - ri->ri_emuheight) >> 1) *
			    ri->ri_stride;
		}
		ri->ri_yorigin = (int)(ri->ri_bits - ri->ri_origbits)
		   / ri->ri_stride;
		ri->ri_xorigin = (((int)(ri->ri_bits - ri->ri_origbits)
		   % ri->ri_stride) * 8 / bpp);
	} else
		ri->ri_xorigin = ri->ri_yorigin = 0;

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

	if (ri->ri_depth < 8 || (ri->ri_flg & RI_FORCEMONO) != 0) {
		ri->ri_ops.allocattr = rasops_allocattr_mono;
		ri->ri_caps = WSSCREEN_UNDERLINE | WSSCREEN_REVERSE;
	} else {
		ri->ri_ops.allocattr = rasops_allocattr_color;
		ri->ri_caps = WSSCREEN_UNDERLINE | WSSCREEN_HILIT |
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
#if NRASOPS15 > 0 || NRASOPS16 > 0
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
		splx(s);
		return (-1);
	}

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
	splx(s);
	return (0);
}

/*
 * Map a character.
 */
static int
rasops_mapchar(void *cookie, int c, u_int *cp)
{
	struct rasops_info *ri;

	ri = (struct rasops_info *)cookie;

#ifdef DIAGNOSTIC
	if (ri->ri_font == NULL)
		panic("rasops_mapchar: no font selected");
#endif

	if ( (c = wsfont_map_unichar(ri->ri_font, c)) < 0) {
		*cp = ' ';
		return (0);
	}

	if (c < ri->ri_font->firstchar) {
		*cp = ' ';
		return (0);
	}

#if 0
	if (c - ri->ri_font->firstchar >= ri->ri_font->numchars) {
		*cp = ' ';
		return (0);
	}
#endif
	*cp = c;
	return (5);
}

/*
 * Allocate a color attribute.
 */
static int
rasops_allocattr_color(void *cookie, int fg, int bg, int flg,
    long *attr)
{
	int swap;

	if (__predict_false((unsigned int)fg >= sizeof(rasops_isgray) ||
	    (unsigned int)bg >= sizeof(rasops_isgray)))
		return (EINVAL);

#ifdef RASOPS_CLIPPING
	fg &= 7;
	bg &= 7;
#endif
	if ((flg & WSATTR_BLINK) != 0)
		return (EINVAL);

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

	if ((flg & WSATTR_REVERSE) != 0) {
		swap = fg;
		fg = bg;
		bg = swap;
	}

	if ((flg & WSATTR_HILIT) != 0)
		fg += 8;

	flg = ((flg & WSATTR_UNDERLINE) ? 1 : 0);

	if (rasops_isgray[fg])
		flg |= 2;

	if (rasops_isgray[bg])
		flg |= 4;

	*attr = (bg << 16) | (fg << 24) | flg;
	return (0);
}

/*
 * Allocate a mono attribute.
 */
static int
rasops_allocattr_mono(void *cookie, int fg, int bg, int flg,
    long *attr)
{
	int swap;

	if ((flg & (WSATTR_BLINK | WSATTR_HILIT | WSATTR_WSCOLORS)) != 0)
		return (EINVAL);

	fg = 1;
	bg = 0;

	if ((flg & WSATTR_REVERSE) != 0) {
		swap = fg;
		fg = bg;
		bg = swap;
	}

	*attr = (bg << 16) | (fg << 24) | ((flg & WSATTR_UNDERLINE) ? 7 : 6);
	return (0);
}

/*
 * Copy rows.
 */
static void
rasops_copyrows(void *cookie, int src, int dst, int num)
{
	int32_t *sp, *dp, *hp, *srp, *drp, *hrp;
	struct rasops_info *ri;
	int n8, n1, cnt, delta;

	ri = (struct rasops_info *)cookie;
	hp = hrp = NULL;

#ifdef RASOPS_CLIPPING
	if (dst == src)
		return;

	if (src < 0) {
		num += src;
		src = 0;
	}

	if ((src + num) > ri->ri_rows)
		num = ri->ri_rows - src;

	if (dst < 0) {
		num += dst;
		dst = 0;
	}

	if ((dst + num) > ri->ri_rows)
		num = ri->ri_rows - dst;

	if (num <= 0)
		return;
#endif

	num *= ri->ri_font->fontheight;
	n8 = ri->ri_emustride >> 5;
	n1 = (ri->ri_emustride >> 2) & 7;

	if (dst < src) {
		srp = (int32_t *)(ri->ri_bits + src * ri->ri_yscale);
		drp = (int32_t *)(ri->ri_bits + dst * ri->ri_yscale);
		if (ri->ri_hwbits)
			hrp = (int32_t *)(ri->ri_hwbits + dst *
			    ri->ri_yscale);
		delta = ri->ri_stride;
	} else {
		src = ri->ri_font->fontheight * src + num - 1;
		dst = ri->ri_font->fontheight * dst + num - 1;
		srp = (int32_t *)(ri->ri_bits + src * ri->ri_stride);
		drp = (int32_t *)(ri->ri_bits + dst * ri->ri_stride);
		if (ri->ri_hwbits)
			hrp = (int32_t *)(ri->ri_hwbits + dst *
			    ri->ri_stride);
		
		delta = -ri->ri_stride;
	}

	while (num--) {
		dp = drp;
		sp = srp;
		if (ri->ri_hwbits)
			hp = hrp;

		DELTA(drp, delta, int32_t *);
		DELTA(srp, delta, int32_t *);
		if (ri->ri_hwbits)
			DELTA(hrp, delta, int32_t *);

		for (cnt = n8; cnt; cnt--) {
			dp[0] = sp[0];
			dp[1] = sp[1];
			dp[2] = sp[2];
			dp[3] = sp[3];
			dp[4] = sp[4];
			dp[5] = sp[5];
			dp[6] = sp[6];
			dp[7] = sp[7];
			dp += 8;
			sp += 8;
		}
		if (ri->ri_hwbits) {
			sp -= (8 * n8);
			for (cnt = n8; cnt; cnt--) {
				hp[0] = sp[0];
				hp[1] = sp[1];
				hp[2] = sp[2];
				hp[3] = sp[3];
				hp[4] = sp[4];
				hp[5] = sp[5];
				hp[6] = sp[6];
				hp[7] = sp[7];
				hp += 8;
				sp += 8;
			}
		}

		for (cnt = n1; cnt; cnt--) {
			*dp++ = *sp++;
			if (ri->ri_hwbits)
				*hp++ = *(sp - 1);
		}
	}
}

/*
 * Copy columns. This is slow, and hard to optimize due to alignment,
 * and the fact that we have to copy both left->right and right->left.
 * We simply cop-out here and use memmove(), since it handles all of
 * these cases anyway.
 */
void
rasops_copycols(void *cookie, int row, int src, int dst, int num)
{
	struct rasops_info *ri;
	u_char *sp, *dp, *hp;
	int height;

	ri = (struct rasops_info *)cookie;
	hp = NULL;

#ifdef RASOPS_CLIPPING
	if (dst == src)
		return;

	/* Catches < 0 case too */
	if ((unsigned)row >= (unsigned)ri->ri_rows)
		return;

	if (src < 0) {
		num += src;
		src = 0;
	}

	if ((src + num) > ri->ri_cols)
		num = ri->ri_cols - src;

	if (dst < 0) {
		num += dst;
		dst = 0;
	}

	if ((dst + num) > ri->ri_cols)
		num = ri->ri_cols - dst;

	if (num <= 0)
		return;
#endif

	num *= ri->ri_xscale;
	row *= ri->ri_yscale;
	height = ri->ri_font->fontheight;

	sp = ri->ri_bits + row + src * ri->ri_xscale;
	dp = ri->ri_bits + row + dst * ri->ri_xscale;
	if (ri->ri_hwbits)
		hp = ri->ri_hwbits + row + dst * ri->ri_xscale;

	while (height--) {
		memmove(dp, sp, num);
		if (ri->ri_hwbits) {
			memcpy(hp, sp, num);
			hp += ri->ri_stride;
		}
		dp += ri->ri_stride;
		sp += ri->ri_stride;
	}
}

/*
 * Turn cursor off/on.
 */
static void
rasops_cursor(void *cookie, int on, int row, int col)
{
	struct rasops_info *ri;

	ri = (struct rasops_info *)cookie;

	/* Turn old cursor off */
	if ((ri->ri_flg & RI_CURSOR) != 0)
#ifdef RASOPS_CLIPPING
		if ((ri->ri_flg & RI_CURSORCLIP) == 0)
#endif
			ri->ri_do_cursor(ri);

	/* Select new cursor */
#ifdef RASOPS_CLIPPING
	ri->ri_flg &= ~RI_CURSORCLIP;

	if (row < 0 || row >= ri->ri_rows)
		ri->ri_flg |= RI_CURSORCLIP;
	else if (col < 0 || col >= ri->ri_cols)
		ri->ri_flg |= RI_CURSORCLIP;
#endif
	ri->ri_crow = row;
	ri->ri_ccol = col;

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
static void
rasops_init_devcmap(struct rasops_info *ri)
{
	const u_char *p;
	int i, c;

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

	case 8:
		if ((ri->ri_flg & RI_8BIT_IS_RGB) == 0) {
			for (i = 0; i < 16; i++)
				ri->ri_devcmap[i] =
				    i | (i<<8) | (i<<16) | (i<<24);
			return;
		}
	}

	p = rasops_cmap;

	for (i = 0; i < 16; i++) {
		if (ri->ri_rnum <= 8)
			c = (*p >> (8 - ri->ri_rnum)) << ri->ri_rpos;
		else
			c = (*p << (ri->ri_rnum - 8)) << ri->ri_rpos;
		p++;

		if (ri->ri_gnum <= 8)
			c |= (*p >> (8 - ri->ri_gnum)) << ri->ri_gpos;
		else
			c |= (*p << (ri->ri_gnum - 8)) << ri->ri_gpos;
		p++;

		if (ri->ri_bnum <= 8)
			c |= (*p >> (8 - ri->ri_bnum)) << ri->ri_bpos;
		else
			c |= (*p << (ri->ri_bnum - 8)) << ri->ri_bpos;
		p++;

		/* Fill the word for generic routines, which want this */
		if (ri->ri_depth == 24)
			c = c | ((c & 0xff) << 24);
		else if (ri->ri_depth == 8) {
			c = c | (c << 8);
			c |= c << 16;
		} else if (ri->ri_depth <= 16)
			c = c | (c << 16);

		/* 24bpp does bswap on the fly. {32,16,15}bpp do it here. */
		if ((ri->ri_flg & RI_BSWAP) == 0)
			ri->ri_devcmap[i] = c;
		else if (ri->ri_depth == 32)
			ri->ri_devcmap[i] = bswap32(c);
		else if (ri->ri_depth == 16 || ri->ri_depth == 15)
			ri->ri_devcmap[i] = bswap16(c);
		else
			ri->ri_devcmap[i] = c;
	}
}

/*
 * Unpack a rasops attribute
 */
void
rasops_unpack_attr(long attr, int *fg, int *bg, int *underline)
{

	*fg = ((u_int)attr >> 24) & 0xf;
	*bg = ((u_int)attr >> 16) & 0xf;
	if (underline != NULL)
		*underline = (u_int)attr & 1;
}

/*
 * Erase rows. This isn't static, since 24-bpp uses it in special cases.
 */
void
rasops_eraserows(void *cookie, int row, int num, long attr)
{
	struct rasops_info *ri;
	int np, nw, cnt, delta;
	int32_t *dp, *hp, clr;
	int i;

	ri = (struct rasops_info *)cookie;
	hp = NULL;

#ifdef RASOPS_CLIPPING
	if (row < 0) {
		num += row;
		row = 0;
	}

	if ((row + num) > ri->ri_rows)
		num = ri->ri_rows - row;

	if (num <= 0)
		return;
#endif

	clr = ri->ri_devcmap[(attr >> 16) & 0xf];

	/*
	 * XXX The wsdisplay_emulops interface seems a little deficient in
	 * that there is no way to clear the *entire* screen. We provide a
	 * workaround here: if the entire console area is being cleared, and
	 * the RI_FULLCLEAR flag is set, clear the entire display.
	 */
	if (num == ri->ri_rows && (ri->ri_flg & RI_FULLCLEAR) != 0) {
		np = ri->ri_stride >> 5;
		nw = (ri->ri_stride >> 2) & 7;
		num = ri->ri_height;
		dp = (int32_t *)ri->ri_origbits;
		if (ri->ri_hwbits)
			hp = (int32_t *)ri->ri_hworigbits;
		delta = 0;
	} else {
		np = ri->ri_emustride >> 5;
		nw = (ri->ri_emustride >> 2) & 7;
		num *= ri->ri_font->fontheight;
		dp = (int32_t *)(ri->ri_bits + row * ri->ri_yscale);
		if (ri->ri_hwbits)
			hp = (int32_t *)(ri->ri_hwbits + row *
			    ri->ri_yscale);
		delta = ri->ri_delta;
	}

	while (num--) {
		for (cnt = np; cnt; cnt--) {
			for (i = 0; i < 8; i++) {
				dp[i] = clr;
				if (ri->ri_hwbits)
					hp[i] = clr;
			}
			dp += 8;
			if (ri->ri_hwbits)
				hp += 8;
		}

		for (cnt = nw; cnt; cnt--) {
			*(int32_t *)dp = clr;
			DELTA(dp, 4, int32_t *);
			if (ri->ri_hwbits) {
				*(int32_t *)hp = clr;
				DELTA(hp, 4, int32_t *);
			}
		}

		DELTA(dp, delta, int32_t *);
		if (ri->ri_hwbits)
			DELTA(hp, delta, int32_t *);
	}
}

/*
 * Actually turn the cursor on or off. This does the dirty work for
 * rasops_cursor().
 */
static void
rasops_do_cursor(struct rasops_info *ri)
{
	int full1, height, cnt, slop1, slop2, row, col;
	u_char *dp, *rp, *hrp, *hp, tmp = 0;

	hrp = hp = NULL;

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

	rp = ri->ri_bits + row * ri->ri_yscale + col * ri->ri_xscale;
	if (ri->ri_hwbits)
		hrp = ri->ri_hwbits + row * ri->ri_yscale + col
		    * ri->ri_xscale;
	height = ri->ri_font->fontheight;
	slop1 = (4 - ((long)rp & 3)) & 3;

	if (slop1 > ri->ri_xscale)
		slop1 = ri->ri_xscale;

	slop2 = (ri->ri_xscale - slop1) & 3;
	full1 = (ri->ri_xscale - slop1 - slop2) >> 2;

	if ((slop1 | slop2) == 0) {
		uint32_t tmp32;
		/* A common case */
		while (height--) {
			dp = rp;
			rp += ri->ri_stride;
			if (ri->ri_hwbits) {
				hp = hrp;
				hrp += ri->ri_stride;
			}

			for (cnt = full1; cnt; cnt--) {
				tmp32 = *(int32_t *)dp ^ ~0;
				*(int32_t *)dp = tmp32;
				dp += 4;
				if (ri->ri_hwbits) {
					*(int32_t *)hp = tmp32;
					hp += 4;
				}
			}
		}
	} else {
		uint16_t tmp16;
		uint32_t tmp32;
		/* XXX this is stupid.. use masks instead */
		while (height--) {
			dp = rp;
			rp += ri->ri_stride;
			if (ri->ri_hwbits) {
				hp = hrp;
				hrp += ri->ri_stride;
			}

			if (slop1 & 1) {
				tmp = *dp ^ ~0;
				*dp = tmp;
				dp++;
				if (ri->ri_hwbits) {
					*hp++ = tmp;
				}
			}

			if (slop1 & 2) {
				tmp16 = *(int16_t *)dp ^ ~0;
				*(uint16_t *)dp = tmp16;
				dp += 2;
				if (ri->ri_hwbits) {
					*(int16_t *)hp = tmp16;
					hp += 2;
				}
			}

			for (cnt = full1; cnt; cnt--) {
				tmp32 = *(int32_t *)dp ^ ~0;
				*(uint32_t *)dp = tmp32;
				dp += 4;
				if (ri->ri_hwbits) {
					*(int32_t *)hp = tmp32;
					hp += 4;
				}
			}

			if (slop2 & 1) {
				tmp = *dp ^ ~0;
				*dp = tmp;
				dp++;
				if (ri->ri_hwbits)
					*hp++ = tmp;
			}

			if (slop2 & 2) {
				tmp16 = *(int16_t *)dp ^ ~0;
				*(uint16_t *)dp = tmp16;
				if (ri->ri_hwbits)
					*(int16_t *)hp = tmp16;
			}
		}
	}
}

/*
 * Erase columns.
 */
void
rasops_erasecols(void *cookie, int row, int col, int num, long attr)
{
	int n8, height, cnt, slop1, slop2, clr;
	struct rasops_info *ri;
	int32_t *rp, *dp, *hrp, *hp;
	int i;

	ri = (struct rasops_info *)cookie;
	hrp = hp = NULL;

#ifdef RASOPS_CLIPPING
	if ((unsigned)row >= (unsigned)ri->ri_rows)
		return;

	if (col < 0) {
		num += col;
		col = 0;
	}

	if ((col + num) > ri->ri_cols)
		num = ri->ri_cols - col;

	if (num <= 0)
		return;
#endif

	num = num * ri->ri_xscale;
	rp = (int32_t *)(ri->ri_bits + row*ri->ri_yscale + col*ri->ri_xscale);
	if (ri->ri_hwbits)
		hrp = (int32_t *)(ri->ri_hwbits + row*ri->ri_yscale +
		    col*ri->ri_xscale);
	height = ri->ri_font->fontheight;
	clr = ri->ri_devcmap[(attr >> 16) & 0xf];

	/* Don't bother using the full loop for <= 32 pels */
	if (num <= 32) {
		if (((num | ri->ri_xscale) & 3) == 0) {
			/* Word aligned blt */
			num >>= 2;

			while (height--) {
				dp = rp;
				DELTA(rp, ri->ri_stride, int32_t *);
				if (ri->ri_hwbits) {
					hp = hrp;
					DELTA(hrp, ri->ri_stride, int32_t *);
				}

				for (cnt = num; cnt; cnt--) {
					*dp++ = clr;
					if (ri->ri_hwbits)
						*hp++ = clr;
				}
			}
		} else if (((num | ri->ri_xscale) & 1) == 0) {
			/*
			 * Halfword aligned blt. This is needed so the
			 * 15/16 bit ops can use this function.
			 */
			num >>= 1;

			while (height--) {
				dp = rp;
				DELTA(rp, ri->ri_stride, int32_t *);
				if (ri->ri_hwbits) {
					hp = hrp;
					DELTA(hrp, ri->ri_stride, int32_t *);
				}

				for (cnt = num; cnt; cnt--) {
					*(int16_t *)dp = clr;
					DELTA(dp, 2, int32_t *);
					if (ri->ri_hwbits) {
						*(int16_t *)hp = clr;
						DELTA(hp, 2, int32_t *);
					}
				}
			}
		} else {
			while (height--) {
				dp = rp;
				DELTA(rp, ri->ri_stride, int32_t *);
				if (ri->ri_hwbits) {
					hp = hrp;
					DELTA(hrp, ri->ri_stride, int32_t *);
				}

				for (cnt = num; cnt; cnt--) {
					*(u_char *)dp = clr;
					DELTA(dp, 1, int32_t *);
					if (ri->ri_hwbits) {
						*(u_char *)hp = clr;
						DELTA(hp, 1, int32_t *);
					}
				}
			}
		}

		return;
	}

	slop1 = (4 - ((long)rp & 3)) & 3;
	slop2 = (num - slop1) & 3;
	num -= slop1 + slop2;
	n8 = num >> 5;
	num = (num >> 2) & 7;

	while (height--) {
		dp = rp;
		DELTA(rp, ri->ri_stride, int32_t *);
		if (ri->ri_hwbits) {
			hp = hrp;
			DELTA(hrp, ri->ri_stride, int32_t *);
		}

		/* Align span to 4 bytes */
		if (slop1 & 1) {
			*(u_char *)dp = clr;
			DELTA(dp, 1, int32_t *);
			if (ri->ri_hwbits) {
				*(u_char *)hp = clr;
				DELTA(hp, 1, int32_t *);
			}
		}

		if (slop1 & 2) {
			*(int16_t *)dp = clr;
			DELTA(dp, 2, int32_t *);
			if (ri->ri_hwbits) {
				*(int16_t *)hp = clr;
				DELTA(hp, 2, int32_t *);
			}
		}

		/* Write 32 bytes per loop */
		for (cnt = n8; cnt; cnt--) {
			for (i = 0; i < 8; i++) {
				dp[i] = clr;
				if (ri->ri_hwbits)
					hp[i] = clr;
			}
			dp += 8;
			if (ri->ri_hwbits)
				hp += 8;
		}

		/* Write 4 bytes per loop */
		for (cnt = num; cnt; cnt--) {
			*dp++ = clr;
			if (ri->ri_hwbits)
				*hp++ = clr;
		}

		/* Write unaligned trailing slop */
		if (slop2 & 1) {
			*(u_char *)dp = clr;
			DELTA(dp, 1, int32_t *);
			if (ri->ri_hwbits) {
				*(u_char *)hp = clr;
				DELTA(hp, 1, int32_t *);
			}
		}

		if (slop2 & 2) {
			*(int16_t *)dp = clr;
			if (ri->ri_hwbits)
				*(int16_t *)hp = clr;
		}
	}
}

#if NRASOPS_ROTATION > 0
/*
 * Quarter clockwise rotation routines (originally intended for the
 * built-in Zaurus C3x00 display in 16bpp).
 */

#include <sys/malloc.h>

static void
rasops_rotate_font(int *cookie, int rotate)
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

	f = malloc(sizeof(struct rotatedfont), M_DEVBUF, M_WAITOK);
	if (f == NULL)
		goto fail0;

	if ((ncookie = wsfont_rotate(*cookie, rotate)) == -1)
		goto fail1;

	f->rf_cookie = *cookie;
	f->rf_rotated = ncookie;
	SLIST_INSERT_HEAD(&rotatedfonts, f, rf_next);

	*cookie = ncookie;
	return;

fail1:	free(f, M_DEVBUF);
fail0:	/* Just use the existing font, I guess...  */
	return;
}

static void
rasops_copychar(void *cookie, int srcrow, int dstrow, int srccol, int dstcol)
{
	struct rasops_info *ri;
	u_char *sp, *dp;
	int height;
	int r_srcrow, r_dstrow, r_srccol, r_dstcol;

	ri = (struct rasops_info *)cookie;

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

static void
rasops_putchar_rotated_cw(void *cookie, int row, int col, u_int uc, long attr)
{
	struct rasops_info *ri;
	u_char *rp;
	int height;

	ri = (struct rasops_info *)cookie;

	if (__predict_false((unsigned int)row > ri->ri_rows ||
	    (unsigned int)col > ri->ri_cols))
		return;

	/* Avoid underflow */
	if ((ri->ri_rows - row - 1) < 0)
		return;

	/* Do rotated char sans (side)underline */
	ri->ri_real_ops.putchar(cookie, col, ri->ri_rows - row - 1, uc,
	    attr & ~1);

	/* Do rotated underline */
	rp = ri->ri_bits + col * ri->ri_yscale + (ri->ri_rows - row - 1) * 
	    ri->ri_xscale;
	height = ri->ri_font->fontheight;

	/* XXX this assumes 16-bit color depth */
	if ((attr & 1) != 0) {
		int16_t c = (int16_t)ri->ri_devcmap[((u_int)attr >> 24) & 0xf];

		while (height--) {
			*(int16_t *)rp = c;
			rp += ri->ri_stride;
		}
	}
}

static void
rasops_erasecols_rotated_cw(void *cookie, int row, int col, int num, long attr)
{
	struct rasops_info *ri;
	int i;

	ri = (struct rasops_info *)cookie;

	for (i = col; i < col + num; i++)
		ri->ri_ops.putchar(cookie, row, i, ' ', attr);
}

/* XXX: these could likely be optimised somewhat. */
static void
rasops_copyrows_rotated_cw(void *cookie, int src, int dst, int num)
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

static void
rasops_copycols_rotated_cw(void *cookie, int row, int src, int dst, int num)
{
	int coff;

	if (src > dst)
		for (coff = 0; coff < num; coff++)
			rasops_copychar(cookie, row, row, src + coff, dst + coff);
	else
		for (coff = num - 1; coff >= 0; coff--)
			rasops_copychar(cookie, row, row, src + coff, dst + coff);
}

static void
rasops_eraserows_rotated_cw(void *cookie, int row, int num, long attr)
{
	struct rasops_info *ri;
	int col, rn;

	ri = (struct rasops_info *)cookie;

	for (rn = row; rn < row + num; rn++)
		for (col = 0; col < ri->ri_cols; col++)
			ri->ri_ops.putchar(cookie, rn, col, ' ', attr);
}

/*
 * Quarter counter-clockwise rotation routines (originally intended for the
 * built-in Sharp W-ZERO3 display in 16bpp).
 */
static void
rasops_copychar_ccw(void *cookie, int srcrow, int dstrow, int srccol, int dstcol)
{
	struct rasops_info *ri;
	u_char *sp, *dp;
	int height;
	int r_srcrow, r_dstrow, r_srccol, r_dstcol;

	ri = (struct rasops_info *)cookie;

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

static void
rasops_putchar_rotated_ccw(void *cookie, int row, int col, u_int uc, long attr)
{
	struct rasops_info *ri;
	u_char *rp;
	int height;

	ri = (struct rasops_info *)cookie;

	if (__predict_false((unsigned int)row > ri->ri_rows ||
	    (unsigned int)col > ri->ri_cols))
		return;

	/* Avoid underflow */
	if ((ri->ri_cols - col - 1) < 0)
		return;

	/* Do rotated char sans (side)underline */
	ri->ri_real_ops.putchar(cookie, ri->ri_cols - col - 1, row, uc,
	    attr & ~1);

	/* Do rotated underline */
	rp = ri->ri_bits + (ri->ri_cols - col - 1) * ri->ri_yscale +
	    row * ri->ri_xscale +
	    (ri->ri_font->fontwidth - 1) * ri->ri_pelbytes;
	height = ri->ri_font->fontheight;

	/* XXX this assumes 16-bit color depth */
	if ((attr & 1) != 0) {
		int16_t c = (int16_t)ri->ri_devcmap[((u_int)attr >> 24) & 0xf];

		while (height--) {
			*(int16_t *)rp = c;
			rp += ri->ri_stride;
		}
	}
}

/* XXX: these could likely be optimised somewhat. */
static void
rasops_copyrows_rotated_ccw(void *cookie, int src, int dst, int num)
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

static void
rasops_copycols_rotated_ccw(void *cookie, int row, int src, int dst, int num)
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

void
rasops_make_box_chars_16(struct rasops_info *ri)
{
	uint16_t vert_mask, hmask_left, hmask_right;
	uint16_t *data = (uint16_t *)ri->ri_optfont.data;
	int c, i, mid;

	vert_mask = 0xc000 >> ((ri->ri_font->fontwidth >> 1) - 1);
	hmask_left = 0xff00 << (8 - (ri->ri_font->fontwidth >> 1));
	hmask_right = hmask_left >> ((ri->ri_font->fontwidth + 1)>> 1);
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

void
rasops_make_box_chars_8(struct rasops_info *ri)
{
	uint8_t vert_mask, hmask_left, hmask_right;
	uint8_t *data = (uint8_t *)ri->ri_optfont.data;
	int c, i, mid;

	vert_mask = 0xc0 >> ((ri->ri_font->fontwidth >> 1) - 1);
	hmask_left = 0xf0 << (4 - (ri->ri_font->fontwidth >> 1));
	hmask_right = hmask_left >> ((ri->ri_font->fontwidth + 1)>> 1);
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

void
rasops_make_box_chars_32(struct rasops_info *ri)
{
	uint32_t vert_mask, hmask_left, hmask_right;
	uint32_t *data = (uint32_t *)ri->ri_optfont.data;
	int c, i, mid;

	vert_mask = 0xc0000000 >> ((ri->ri_font->fontwidth >> 1) - 1);
	hmask_left = 0xffff0000 << (16 - (ri->ri_font->fontwidth >> 1));
	hmask_right = hmask_left >> ((ri->ri_font->fontwidth + 1)>> 1);
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

void
rasops_make_box_chars_alpha(struct rasops_info *ri)
{
	uint8_t *data = (uint8_t *)ri->ri_optfont.data;
	uint8_t *ddata;
	int c, i, hmid, vmid, wi, he;

	wi = ri->ri_font->fontwidth;
	he = ri->ri_font->fontheight;
	
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
 
int
rasops_get_cmap(struct rasops_info *ri, uint8_t *palette, size_t bytes)
{
	if ((ri->ri_depth == 8 ) && ((ri->ri_flg & RI_8BIT_IS_RGB) > 0)) {
		/* generate an R3G3B2 palette */
		int i, idx = 0;
		uint8_t tmp;

		if (bytes < 768)
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
	} else {
		memcpy(palette, rasops_cmap, MIN(bytes, sizeof(rasops_cmap)));
	}
	return 0;
}
