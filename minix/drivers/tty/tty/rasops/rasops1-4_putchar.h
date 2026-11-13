/* $NetBSD: rasops1-4_putchar.h,v 1.3.2.2 2019/08/15 12:21:27 martin Exp $ */

/* NetBSD: rasops_bitops.h,v 1.23 2019/08/02 04:39:09 rin Exp */
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

#define	PIXEL_BITS	RASOPS_DEPTH

#if   RASOPS_DEPTH == 1
#define	PIXEL_SHIFT	0
#elif RASOPS_DEPTH == 2
#define	PIXEL_SHIFT	1
#elif RASOPS_DEPTH == 4
#define	PIXEL_SHIFT	2
#else
#error "Depth not supported"
#endif

#ifndef RASOPS_AA
#define	COLOR_MASK	__BITS(32 - PIXEL_BITS, 31)
#else
#  if RASOPS_DEPTH == 2
#define	COLOR_MASK	0x3
#  else
#error "Anti-aliasing not supported"
#  endif
#endif

#ifndef RASOPS_AA
#define	PIXEL_OR(tmp)							\
	do {								\
		(tmp) |= clr[(fb >> 31) & 1] >> bit;			\
		fb <<= 1;						\
	} while (0 /* CONSTCOND */)
#else
#define	PIXEL_OR(tmp)							\
	do {								\
		uint8_t c, w = *fr++;					\
		if (w == 0xff)						\
			c = clr[1];					\
		else if (w == 0)					\
			c = clr[0];					\
		else							\
			c = (w * clr[1] + (0xff - w) * clr[0]) >> 8;	\
		(tmp) |= c << (32 - PIXEL_BITS - bit);			\
	} while (0 /* CONSTCOND */)
#endif

#define	NAME(depth)	NAME1(depth)
#ifndef RASOPS_AA
#define	NAME1(depth)	rasops ## depth ## _ ## putchar
#else
#define	NAME1(depth)	rasops ## depth ## _ ## putchar_aa
#endif

/*
 * Paint a single character. This function is also applicable to
 * monochrome, but that in rasops1.c is much simpler and faster.
 */
static void
NAME(RASOPS_DEPTH)(void *cookie, int row, int col, u_int uc, long attr)
{
	struct rasops_info *ri = (struct rasops_info *)cookie;
	struct wsdisplay_font *font = PICK_FONT(ri, uc);
	int height, width, full, cnt, bit;
	uint32_t bg, fg, lbg, rbg, clr[2], lmask, rmask, tmp;
	uint32_t *rp, *bp, *hp;
	uint8_t *fr;
	bool space;

	hp = NULL;	/* XXX GCC */

	if (__predict_false(!CHAR_IN_FONT(uc, font)))
		return;

#ifdef RASOPS_CLIPPING
	/* Catches 'row < 0' case too */
	if ((unsigned)row >= (unsigned)ri->ri_rows)
		return;

	if ((unsigned)col >= (unsigned)ri->ri_cols)
		return;
#endif

	height = font->fontheight;
	width = font->fontwidth << PIXEL_SHIFT;
	col *= width;

	rp = (uint32_t *)(ri->ri_bits + row * ri->ri_yscale +
	    ((col >> 3) & ~3));
	if (ri->ri_hwbits)
		hp = (uint32_t *)(ri->ri_hwbits + row * ri->ri_yscale +
		    ((col >> 3) & ~3));

	col &= 31;

	bg = ATTR_BG(ri, attr);
	fg = ATTR_FG(ri, attr);

	/* If fg and bg match this becomes a space character */
	if (uc == ' ' || __predict_false(fg == bg)) {
		space = true;
		fr = NULL;	/* XXX GCC */
	} else {
		space = false;
		fr = FONT_GLYPH(uc, font, ri);
	}

	if (col + width <= 32) {
		/* Single word, only one mask */
		rmask = rasops_pmask[col][width & 31];
		lmask = ~rmask;

		if (space) {
			bg &= rmask;
			while (height--) {
				tmp = (*rp & lmask) | bg;
				*rp = tmp;
				if (ri->ri_hwbits) {
					*hp = tmp;
					DELTA(hp, ri->ri_stride, uint32_t *);
				}
				DELTA(rp, ri->ri_stride, uint32_t *);
			}
		} else {
			clr[0] = bg & COLOR_MASK;
			clr[1] = fg & COLOR_MASK;

			while (height--) {
#ifndef RASOPS_AA
				uint32_t fb = rasops_be32uatoh(fr);
				fr += ri->ri_font->stride;
#endif

				tmp = 0;
				for (bit = col; bit < col + width;
				    bit += PIXEL_BITS)
					PIXEL_OR(tmp);
				tmp = (*rp & lmask) | MBE(tmp);
				*rp = tmp;

				if (ri->ri_hwbits) {
					*hp = tmp;
					DELTA(hp, ri->ri_stride, uint32_t *);
				}

				DELTA(rp, ri->ri_stride, uint32_t *);
			}
		}

		/* Do underline */
		if ((attr & WSATTR_UNDERLINE) != 0) {
			DELTA(rp, - ri->ri_stride * ri->ri_ul.off, uint32_t *);
			if (ri->ri_hwbits)
				DELTA(hp, - ri->ri_stride * ri->ri_ul.off,
				    uint32_t *);

			for (height = ri->ri_ul.height; height; height--) {
				DELTA(rp, - ri->ri_stride, uint32_t *);
				tmp = (*rp & lmask) | (fg & rmask);
				*rp = tmp;
				if (ri->ri_hwbits) {
					DELTA(hp, - ri->ri_stride, uint32_t *);
					*hp = tmp;
				}
			}
		}

		return;
	}

	/* Word boundary, two masks needed */
	lmask = ~rasops_lmask[col];
	rmask = ~rasops_rmask[(col + width) & 31];

	if (lmask != -1)
		width -= 32 - col;

	full = width / 32;
	width -= full * 32;

	if (space) {
		lbg = bg & ~lmask;
		rbg = bg & ~rmask;

		while (height--) {
			bp = rp;

			if (lmask != -1) {
				*bp = (*bp & lmask) | lbg;
				bp++;
			}

			for (cnt = full; cnt; cnt--)
				*bp++ = bg;

			if (rmask != -1)
				*bp = (*bp & rmask) | rbg;

			if (ri->ri_hwbits) {
				memcpy(hp, rp, ((lmask != -1) + full +
				    (rmask != -1)) << 2);
				DELTA(hp, ri->ri_stride, uint32_t *);
			}

			DELTA(rp, ri->ri_stride, uint32_t *);
		}
	} else {
		clr[0] = bg & COLOR_MASK;
		clr[1] = fg & COLOR_MASK;

		while (height--) {
			bp = rp;

#ifndef RASOPS_AA
			uint32_t fb = rasops_be32uatoh(fr);
			fr += ri->ri_font->stride;
#endif

			if (lmask != -1) {
				tmp = 0;
				for (bit = col; bit < 32; bit += PIXEL_BITS)
					PIXEL_OR(tmp);
				*bp = (*bp & lmask) | MBE(tmp);
				bp++;
			}

			for (cnt = full; cnt; cnt--) {
				tmp = 0;
				for (bit = 0; bit < 32; bit += PIXEL_BITS)
					PIXEL_OR(tmp);
				*bp++ = MBE(tmp);
			}

			if (rmask != -1) {
				tmp = 0;
				for (bit = 0; bit < width; bit += PIXEL_BITS)
					PIXEL_OR(tmp);
				*bp = (*bp & rmask) | MBE(tmp);
			}

			if (ri->ri_hwbits) {
				memcpy(hp, rp, ((lmask != -1) + full +
				    (rmask != -1)) << 2);
				DELTA(hp, ri->ri_stride, uint32_t *);
			}

			DELTA(rp, ri->ri_stride, uint32_t *);
		}
	}

	/* Do underline */
	if ((attr & WSATTR_UNDERLINE) != 0) {
		DELTA(rp, - ri->ri_stride * ri->ri_ul.off, uint32_t *);
		if (ri->ri_hwbits)
			DELTA(hp, - ri->ri_stride * ri->ri_ul.off, uint32_t *);

		for (height = ri->ri_ul.height; height; height--) {
			DELTA(rp, - ri->ri_stride, uint32_t *);
			bp = rp;
			if (lmask != -1) {
				*bp = (*bp & lmask) | (fg & ~lmask);
				bp++;
			}
			for (cnt = full; cnt; cnt--)
				*bp++ = fg;
			if (rmask != -1)
				*bp = (*bp & rmask) | (fg & ~rmask);
			if (ri->ri_hwbits) {
				DELTA(hp, - ri->ri_stride, uint32_t *);
				memcpy(hp, rp, ((lmask != -1) + full +
				    (rmask != -1)) << 2);
			}
		}
	}
}

#undef	PIXEL_BITS
#undef	PIXEL_SHIFT
#undef	COLOR_MASK

#undef	PIXEL_OR

#undef	NAME
#undef	NAME1
