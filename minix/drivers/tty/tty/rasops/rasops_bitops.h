/* 	$NetBSD: rasops_bitops.h,v 1.18.2.1 2019/08/15 12:21:27 martin Exp $	*/

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

#ifndef _RASOPS_BITOPS_H_
#define _RASOPS_BITOPS_H_ 1

#if   RASOPS_DEPTH == 1
#define	PIXEL_SHIFT	0
#elif RASOPS_DEPTH == 2
#define	PIXEL_SHIFT	1
#elif RASOPS_DEPTH == 4
#define	PIXEL_SHIFT	2
#else
#error "Depth not supported"
#endif

#define	NAME(name)		NAME1(RASOPS_DEPTH, name)
#define	NAME1(depth, name)	NAME2(depth, name)
#define	NAME2(depth, name)	rasops ## depth ## _ ## name

/*
 * Erase columns.
 */
static void
NAME(erasecols)(void *cookie, int row, int col, int num, long attr)
{
	struct rasops_info *ri = (struct rasops_info *)cookie;
	int height, cnt;
	uint32_t bg, lbg, rbg, lmask, rmask, tmp;
	uint32_t *dp, *rp, *hp;

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
	col *= ri->ri_font->fontwidth << PIXEL_SHIFT;
	num *= ri->ri_font->fontwidth << PIXEL_SHIFT;

	rp = (uint32_t *)(ri->ri_bits + row*ri->ri_yscale + ((col >> 3) & ~3));
	if (ri->ri_hwbits)
		hp = (uint32_t *)(ri->ri_hwbits + row*ri->ri_yscale +
		    ((col >> 3) & ~3));

	col &= 31;

	bg = ATTR_BG(ri, attr);

	if (col + num <= 32) {
		lmask = ~rasops_pmask[col][num & 31];
		bg &= ~lmask;

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
		lmask = rasops_rmask[col];
		rmask = rasops_lmask[(col + num) & 31];

		if (lmask)
			num = (num - (32 - col)) >> 5;
		else
			num = num >> 5;

		lbg = bg & ~lmask;
		rbg = bg & ~rmask;

		while (height--) {
			dp = rp;

			if (lmask) {
				*dp = (*dp & lmask) | lbg;
				dp++;
			}

			for (cnt = num; cnt > 0; cnt--)
				*dp++ = bg;

			if (rmask)
				*dp = (*dp & rmask) | rbg;

			if (ri->ri_hwbits) {
				memcpy(hp, rp, ((lmask != 0) + num +
				    (rmask != 0)) << 2);
				DELTA(hp, ri->ri_stride, uint32_t *);
			}

			DELTA(rp, ri->ri_stride, uint32_t *);
		}
	}
}

/*
 * Actually paint the cursor.
 */
static void
NAME(do_cursor)(struct rasops_info *ri)
{
	int row, col, height, width, cnt;
	uint32_t lmask, rmask, tmp;
	uint32_t *dp, *rp, *hp;

	hp = NULL;	/* XXX GCC */

	row = ri->ri_crow;
	col = ri->ri_ccol * ri->ri_font->fontwidth << PIXEL_SHIFT;

	height = ri->ri_font->fontheight;
	width = ri->ri_font->fontwidth << PIXEL_SHIFT;

	rp = (uint32_t *)(ri->ri_bits + row * ri->ri_yscale +
	    ((col >> 3) & ~3));
	if (ri->ri_hwbits)
		hp = (uint32_t *)(ri->ri_hwbits + row * ri->ri_yscale +
		    ((col >> 3) & ~3));

	col &= 31;

	if (col + width <= 32) {
		lmask = rasops_pmask[col][width & 31];

		while (height--) {
			tmp = *rp ^ lmask;
			*rp = tmp;
			if (ri->ri_hwbits) {
				*hp = tmp;
				DELTA(hp, ri->ri_stride, uint32_t *);
			}
			DELTA(rp, ri->ri_stride, uint32_t *);
		}
	} else {
		lmask = ~rasops_rmask[col];
		rmask = ~rasops_lmask[(col + width) & 31];

		if (lmask != -1)
			width = (width - (32 - col)) >> 5;
		else
			width = width >> 5;

		while (height--) {
			dp = rp;

			if (lmask != -1)
				*dp++ ^= lmask;

			for (cnt = width; cnt; cnt--) {
				*dp = ~*dp;
				dp++;
			}

			if (rmask != -1)
				*dp ^= rmask;

			if (ri->ri_hwbits) {
				memcpy(hp, rp, ((lmask != -1) + width +
				    (rmask != -1)) << 2);
				DELTA(hp, ri->ri_stride, uint32_t *);
			}

			DELTA(rp, ri->ri_stride, uint32_t *);
		}
	}
}

/*
 * Copy columns. Ick!
 */
static void
NAME(copycols)(void *cookie, int row, int src, int dst, int num)
{
	struct rasops_info *ri = (struct rasops_info *)cookie;
	int height, width, lnum, rnum, sb, db, full, cnt, sboff;
	uint32_t lmask, rmask, tmp;
	uint32_t *sp, *dp, *srp, *drp, *hp;
	bool sbover;

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
	width = ri->ri_font->fontwidth << PIXEL_SHIFT;

	row *= ri->ri_yscale;

	src *= width;
	dst *= width;
	num *= width;

	sb = src & 31;
	db = dst & 31;

	if (db + num <= 32) {
		/* Destination is contained within a single word */
		srp = (uint32_t *)(ri->ri_bits + row + ((src >> 3) & ~3));
		drp = (uint32_t *)(ri->ri_bits + row + ((dst >> 3) & ~3));
		if (ri->ri_hwbits)
			hp = (uint32_t *)(ri->ri_hwbits + row +
			    ((dst >> 3) & ~3));

		while (height--) {
			GETBITS(srp, sb, num, tmp);
			PUTBITS(tmp, db, num, drp);
			if (ri->ri_hwbits) {
				*hp = *drp;
				DELTA(hp, ri->ri_stride, uint32_t *);
			}	
			DELTA(srp, ri->ri_stride, uint32_t *);
			DELTA(drp, ri->ri_stride, uint32_t *);
		}

		return;
	}

	lmask = rasops_rmask[db];
	rmask = rasops_lmask[(dst + num) & 31];
	lnum = (32 - db) & 31;
	rnum = (dst + num) & 31;

	if (lmask != 0)
		full = (num - lnum) >> 5;
	else
		full = num >> 5;

	if (src < dst && src + num > dst) {
		/* Copy right-to-left */
		srp = (uint32_t *)(ri->ri_bits + row +
		    (((src + num) >> 3) & ~3));
		drp = (uint32_t *)(ri->ri_bits + row +
		    (((dst + num) >> 3) & ~3));
		if (ri->ri_hwbits) {
			hp = (uint32_t *)(ri->ri_hwbits + row +
			    (((dst + num) >> 3) & ~3));
			hp -= (lmask != 0) + full;
		}

		sboff = (src + num) & 31;
		sbover = sb + lnum >= 32;
		if ((sboff -= rnum) < 0) {
			srp--;
			sboff += 32;
		}

		while (height--) {
			sp = srp;
			dp = drp;

			if (rmask != 0) {
				GETBITS(sp, sboff, rnum, tmp);
				PUTBITS(tmp, 0, rnum, dp);
			}

			/* Now aligned to 32-bits wrt dp */
			for (cnt = full; cnt; cnt--) {
				--dp;
				--sp;
				GETBITS(sp, sboff, 32, tmp);
				*dp = tmp;
			}

			if (lmask != 0) {
				if (sbover)
					--sp;
				--dp;
				GETBITS(sp, sb, lnum, tmp);
				PUTBITS(tmp, db, lnum, dp);
 			}

			if (ri->ri_hwbits) {
				memcpy(hp, dp, ((lmask != 0) + full +
				    (rmask != 0)) << 2);
				DELTA(hp, ri->ri_stride, uint32_t *);
			}

			DELTA(srp, ri->ri_stride, uint32_t *);
			DELTA(drp, ri->ri_stride, uint32_t *);
 		}
	} else {
		/* Copy left-to-right */
		srp = (uint32_t *)(ri->ri_bits + row + ((src >> 3) & ~3));
		drp = (uint32_t *)(ri->ri_bits + row + ((dst >> 3) & ~3));
		if (ri->ri_hwbits)
			hp = (uint32_t *)(ri->ri_hwbits + row +
			    ((dst >> 3) & ~3));

		while (height--) {
			sp = srp;
			dp = drp;

			sboff = sb;

			if (lmask != 0) {
				GETBITS(sp, sboff, lnum, tmp);
				PUTBITS(tmp, db, lnum, dp);
				dp++;

				if ((sboff += lnum) > 31) {
					sp++;
					sboff -= 32;
				}
			}

			/* Now aligned to 32-bits wrt dp */
			for (cnt = full; cnt; cnt--, sp++) {
				GETBITS(sp, sboff, 32, tmp);
				*dp++ = tmp;
			}

			if (rmask != 0) {
				GETBITS(sp, sboff, rnum, tmp);
				PUTBITS(tmp, 0, rnum, dp);
 			}

			if (ri->ri_hwbits) {
				memcpy(hp, drp, ((lmask != 0) + full +
				    (rmask != 0)) << 2);
				DELTA(hp, ri->ri_stride, uint32_t *);
			}

			DELTA(srp, ri->ri_stride, uint32_t *);
			DELTA(drp, ri->ri_stride, uint32_t *);
 		}
 	}
}

#undef	PIXEL_SHIFT

#undef	NAME
#undef	NAME1
#undef	NAME2

#endif /* _RASOPS_BITOPS_H_ */
