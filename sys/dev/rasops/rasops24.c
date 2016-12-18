/* 	$NetBSD: rasops24.c,v 1.29 2011/07/25 18:02:47 njoly Exp $	*/

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
__KERNEL_RCSID(0, "$NetBSD: rasops24.c,v 1.29 2011/07/25 18:02:47 njoly Exp $");

#include "opt_rasops.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>

#include <machine/endian.h>
#include <sys/bswap.h>

#include <dev/wscons/wsdisplayvar.h>
#include <dev/wscons/wsconsio.h>
#include <dev/rasops/rasops.h>

static void 	rasops24_erasecols(void *, int, int, int, long);
static void 	rasops24_eraserows(void *, int, int, long);
static void 	rasops24_putchar(void *, int, int, u_int, long attr);
#ifndef RASOPS_SMALL
static void 	rasops24_putchar8(void *, int, int, u_int, long attr);
static void 	rasops24_putchar12(void *, int, int, u_int, long attr);
static void 	rasops24_putchar16(void *, int, int, u_int, long attr);
static void	rasops24_makestamp(struct rasops_info *, long);

/*
 * 4x1 stamp for optimized character blitting
 */
static int32_t	stamp[64];
static long	stamp_attr;
static int	stamp_mutex;	/* XXX see note in readme */
#endif

/*
 * XXX this confuses the hell out of gcc2 (not egcs) which always insists
 * that the shift count is negative.
 *
 * offset = STAMP_SHIFT(fontbits, nibble #) & STAMP_MASK
 * destination int32_t[0] = STAMP_READ(offset)
 * destination int32_t[1] = STAMP_READ(offset + 4)
 * destination int32_t[2] = STAMP_READ(offset + 8)
 */
#define STAMP_SHIFT(fb,n)	((n*4-4) >= 0 ? (fb)>>(n*4-4):(fb)<<-(n*4-4))
#define STAMP_MASK		(0xf << 4)
#define STAMP_READ(o)		(*(int32_t *)((char *)stamp + (o)))

/*
 * Initialize rasops_info struct for this colordepth.
 */
void
rasops24_init(struct rasops_info *ri)
{

	switch (ri->ri_font->fontwidth) {
#ifndef RASOPS_SMALL
	case 8:
		ri->ri_ops.putchar = rasops24_putchar8;
		break;
	case 12:
		ri->ri_ops.putchar = rasops24_putchar12;
		break;
	case 16:
		ri->ri_ops.putchar = rasops24_putchar16;
		break;
#endif
	default:
		ri->ri_ops.putchar = rasops24_putchar;
		break;
	}

	if (ri->ri_rnum == 0) {
		ri->ri_rnum = 8;
		ri->ri_rpos = 0;
		ri->ri_gnum = 8;
		ri->ri_gpos = 8;
		ri->ri_bnum = 8;
		ri->ri_bpos = 16;
	}

	ri->ri_ops.erasecols = rasops24_erasecols;
	ri->ri_ops.eraserows = rasops24_eraserows;
}

/*
 * Put a single character. This is the generic version.
 * XXX this bites - we should use masks.
 */
static void
rasops24_putchar(void *cookie, int row, int col, u_int uc, long attr)
{
	int fb, width, height, cnt, clr[2];
	struct rasops_info *ri = (struct rasops_info *)cookie;
	struct wsdisplay_font *font = PICK_FONT(ri, uc);
	u_char *dp, *rp, *fr;

#ifdef RASOPS_CLIPPING
	/* Catches 'row < 0' case too */
	if ((unsigned)row >= (unsigned)ri->ri_rows)
		return;

	if ((unsigned)col >= (unsigned)ri->ri_cols)
		return;
#endif

	rp = ri->ri_bits + row * ri->ri_yscale + col * ri->ri_xscale;
	height = font->fontheight;
	width = font->fontwidth;

	clr[1] = ri->ri_devcmap[((u_int)attr >> 24) & 0xf];
	clr[0] = ri->ri_devcmap[((u_int)attr >> 16) & 0xf];

	if (uc == ' ') {
		u_char c = clr[0];
		while (height--) {
			dp = rp;
			rp += ri->ri_stride;

			for (cnt = width; cnt; cnt--) {
				*dp++ = c >> 16;
				*dp++ = c >> 8;
				*dp++ = c;
			}
		}
	} else {
		uc -= font->firstchar;
		fr = (u_char *)font->data + uc * ri->ri_fontscale;

		while (height--) {
			dp = rp;
			fb = fr[3] | (fr[2] << 8) | (fr[1] << 16) |
			    (fr[0] << 24);
			fr += font->stride;
			rp += ri->ri_stride;

			for (cnt = width; cnt; cnt--, fb <<= 1) {
				if ((fb >> 31) & 1) {
					*dp++ = clr[1] >> 16;
					*dp++ = clr[1] >> 8;
					*dp++ = clr[1];
				} else {
					*dp++ = clr[0] >> 16;
					*dp++ = clr[0] >> 8;
					*dp++ = clr[0];
				}
			}
		}
	}

	/* Do underline */
	if ((attr & 1) != 0) {
		u_char c = clr[1];

		rp -= ri->ri_stride << 1;

		while (width--) {
			*rp++ = c >> 16;
			*rp++ = c >> 8;
			*rp++ = c;
		}
	}
}

#ifndef RASOPS_SMALL
/*
 * Recompute the blitting stamp.
 */
static void
rasops24_makestamp(struct rasops_info *ri, long attr)
{
	u_int fg, bg, c1, c2, c3, c4;
	int i;

	fg = ri->ri_devcmap[((u_int)attr >> 24) & 0xf] & 0xffffff;
	bg = ri->ri_devcmap[((u_int)attr >> 16) & 0xf] & 0xffffff;
	stamp_attr = attr;

	for (i = 0; i < 64; i += 4) {
#if BYTE_ORDER == LITTLE_ENDIAN
		c1 = (i & 32 ? fg : bg);
		c2 = (i & 16 ? fg : bg);
		c3 = (i & 8 ? fg : bg);
		c4 = (i & 4 ? fg : bg);
#else
		c1 = (i & 8 ? fg : bg);
		c2 = (i & 4 ? fg : bg);
		c3 = (i & 16 ? fg : bg);
		c4 = (i & 32 ? fg : bg);
#endif
		stamp[i+0] = (c1 <<  8) | (c2 >> 16);
		stamp[i+1] = (c2 << 16) | (c3 >>  8);
		stamp[i+2] = (c3 << 24) | c4;

#if BYTE_ORDER == LITTLE_ENDIAN
		if ((ri->ri_flg & RI_BSWAP) == 0) {
#else
		if ((ri->ri_flg & RI_BSWAP) != 0) {
#endif
			stamp[i+0] = bswap32(stamp[i+0]);
			stamp[i+1] = bswap32(stamp[i+1]);
			stamp[i+2] = bswap32(stamp[i+2]);
		}
	}
}

/*
 * Put a single character. This is for 8-pixel wide fonts.
 */
static void
rasops24_putchar8(void *cookie, int row, int col, u_int uc, long attr)
{
	struct rasops_info *ri = (struct rasops_info *)cookie;
	struct wsdisplay_font *font = PICK_FONT(ri, uc);
	int height, so, fs;
	int32_t *rp;
	u_char *fr;

	/* Can't risk remaking the stamp if it's already in use */
	if (stamp_mutex++) {
		stamp_mutex--;
		rasops24_putchar(cookie, row, col, uc, attr);
		return;
	}

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
		rasops24_makestamp(ri, attr);

	rp = (int32_t *)(ri->ri_bits + row*ri->ri_yscale + col*ri->ri_xscale);
	height = font->fontheight;

	if (uc == (u_int)-1) {
		int32_t c = stamp[0];
		while (height--) {
			rp[0] = rp[1] = rp[2] = rp[3] = rp[4] = rp[5] = c;
			DELTA(rp, ri->ri_stride, int32_t *);
		}
	} else {
		uc -= font->firstchar;
		fr = (u_char *)font->data + uc*ri->ri_fontscale;
		fs = font->stride;

		while (height--) {
			so = STAMP_SHIFT(fr[0], 1) & STAMP_MASK;
			rp[0] = STAMP_READ(so);
			rp[1] = STAMP_READ(so + 4);
			rp[2] = STAMP_READ(so + 8);

			so = STAMP_SHIFT(fr[0], 0) & STAMP_MASK;
			rp[3] = STAMP_READ(so);
			rp[4] = STAMP_READ(so + 4);
			rp[5] = STAMP_READ(so + 8);

			fr += fs;
			DELTA(rp, ri->ri_stride, int32_t *);
		}
	}

	/* Do underline */
	if ((attr & 1) != 0) {
		int32_t c = STAMP_READ(52);

		DELTA(rp, -(ri->ri_stride << 1), int32_t *);
		rp[0] = rp[1] = rp[2] = rp[3] = rp[4] = rp[5] = c;
	}

	stamp_mutex--;
}

/*
 * Put a single character. This is for 12-pixel wide fonts.
 */
static void
rasops24_putchar12(void *cookie, int row, int col, u_int uc, long attr)
{
	struct rasops_info *ri = (struct rasops_info *)cookie;
	struct wsdisplay_font *font = PICK_FONT(ri, uc);
	int height, so, fs;
	int32_t *rp;
	u_char *fr;

	/* Can't risk remaking the stamp if it's already in use */
	if (stamp_mutex++) {
		stamp_mutex--;
		rasops24_putchar(cookie, row, col, uc, attr);
		return;
	}

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
		rasops24_makestamp(ri, attr);

	rp = (int32_t *)(ri->ri_bits + row*ri->ri_yscale + col*ri->ri_xscale);
	height = font->fontheight;

	if (uc == (u_int)-1) {
		int32_t c = stamp[0];
		while (height--) {
			rp[0] = rp[1] = rp[2] = rp[3] =
			rp[4] = rp[5] = rp[6] = rp[7] = rp[8] = c;
			DELTA(rp, ri->ri_stride, int32_t *);
		}
	} else {
		uc -= font->firstchar;
		fr = (u_char *)font->data + uc*ri->ri_fontscale;
		fs = font->stride;

		while (height--) {
			so = STAMP_SHIFT(fr[0], 1) & STAMP_MASK;
			rp[0] = STAMP_READ(so);
			rp[1] = STAMP_READ(so + 4);
			rp[2] = STAMP_READ(so + 8);

			so = STAMP_SHIFT(fr[0], 0) & STAMP_MASK;
			rp[3] = STAMP_READ(so);
			rp[4] = STAMP_READ(so + 4);
			rp[5] = STAMP_READ(so + 8);

			so = STAMP_SHIFT(fr[1], 1) & STAMP_MASK;
			rp[6] = STAMP_READ(so);
			rp[7] = STAMP_READ(so + 4);
			rp[8] = STAMP_READ(so + 8);

			fr += fs;
			DELTA(rp, ri->ri_stride, int32_t *);
		}
	}

	/* Do underline */
	if ((attr & 1) != 0) {
		int32_t c = STAMP_READ(52);

		DELTA(rp, -(ri->ri_stride << 1), int32_t *);
		rp[0] = rp[1] = rp[2] = rp[3] =
		rp[4] = rp[5] = rp[6] = rp[7] = rp[8] = c;
	}

	stamp_mutex--;
}

/*
 * Put a single character. This is for 16-pixel wide fonts.
 */
static void
rasops24_putchar16(void *cookie, int row, int col, u_int uc, long attr)
{
	struct rasops_info *ri = (struct rasops_info *)cookie;
	struct wsdisplay_font *font = PICK_FONT(ri, uc);
	int height, so, fs;
	int32_t *rp;
	u_char *fr;

	/* Can't risk remaking the stamp if it's already in use */
	if (stamp_mutex++) {
		stamp_mutex--;
		rasops24_putchar(cookie, row, col, uc, attr);
		return;
	}

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
		rasops24_makestamp(ri, attr);

	rp = (int32_t *)(ri->ri_bits + row*ri->ri_yscale + col*ri->ri_xscale);
	height = font->fontheight;

	if (uc == (u_int)-1) {
		int32_t c = stamp[0];
		while (height--) {
			rp[0] = rp[1] = rp[2] = rp[3] =
			rp[4] = rp[5] = rp[6] = rp[7] =
			rp[8] = rp[9] = rp[10] = rp[11] = c;
			DELTA(rp, ri->ri_stride, int32_t *);
		}
	} else {
		uc -= font->firstchar;
		fr = (u_char *)font->data + uc*ri->ri_fontscale;
		fs = font->stride;

		while (height--) {
			so = STAMP_SHIFT(fr[0], 1) & STAMP_MASK;
			rp[0] = STAMP_READ(so);
			rp[1] = STAMP_READ(so + 4);
			rp[2] = STAMP_READ(so + 8);

			so = STAMP_SHIFT(fr[0], 0) & STAMP_MASK;
			rp[3] = STAMP_READ(so);
			rp[4] = STAMP_READ(so + 4);
			rp[5] = STAMP_READ(so + 8);

			so = STAMP_SHIFT(fr[1], 1) & STAMP_MASK;
			rp[6] = STAMP_READ(so);
			rp[7] = STAMP_READ(so + 4);
			rp[8] = STAMP_READ(so + 8);

			so = STAMP_SHIFT(fr[1], 0) & STAMP_MASK;
			rp[9] = STAMP_READ(so);
			rp[10] = STAMP_READ(so + 4);
			rp[11] = STAMP_READ(so + 8);

			DELTA(rp, ri->ri_stride, int32_t *);
			fr += fs;
		}
	}

	/* Do underline */
	if ((attr & 1) != 0) {
		int32_t c = STAMP_READ(52);

		DELTA(rp, -(ri->ri_stride << 1), int32_t *);
		rp[0] = rp[1] = rp[2] = rp[3] =
		rp[4] = rp[5] = rp[6] = rp[7] =
		rp[8] = rp[9] = rp[10] = rp[11] = c;
	}

	stamp_mutex--;
}
#endif	/* !RASOPS_SMALL */

/*
 * Erase rows. This is nice and easy due to alignment.
 */
static void
rasops24_eraserows(void *cookie, int row, int num, long attr)
{
	int n9, n3, n1, cnt, stride, delta;
	u_int32_t *dp, clr, xstamp[3];
	struct rasops_info *ri;

	/*
	 * If the color is gray, we can cheat and use the generic routines
	 * (which are faster, hopefully) since the r,g,b values are the same.
	 */
	if ((attr & 4) != 0) {
		rasops_eraserows(cookie, row, num, attr);
		return;
	}

	ri = (struct rasops_info *)cookie;

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

	clr = ri->ri_devcmap[(attr >> 16) & 0xf] & 0xffffff;
	xstamp[0] = (clr <<  8) | (clr >> 16);
	xstamp[1] = (clr << 16) | (clr >>  8);
	xstamp[2] = (clr << 24) | clr;

#if BYTE_ORDER == LITTLE_ENDIAN
	if ((ri->ri_flg & RI_BSWAP) == 0) {
#else
	if ((ri->ri_flg & RI_BSWAP) != 0) {
#endif
		xstamp[0] = bswap32(xstamp[0]);
		xstamp[1] = bswap32(xstamp[1]);
		xstamp[2] = bswap32(xstamp[2]);
	}

	/*
	 * XXX the wsdisplay_emulops interface seems a little deficient in
	 * that there is no way to clear the *entire* screen. We provide a
	 * workaround here: if the entire console area is being cleared, and
	 * the RI_FULLCLEAR flag is set, clear the entire display.
	 */
	if (num == ri->ri_rows && (ri->ri_flg & RI_FULLCLEAR) != 0) {
		stride = ri->ri_stride;
		num = ri->ri_height;
		dp = (int32_t *)ri->ri_origbits;
		delta = 0;
	} else {
		stride = ri->ri_emustride;
		num *= ri->ri_font->fontheight;
		dp = (int32_t *)(ri->ri_bits + row * ri->ri_yscale);
		delta = ri->ri_delta;
	}

	n9 = stride / 36;
	cnt = (n9 << 5) + (n9 << 2); /* (32*n9) + (4*n9) */
	n3 = (stride - cnt) / 12;
	cnt += (n3 << 3) + (n3 << 2); /* (8*n3) + (4*n3) */
	n1 = (stride - cnt) >> 2;

	while (num--) {
		for (cnt = n9; cnt; cnt--) {
			dp[0] = xstamp[0];
			dp[1] = xstamp[1];
			dp[2] = xstamp[2];
			dp[3] = xstamp[0];
			dp[4] = xstamp[1];
			dp[5] = xstamp[2];
			dp[6] = xstamp[0];
			dp[7] = xstamp[1];
			dp[8] = xstamp[2];
			dp += 9;
		}

		for (cnt = n3; cnt; cnt--) {
			dp[0] = xstamp[0];
			dp[1] = xstamp[1];
			dp[2] = xstamp[2];
			dp += 3;
		}

		for (cnt = 0; cnt < n1; cnt++)
			*dp++ = xstamp[cnt];

		DELTA(dp, delta, int32_t *);
	}
}

/*
 * Erase columns.
 */
static void
rasops24_erasecols(void *cookie, int row, int col, int num, long attr)
{
	int n12, n4, height, cnt, slop, clr, xstamp[3];
	struct rasops_info *ri;
	int32_t *dp, *rp;
	u_char *dbp;

	/*
	 * If the color is gray, we can cheat and use the generic routines
	 * (which are faster, hopefully) since the r,g,b values are the same.
	 */
	if ((attr & 4) != 0) {
		rasops_erasecols(cookie, row, col, num, attr);
		return;
	}

	ri = (struct rasops_info *)cookie;

#ifdef RASOPS_CLIPPING
	/* Catches 'row < 0' case too */
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

	rp = (int32_t *)(ri->ri_bits + row*ri->ri_yscale + col*ri->ri_xscale);
	num *= ri->ri_font->fontwidth;
	height = ri->ri_font->fontheight;

	clr = ri->ri_devcmap[(attr >> 16) & 0xf] & 0xffffff;
	xstamp[0] = (clr <<  8) | (clr >> 16);
	xstamp[1] = (clr << 16) | (clr >>  8);
	xstamp[2] = (clr << 24) | clr;

#if BYTE_ORDER == LITTLE_ENDIAN
	if ((ri->ri_flg & RI_BSWAP) == 0) {
#else
	if ((ri->ri_flg & RI_BSWAP) != 0) {
#endif
		xstamp[0] = bswap32(xstamp[0]);
		xstamp[1] = bswap32(xstamp[1]);
		xstamp[2] = bswap32(xstamp[2]);
	}

	/*
	 * The current byte offset mod 4 tells us the number of 24-bit pels
	 * we need to write for alignment to 32-bits. Once we're aligned on
	 * a 32-bit boundary, we're also aligned on a 4 pixel boundary, so
	 * the stamp does not need to be rotated. The following shows the
	 * layout of 4 pels in a 3 word region and illustrates this:
	 *
	 *	aaab bbcc cddd
	 */
	slop = (int)(long)rp & 3;	num -= slop;
	n12 = num / 12;		num -= (n12 << 3) + (n12 << 2);
	n4 = num >> 2;		num &= 3;

	while (height--) {
		dbp = (u_char *)rp;
		DELTA(rp, ri->ri_stride, int32_t *);

		/* Align to 4 bytes */
		/* XXX handle with masks, bring under control of RI_BSWAP */
		for (cnt = slop; cnt; cnt--) {
			*dbp++ = (clr >> 16);
			*dbp++ = (clr >> 8);
			*dbp++ = clr;
		}

		dp = (int32_t *)dbp;

		/* 12 pels per loop */
		for (cnt = n12; cnt; cnt--) {
			dp[0] = xstamp[0];
			dp[1] = xstamp[1];
			dp[2] = xstamp[2];
			dp[3] = xstamp[0];
			dp[4] = xstamp[1];
			dp[5] = xstamp[2];
			dp[6] = xstamp[0];
			dp[7] = xstamp[1];
			dp[8] = xstamp[2];
			dp += 9;
		}

		/* 4 pels per loop */
		for (cnt = n4; cnt; cnt--) {
			dp[0] = xstamp[0];
			dp[1] = xstamp[1];
			dp[2] = xstamp[2];
			dp += 3;
		}

		/* Trailing slop */
		/* XXX handle with masks, bring under control of RI_BSWAP */
		dbp = (u_char *)dp;
		for (cnt = num; cnt; cnt--) {
			*dbp++ = (clr >> 16);
			*dbp++ = (clr >> 8);
			*dbp++ = clr;
		}
	}
}
