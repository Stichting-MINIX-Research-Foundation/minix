/* 	$NetBSD: rasops24.c,v 1.39.2.1 2019/08/15 12:21:27 martin Exp $	*/

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
__KERNEL_RCSID(0, "$NetBSD: rasops24.c,v 1.39.2.1 2019/08/15 12:21:27 martin Exp $");

#ifdef _KERNEL_OPT
#include "opt_rasops.h"
#endif

#include <sys/param.h>
#include <sys/bswap.h>

#include <machine/endian.h>

#include "../wscons/wsconsio.h"
#include "../wscons/wsdisplayvar.h"

#define	_RASOPS_PRIVATE
#define	RASOPS_DEPTH	24
#include "rasops.h"

static void 	rasops24_erasecols(void *, int, int, int, long);
static void 	rasops24_eraserows(void *, int, int, long);
static void 	rasops24_putchar(void *, int, int, u_int, long);
static void 	rasops24_putchar_aa(void *, int, int, u_int, long);
static __inline void
		rasops24_makestamp1(struct rasops_info *, uint32_t *,
				    uint32_t, uint32_t, uint32_t, uint32_t);
#ifndef RASOPS_SMALL
static void 	rasops24_putchar8(void *, int, int, u_int, long);
static void 	rasops24_putchar12(void *, int, int, u_int, long);
static void 	rasops24_putchar16(void *, int, int, u_int, long);
static void	rasops24_makestamp(struct rasops_info *, long);
#endif

#ifndef RASOPS_SMALL
/* stamp for optimized character blitting */
static uint32_t			stamp[64];
static long			stamp_attr;
static struct rasops_info	*stamp_ri;

/*
 * offset = STAMP_SHIFT(fontbits, nibble #) & STAMP_MASK
 * destination uint32_t[0] = STAMP_READ(offset)
 * destination uint32_t[1] = STAMP_READ(offset + 4)
 * destination uint32_t[2] = STAMP_READ(offset + 8)
 */
#define	STAMP_SHIFT(fb, n)	((n) ? (fb) : (fb) << 4)
#define	STAMP_MASK		(0xf << 4)
#define	STAMP_READ(o)		(*(uint32_t *)((uint8_t *)stamp + (o)))
#endif

/*
 * Initialize rasops_info struct for this colordepth.
 */
void rasops24_init(struct rasops_info *ri)
{

	if (ri->ri_rnum == 0) {
		ri->ri_rnum = ri->ri_gnum = ri->ri_bnum = 8;

		ri->ri_rpos = 0;
		ri->ri_gpos = 8;
		ri->ri_bpos = 16;
	}

	ri->ri_ops.erasecols = rasops24_erasecols;
	ri->ri_ops.eraserows = rasops24_eraserows;

	if (FONT_IS_ALPHA(ri->ri_font)) {
		ri->ri_ops.putchar = rasops24_putchar_aa;
		return;
	}

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
		return;
	}

#ifndef RASOPS_SMALL
	stamp_attr = -1;
	stamp_ri = NULL;
#endif
}

/* rasops24_putchar */
#undef	RASOPS_AA
#include "rasops_putchar.h"

/* rasops24_putchar_aa */
#define	RASOPS_AA
#include "rasops_putchar.h"
#undef	RASOPS_AA

static __inline void
rasops24_makestamp1(struct rasops_info *ri, uint32_t *xstamp,
    uint32_t c1, uint32_t c2, uint32_t c3, uint32_t c4)
{

	xstamp[0] = (c1 <<  8) | (c2 >> 16);
	xstamp[1] = (c2 << 16) | (c3 >>  8);
	xstamp[2] = (c3 << 24) |  c4;

#if BYTE_ORDER == LITTLE_ENDIAN
	if ((ri->ri_flg & RI_BSWAP) == 0)
#else
	if ((ri->ri_flg & RI_BSWAP) != 0)
#endif
	{
		xstamp[0] = bswap32(xstamp[0]);
		xstamp[1] = bswap32(xstamp[1]);
		xstamp[2] = bswap32(xstamp[2]);
	}
}

#ifndef RASOPS_SMALL
/*
 * Recompute the blitting stamp.
 */
static void rasops24_makestamp(struct rasops_info *ri, long attr)
{
	int i;
	uint32_t bg, fg, c1, c2, c3, c4;

	stamp_attr = attr;
	stamp_ri = ri;

	bg = ATTR_BG(ri, attr) & 0xffffff;
	fg = ATTR_FG(ri, attr) & 0xffffff;

	for (i = 0; i < 64; i += 4) {
#if BYTE_ORDER == LITTLE_ENDIAN
		c1 = i & 32 ? fg : bg;
		c2 = i & 16 ? fg : bg;
		c3 = i &  8 ? fg : bg;
		c4 = i &  4 ? fg : bg;
#else
		c1 = i &  8 ? fg : bg;
		c2 = i &  4 ? fg : bg;
		c3 = i & 16 ? fg : bg;
		c4 = i & 32 ? fg : bg;
#endif
		rasops24_makestamp1(ri, &stamp[i], c1, c2, c3, c4);
	}
}

/*
 * Width-optimized putchar functions
 */
#define	RASOPS_WIDTH	8
#include "rasops_putchar_width.h"
#undef	RASOPS_WIDTH

#define	RASOPS_WIDTH	12
#include "rasops_putchar_width.h"
#undef	RASOPS_WIDTH

#define	RASOPS_WIDTH	16
#include "rasops_putchar_width.h"
#undef	RASOPS_WIDTH

#endif	/* !RASOPS_SMALL */

/*
 * Erase rows. This is nice and easy due to alignment.
 */
static void rasops24_eraserows(void *cookie, int row, int num, long attr)
{
	struct rasops_info *ri = (struct rasops_info *)cookie;
	int bytes, full, slop, cnt;
	uint32_t bg, xstamp[3];
	uint32_t *dp, *rp, *hp;

	hp = NULL;	/* XXX GCC */

	/*
	 * If the color is gray, we can cheat and use the generic routines
	 * (which are faster, hopefully) since the r,g,b values are the same.
	 */
	if ((attr & WSATTR_PRIVATE2) != 0) {
		rasops_eraserows(cookie, row, num, attr);
		return;
	}

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

	bg = ATTR_BG(ri, attr) & 0xffffff;
	rasops24_makestamp1(ri, xstamp, bg, bg, bg, bg);

	/*
	 * XXX the wsdisplay_emulops interface seems a little deficient in
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

	full = bytes / (4 * 3);
	slop = (bytes - full * (4 * 3)) / 4;

	while (num--) {
		dp = rp;

		for (cnt = full; cnt; cnt--) {
			dp[0] = xstamp[0];
			dp[1] = xstamp[1];
			dp[2] = xstamp[2];
			dp += 3;
		}

		for (cnt = 0; cnt < slop; cnt++)
			*dp++ = xstamp[cnt];

		if (ri->ri_hwbits) {
			memcpy(hp, rp, bytes);
			DELTA(hp, ri->ri_stride, uint32_t *);
		}

		DELTA(rp, ri->ri_stride, uint32_t *);
	}
}

/*
 * Erase columns.
 */
static void rasops24_erasecols(void *cookie, int row, int col, int num, long attr)
{
	struct rasops_info *ri = (struct rasops_info *)cookie;
	int height, slop1, slop2, full, cnt;
	uint32_t bg, xstamp[3];
	uint32_t *dp;
	uint8_t *bp, *rp, *hp;

	hp = NULL;	/* XXX GCC */

	/*
	 * If the color is gray, we can cheat and use the generic routines
	 * (which are faster, hopefully) since the r,g,b values are the same.
	 */
	if ((attr & WSATTR_PRIVATE2) != 0) {
		rasops_erasecols(cookie, row, col, num, attr);
		return;
	}

#ifdef RASOPS_CLIPPING
	/* Catches 'row < 0' case too */
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

	rp = ri->ri_bits + row * ri->ri_yscale + col * ri->ri_xscale;
	if (ri->ri_hwbits)
		hp = ri->ri_hwbits + row * ri->ri_yscale + col * ri->ri_xscale;

	bg = ATTR_BG(ri, attr) & 0xffffff;
	rasops24_makestamp1(ri, xstamp, bg, bg, bg, bg);

	/*
	 * Align to word boundary by 24-bit-wise operations:
	 *
	 * rp % 4 == 1 ---> slop1 = 3:
	 *	0123
	 *	-RGB
	 *
	 * rp % 4 == 2 ---> slop1 = 6:
	 *	0123 0123
	 *	--RG BRGB
	 *
	 * rp % 4 == 3 ---> slop1 = 9:
	 *	0123 0123 0123
	 *	---R GBRG BRGB
	 */
	slop1 = 3 * ((uintptr_t)rp % 4);
	slop2 = (num - slop1) % 12;
	full = (num - slop1 /* - slop2 */) / 12;

	while (height--) {
		/* Align to word boundary */
		bp = rp;
		for (cnt = slop1; cnt; cnt -= 3) {
			*bp++ = (bg >> 16);
			*bp++ = (bg >> 8);
			*bp++ = bg;
		}

		/* 4 pels per loop */
		dp = (uint32_t *)bp;
		for (cnt = full; cnt; cnt--) {
			dp[0] = xstamp[0];
			dp[1] = xstamp[1];
			dp[2] = xstamp[2];
			dp += 3;
		}

		/* Trailing slop */
		bp = (uint8_t *)dp;
		for (cnt = slop2; cnt; cnt -= 3) {
			*bp++ = (bg >> 16);
			*bp++ = (bg >> 8);
			*bp++ = bg;
		}

		if (ri->ri_hwbits) {
			memcpy(hp, rp, num);
			hp += ri->ri_stride;
		}

		rp += ri->ri_stride;
	}
}
