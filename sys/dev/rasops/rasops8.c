/* 	$NetBSD: rasops8.c,v 1.34 2013/09/15 09:41:55 martin Exp $	*/

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
__KERNEL_RCSID(0, "$NetBSD: rasops8.c,v 1.34 2013/09/15 09:41:55 martin Exp $");

#include "opt_rasops.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>

#include <dev/wscons/wsdisplayvar.h>
#include <dev/wscons/wsconsio.h>
#include <dev/rasops/rasops.h>

static void 	rasops8_putchar(void *, int, int, u_int, long attr);
static void 	rasops8_putchar_aa(void *, int, int, u_int, long attr);
#ifndef RASOPS_SMALL
static void 	rasops8_putchar8(void *, int, int, u_int, long attr);
static void 	rasops8_putchar12(void *, int, int, u_int, long attr);
static void 	rasops8_putchar16(void *, int, int, u_int, long attr);
static void	rasops8_makestamp(struct rasops_info *ri, long);

/*
 * 4x1 stamp for optimized character blitting
 */
static int32_t	stamp[16];
static long	stamp_attr;
static int	stamp_mutex;	/* XXX see note in README */
#endif

/*
 * XXX this confuses the hell out of gcc2 (not egcs) which always insists
 * that the shift count is negative.
 *
 * offset = STAMP_SHIFT(fontbits, nibble #) & STAMP_MASK
 * destination = STAMP_READ(offset)
 */
#define STAMP_SHIFT(fb,n)	((n*4-2) >= 0 ? (fb)>>(n*4-2):(fb)<<-(n*4-2))
#define STAMP_MASK		(0xf << 2)
#define STAMP_READ(o)		(*(int32_t *)((char *)stamp + (o)))

/*
 * Initialize a 'rasops_info' descriptor for this depth.
 */
void
rasops8_init(struct rasops_info *ri)
{

	if (FONT_IS_ALPHA(ri->ri_font)) {
		ri->ri_ops.putchar = rasops8_putchar_aa;
	} else {
		switch (ri->ri_font->fontwidth) {
#ifndef RASOPS_SMALL
		case 8:
			ri->ri_ops.putchar = rasops8_putchar8;
			break;
		case 12:
			ri->ri_ops.putchar = rasops8_putchar12;
			break;
		case 16:
			ri->ri_ops.putchar = rasops8_putchar16;
			break;
#endif /* !RASOPS_SMALL */
		default:
			ri->ri_ops.putchar = rasops8_putchar;
			break;
		}
	}
	if (ri->ri_flg & RI_8BIT_IS_RGB) {
		ri->ri_rnum = 3;
		ri->ri_rpos = 5;
		ri->ri_gnum = 3;
		ri->ri_gpos = 2;
		ri->ri_bnum = 2;
		ri->ri_bpos = 0;
	}
}

/*
 * Put a single character.
 */
static void
rasops8_putchar(void *cookie, int row, int col, u_int uc, long attr)
{
	int width, height, cnt, fs, fb;
	u_char *dp, *rp, *hp, *hrp, *fr, clr[2];
	struct rasops_info *ri = (struct rasops_info *)cookie;
	struct wsdisplay_font *font = PICK_FONT(ri, uc);

	hp = hrp = NULL;

	if (!CHAR_IN_FONT(uc, font))
		return;

#ifdef RASOPS_CLIPPING
	/* Catches 'row < 0' case too */
	if ((unsigned)row >= (unsigned)ri->ri_rows)
		return;

	if ((unsigned)col >= (unsigned)ri->ri_cols)
		return;
#endif
	rp = ri->ri_bits + row * ri->ri_yscale + col * ri->ri_xscale;
	if (ri->ri_hwbits)
		hrp = ri->ri_hwbits + row * ri->ri_yscale + col *
		    ri->ri_xscale;

	height = font->fontheight;
	width = font->fontwidth;
	clr[0] = (u_char)ri->ri_devcmap[(attr >> 16) & 0xf];
	clr[1] = (u_char)ri->ri_devcmap[(attr >> 24) & 0xf];

	if (uc == ' ') {
		u_char c = clr[0];

		while (height--) {
			memset(rp, c, width);
			if (ri->ri_hwbits) {
				memset(hrp, c, width);
				hrp += ri->ri_stride;
			}
			rp += ri->ri_stride;
		}
	} else {
		fr = WSFONT_GLYPH(uc, font);
		fs = font->stride;

		while (height--) {
			dp = rp;
			if (ri->ri_hwbits)
				hp = hrp;
			fb = fr[3] | (fr[2] << 8) | (fr[1] << 16) | (fr[0] << 24);
			fr += fs;
			rp += ri->ri_stride;
			if (ri->ri_hwbits)
				hrp += ri->ri_stride;

			for (cnt = width; cnt; cnt--) {
				*dp++ = clr[(fb >> 31) & 1];
				if (ri->ri_hwbits)
					*hp++ = clr[(fb >> 31) & 1];
				fb <<= 1;
			}
		}
	}

	/* Do underline */
	if ((attr & 1) != 0) {
		u_char c = clr[1];

		rp -= (ri->ri_stride << 1);
		if (ri->ri_hwbits)
			hrp -= (ri->ri_stride << 1);

		while (width--) {
			*rp++ = c;
			if (ri->ri_hwbits)
				*hrp++ = c;
		}
	}
}

static void
rasops8_putchar_aa(void *cookie, int row, int col, u_int uc, long attr)
{
	int width, height;
	u_char *rp, *hrp, *fr, bg, fg, pixel;
	struct rasops_info *ri = (struct rasops_info *)cookie;
	struct wsdisplay_font *font = PICK_FONT(ri, uc);
	int x, y, r, g, b, aval;
	int r1, g1, b1, r0, g0, b0, fgo, bgo;
	uint8_t scanline[32] __attribute__ ((aligned(8)));

	hrp = NULL;

	if (!CHAR_IN_FONT(uc, font))
		return;

#ifdef RASOPS_CLIPPING
	/* Catches 'row < 0' case too */
	if ((unsigned)row >= (unsigned)ri->ri_rows)
		return;

	if ((unsigned)col >= (unsigned)ri->ri_cols)
		return;
#endif
	rp = ri->ri_bits + row * ri->ri_yscale + col * ri->ri_xscale;
	if (ri->ri_hwbits)
		hrp = ri->ri_hwbits + row * ri->ri_yscale + col *
		    ri->ri_xscale;

	height = font->fontheight;
	width = font->fontwidth;
	bg = (u_char)ri->ri_devcmap[(attr >> 16) & 0xf];
	fg = (u_char)ri->ri_devcmap[(attr >> 24) & 0xf];

	if (uc == ' ') {

		while (height--) {
			memset(rp, bg, width);
			if (ri->ri_hwbits) {
				memset(hrp, bg, width);
				hrp += ri->ri_stride;
			}
			rp += ri->ri_stride;
		}
	} else {
		fr = WSFONT_GLYPH(uc, font);
		/*
		 * we need the RGB colours here, get offsets into rasops_cmap
		 */
		fgo = ((attr >> 24) & 0xf) * 3;
		bgo = ((attr >> 16) & 0xf) * 3;

		r0 = rasops_cmap[bgo];
		r1 = rasops_cmap[fgo];
		g0 = rasops_cmap[bgo + 1];
		g1 = rasops_cmap[fgo + 1];
		b0 = rasops_cmap[bgo + 2];
		b1 = rasops_cmap[fgo + 2];

		for (y = 0; y < height; y++) {
			for (x = 0; x < width; x++) {
				aval = *fr;
				fr++;
				if (aval == 0) {
					pixel = bg;
				} else if (aval == 255) {
					pixel = fg;
				} else {
					r = aval * r1 + (255 - aval) * r0;
					g = aval * g1 + (255 - aval) * g0;
					b = aval * b1 + (255 - aval) * b0;
					pixel = ((r & 0xe000) >> 8) |
						((g & 0xe000) >> 11) |
						((b & 0xc000) >> 14);
				}
				scanline[x] = pixel;
			}
			memcpy(rp, scanline, width);
			if (ri->ri_hwbits) {
				memcpy(hrp, scanline, width);
				hrp += ri->ri_stride;
			}
			rp += ri->ri_stride;

		}
	}

	/* Do underline */
	if ((attr & 1) != 0) {

		rp -= (ri->ri_stride << 1);
		if (ri->ri_hwbits)
			hrp -= (ri->ri_stride << 1);

		while (width--) {
			*rp++ = fg;
			if (ri->ri_hwbits)
				*hrp++ = fg;
		}
	}
}

#ifndef RASOPS_SMALL
/*
 * Recompute the 4x1 blitting stamp.
 */
static void
rasops8_makestamp(struct rasops_info *ri, long attr)
{
	int32_t fg, bg;
	int i;

	fg = ri->ri_devcmap[(attr >> 24) & 0xf] & 0xff;
	bg = ri->ri_devcmap[(attr >> 16) & 0xf] & 0xff;
	stamp_attr = attr;

	for (i = 0; i < 16; i++) {
#if BYTE_ORDER == BIG_ENDIAN
#define NEED_LITTLE_ENDIAN_STAMP RI_BSWAP
#else
#define NEED_LITTLE_ENDIAN_STAMP 0
#endif
		if ((ri->ri_flg & RI_BSWAP) == NEED_LITTLE_ENDIAN_STAMP) {
			/* little endian */
			stamp[i] = (i & 8 ? fg : bg);
			stamp[i] |= ((i & 4 ? fg : bg) << 8);
			stamp[i] |= ((i & 2 ? fg : bg) << 16);
			stamp[i] |= ((i & 1 ? fg : bg) << 24);
		} else {
			/* big endian */
			stamp[i] = (i & 1 ? fg : bg);
			stamp[i] |= ((i & 2 ? fg : bg) << 8);
			stamp[i] |= ((i & 4 ? fg : bg) << 16);
			stamp[i] |= ((i & 8 ? fg : bg) << 24);
		}
	}
}

/*
 * Put a single character. This is for 8-pixel wide fonts.
 */
static void
rasops8_putchar8(void *cookie, int row, int col, u_int uc, long attr)
{
	struct rasops_info *ri = (struct rasops_info *)cookie;
	struct wsdisplay_font *font = PICK_FONT(ri, uc);
	int height, fs;
	int32_t *rp, *hp;
	u_char *fr;

	/* Can't risk remaking the stamp if it's already in use */
	if (stamp_mutex++) {
		stamp_mutex--;
		rasops8_putchar(cookie, row, col, uc, attr);
		return;
	}

	hp = NULL;

	if (!CHAR_IN_FONT(uc, font))
		return;

#ifdef RASOPS_CLIPPING
	if ((unsigned)row >= (unsigned)ri->ri_rows) {
		stamp_mutex--;
		return;
	}

	if ((unsigned)col >= (unsigned)ri->ri_cols) {
		stamp_mutex--;
		return;
	}
#endif

	/* Recompute stamp? */
	if (attr != stamp_attr)
		rasops8_makestamp(ri, attr);

	rp = (int32_t *)(ri->ri_bits + row*ri->ri_yscale + col*ri->ri_xscale);
	if (ri->ri_hwbits)
		hp = (int32_t *)(ri->ri_hwbits + row*ri->ri_yscale +
		    col*ri->ri_xscale);
	height = font->fontheight;

	if (uc == ' ') {
		while (height--) {
			rp[0] = rp[1] = stamp[0];
			DELTA(rp, ri->ri_stride, int32_t *);
			if (ri->ri_hwbits) {
				hp[0] = stamp[0];
				hp[1] = stamp[0];
				DELTA(hp, ri->ri_stride, int32_t *);
			}
		}
	} else {
		fr = WSFONT_GLYPH(uc, font);
		fs = font->stride;

		while (height--) {
			rp[0] = STAMP_READ(STAMP_SHIFT(fr[0], 1) & STAMP_MASK);
			rp[1] = STAMP_READ(STAMP_SHIFT(fr[0], 0) & STAMP_MASK);
			if (ri->ri_hwbits) {
				hp[0] = STAMP_READ(STAMP_SHIFT(fr[0], 1) &
				    STAMP_MASK);
				hp[1] = STAMP_READ(STAMP_SHIFT(fr[0], 0) &
				    STAMP_MASK);
			}

			fr += fs;
			DELTA(rp, ri->ri_stride, int32_t *);
			if (ri->ri_hwbits)
				DELTA(hp, ri->ri_stride, int32_t *);
		}
	}

	/* Do underline */
	if ((attr & 1) != 0) {
		DELTA(rp, -(ri->ri_stride << 1), int32_t *);
		rp[0] = rp[1] = stamp[15];
		if (ri->ri_hwbits) {
			DELTA(hp, -(ri->ri_stride << 1), int32_t *);
			hp[0] = stamp[15];
			hp[1] = stamp[15];
		}
	}

	stamp_mutex--;
}

/*
 * Put a single character. This is for 12-pixel wide fonts.
 */
static void
rasops8_putchar12(void *cookie, int row, int col, u_int uc, long attr)
{
	struct rasops_info *ri = (struct rasops_info *)cookie;
	struct wsdisplay_font *font = PICK_FONT(ri, uc);
	int height, fs;
	int32_t *rp,  *hrp;
	u_char *fr;

	/* Can't risk remaking the stamp if it's already in use */
	if (stamp_mutex++) {
		stamp_mutex--;
		rasops8_putchar(cookie, row, col, uc, attr);
		return;
	}

	hrp = NULL;

	if (!CHAR_IN_FONT(uc, font))
	    return;

#ifdef RASOPS_CLIPPING
	if ((unsigned)row >= (unsigned)ri->ri_rows) {
		stamp_mutex--;
		return;
	}

	if ((unsigned)col >= (unsigned)ri->ri_cols) {
		stamp_mutex--;
		return;
	}
#endif

	/* Recompute stamp? */
	if (attr != stamp_attr)
		rasops8_makestamp(ri, attr);

	rp = (int32_t *)(ri->ri_bits + row*ri->ri_yscale + col*ri->ri_xscale);
	if (ri->ri_hwbits)
		hrp = (int32_t *)(ri->ri_hwbits + row*ri->ri_yscale +
		    col*ri->ri_xscale);
	height = font->fontheight;

	if (uc == ' ') {
		while (height--) {
			int32_t c = stamp[0];

			rp[0] = rp[1] = rp[2] = c;
			DELTA(rp, ri->ri_stride, int32_t *);
			if (ri->ri_hwbits) {
				hrp[0] = c;
				hrp[1] = c;
				hrp[2] = c;
				DELTA(hrp, ri->ri_stride, int32_t *);
			}
		}
	} else {
		fr = WSFONT_GLYPH(uc, font);
		fs = font->stride;

		while (height--) {
			rp[0] = STAMP_READ(STAMP_SHIFT(fr[0], 1) & STAMP_MASK);
			rp[1] = STAMP_READ(STAMP_SHIFT(fr[0], 0) & STAMP_MASK);
			rp[2] = STAMP_READ(STAMP_SHIFT(fr[1], 1) & STAMP_MASK);
			if (ri->ri_hwbits) {
				hrp[0] = STAMP_READ(STAMP_SHIFT(fr[0], 1) & STAMP_MASK);
				hrp[1] = STAMP_READ(STAMP_SHIFT(fr[0], 0) & STAMP_MASK);
				hrp[2] = STAMP_READ(STAMP_SHIFT(fr[1], 1) & STAMP_MASK);
			}

			fr += fs;
			DELTA(rp, ri->ri_stride, int32_t *);
			if (ri->ri_hwbits)
				DELTA(hrp, ri->ri_stride, int32_t *);
		}
	}

	/* Do underline */
	if ((attr & 1) != 0) {
		DELTA(rp, -(ri->ri_stride << 1), int32_t *);
		rp[0] = rp[1] = rp[2] = stamp[15];
		if (ri->ri_hwbits) {
			DELTA(hrp, -(ri->ri_stride << 1), int32_t *);
			hrp[0] = stamp[15];
			hrp[1] = stamp[15];
			hrp[2] = stamp[15];
		}
	}

	stamp_mutex--;
}

/*
 * Put a single character. This is for 16-pixel wide fonts.
 */
static void
rasops8_putchar16(void *cookie, int row, int col, u_int uc, long attr)
{
	struct rasops_info *ri = (struct rasops_info *)cookie;
	struct wsdisplay_font *font = PICK_FONT(ri, uc);
	int height, fs;
	int32_t *rp, *hrp;
	u_char *fr;

	/* Can't risk remaking the stamp if it's already in use */
	if (stamp_mutex++) {
		stamp_mutex--;
		rasops8_putchar(cookie, row, col, uc, attr);
		return;
	}

	hrp = NULL;

	if (!CHAR_IN_FONT(uc, font))
		return;

#ifdef RASOPS_CLIPPING
	if ((unsigned)row >= (unsigned)ri->ri_rows) {
		stamp_mutex--;
		return;
	}

	if ((unsigned)col >= (unsigned)ri->ri_cols) {
		stamp_mutex--;
		return;
	}
#endif

	/* Recompute stamp? */
	if (attr != stamp_attr)
		rasops8_makestamp(ri, attr);

	rp = (int32_t *)(ri->ri_bits + row*ri->ri_yscale + col*ri->ri_xscale);
	if (ri->ri_hwbits)
		hrp = (int32_t *)(ri->ri_hwbits + row*ri->ri_yscale +
		    col*ri->ri_xscale);

	height = font->fontheight;

	if (uc == ' ') {
		while (height--) {
			rp[0] = rp[1] = rp[2] = rp[3] = stamp[0];
			if (ri->ri_hwbits) {
				hrp[0] = stamp[0];
				hrp[1] = stamp[0];
				hrp[2] = stamp[0];
				hrp[3] = stamp[0];
			}
		}
	} else {
		fr = WSFONT_GLYPH(uc, font);
		fs = font->stride;

		while (height--) {
			rp[0] = STAMP_READ(STAMP_SHIFT(fr[0], 1) & STAMP_MASK);
			rp[1] = STAMP_READ(STAMP_SHIFT(fr[0], 0) & STAMP_MASK);
			rp[2] = STAMP_READ(STAMP_SHIFT(fr[1], 1) & STAMP_MASK);
			rp[3] = STAMP_READ(STAMP_SHIFT(fr[1], 0) & STAMP_MASK);
			if (ri->ri_hwbits) {
				hrp[0] = STAMP_READ(STAMP_SHIFT(fr[0], 1) & STAMP_MASK);
				hrp[1] = STAMP_READ(STAMP_SHIFT(fr[0], 0) & STAMP_MASK);
				hrp[2] = STAMP_READ(STAMP_SHIFT(fr[1], 1) & STAMP_MASK);
				hrp[3] = STAMP_READ(STAMP_SHIFT(fr[1], 0) & STAMP_MASK);
			}

			fr += fs;
			DELTA(rp, ri->ri_stride, int32_t *);
			if (ri->ri_hwbits)
				DELTA(hrp, ri->ri_stride, int32_t *);
		}
	}

	/* Do underline */
	if ((attr & 1) != 0) {
		DELTA(rp, -(ri->ri_stride << 1), int32_t *);
		rp[0] = rp[1] = rp[2] = rp[3] = stamp[15];
		if (ri->ri_hwbits) {
			DELTA(hrp, -(ri->ri_stride << 1), int32_t *);
			hrp[0] = stamp[15];
			hrp[1] = stamp[15];
			hrp[2] = stamp[15];
			hrp[3] = stamp[15];
		}
	}

	stamp_mutex--;
}
#endif /* !RASOPS_SMALL */
