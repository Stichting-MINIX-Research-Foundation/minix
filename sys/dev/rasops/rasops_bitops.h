/* 	$NetBSD: rasops_bitops.h,v 1.15 2013/12/02 14:05:51 tsutsui Exp $	*/

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

/*
 * Erase columns.
 */
static void
NAME(erasecols)(void *cookie, int row, int col, int num, long attr)
{
	int lclr, rclr, clr;
	struct rasops_info *ri;
	uint32_t *dp, *rp, *hrp = NULL, *hp = NULL, tmp, lmask, rmask;
	int height, cnt;

	ri = (struct rasops_info *)cookie;

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
	col *= ri->ri_font->fontwidth << PIXEL_SHIFT;
	num *= ri->ri_font->fontwidth << PIXEL_SHIFT;
	height = ri->ri_font->fontheight;
	clr = ri->ri_devcmap[(attr >> 16) & 0xf];
	rp = (uint32_t *)(ri->ri_bits + row*ri->ri_yscale + ((col >> 3) & ~3));
	if (ri->ri_hwbits)
		hrp = (uint32_t *)(ri->ri_hwbits + row*ri->ri_yscale +
		    ((col >> 3) & ~3));
	if ((col & 31) + num <= 32) {
		lmask = ~rasops_pmask[col & 31][num];
		lclr = clr & ~lmask;

		while (height--) {
			dp = rp;
			DELTA(rp, ri->ri_stride, uint32_t *);

			tmp = (*dp & lmask) | lclr;
			*dp = tmp;
			if (ri->ri_hwbits) {
				*hrp = tmp;
				DELTA(hrp, ri->ri_stride, uint32_t *);
			}
		}
	} else {
		lmask = rasops_rmask[col & 31];
		rmask = rasops_lmask[(col + num) & 31];

		if (lmask)
			num = (num - (32 - (col & 31))) >> 5;
		else
			num = num >> 5;

		lclr = clr & ~lmask;
		rclr = clr & ~rmask;

		while (height--) {
			dp = rp;
			DELTA(rp, ri->ri_stride, uint32_t *);
			if (ri->ri_hwbits) {
				hp = hrp;
				DELTA(hrp, ri->ri_stride, uint32_t *);
			}

			if (lmask) {
				tmp = (*dp & lmask) | lclr;
				*dp = tmp;
				dp++;
				if (ri->ri_hwbits) {
					*hp = tmp;
					hp++;
				}
			}

			for (cnt = num; cnt > 0; cnt--)
				*dp++ = clr;
			if (ri->ri_hwbits) {
				for (cnt = num; cnt > 0; cnt--)
					*hp++ = clr;
			}

			if (rmask) {
				tmp = (*dp & rmask) | rclr;
				*dp = tmp;
				if (ri->ri_hwbits)
					*hp = tmp;
			}
		}
	}
}

/*
 * Actually paint the cursor.
 */
static void
NAME(do_cursor)(struct rasops_info *ri)
{
	int height, row, col, num;
	uint32_t *dp, *rp, *hp = NULL, *hrp = NULL, tmp, lmask, rmask;

	row = ri->ri_crow;
	col = ri->ri_ccol * ri->ri_font->fontwidth << PIXEL_SHIFT;
	height = ri->ri_font->fontheight;
	num = ri->ri_font->fontwidth << PIXEL_SHIFT;
	rp = (uint32_t *)(ri->ri_bits + row * ri->ri_yscale + ((col >> 3) & ~3));
	if (ri->ri_hwbits)
		hrp = (uint32_t *)(ri->ri_hwbits + row * ri->ri_yscale +
		    ((col >> 3) & ~3));

	if ((col & 31) + num <= 32) {
		lmask = rasops_pmask[col & 31][num];

		while (height--) {
			dp = rp;
			DELTA(rp, ri->ri_stride, uint32_t *);
			*dp ^= lmask;
		}
		if (ri->ri_hwbits) {
			height = ri->ri_font->fontheight;
			while (height--) {
				hp = hrp;
				DELTA(hrp, ri->ri_stride, uint32_t *);
				*hp ^= lmask;
			}
		}
	} else {
		lmask = ~rasops_rmask[col & 31];
		rmask = ~rasops_lmask[(col + num) & 31];

		while (height--) {
			dp = rp;
			DELTA(rp, ri->ri_stride, uint32_t *);
			if (ri->ri_hwbits) {
				hp = hrp;
				DELTA(hrp, ri->ri_stride, uint32_t *);
			}
			if (lmask != -1) {
				tmp = *dp ^ lmask;
				*dp = tmp;
				dp++;
				if (ri->ri_hwbits) {
					*hp = tmp;
					hp++;
				}
			}

			if (rmask != -1) {
				tmp = *dp ^ rmask;
				*dp = tmp;
				if (ri->ri_hwbits)
					*hp = tmp;
			}
		}
	}
}

/*
 * Copy columns. Ick!
 */
static void
NAME(copycols)(void *cookie, int row, int src, int dst, int num)
{
	int height, lnum, rnum, sb, db, cnt, full;
	uint32_t tmp, lmask, rmask;
	uint32_t *sp, *dp, *srp, *drp, *dhp = NULL, *hp = NULL;
	struct rasops_info *ri;

	ri = (struct rasops_info *)cookie;

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

	cnt = ri->ri_font->fontwidth << PIXEL_SHIFT;
	src *= cnt;
	dst *= cnt;
	num *= cnt;
	row *= ri->ri_yscale;
	height = ri->ri_font->fontheight;
	db = dst & 31;

	if (db + num <= 32) {
		/* Destination is contained within a single word */
		srp = (uint32_t *)(ri->ri_bits + row + ((src >> 3) & ~3));
		drp = (uint32_t *)(ri->ri_bits + row + ((dst >> 3) & ~3));
		if (ri->ri_hwbits)
			dhp = (uint32_t *)(ri->ri_hwbits + row +
			    ((dst >> 3) & ~3));
		sb = src & 31;

		while (height--) {
			GETBITS(srp, sb, num, tmp);
			PUTBITS(tmp, db, num, drp);
			if (ri->ri_hwbits) {
				PUTBITS(tmp, db, num, dhp);
				DELTA(dhp, ri->ri_stride, uint32_t *);
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

	if (lmask)
		full = (num - (32 - (dst & 31))) >> 5;
	else
		full = num >> 5;

	if (src < dst && src + num > dst) {
		/* Copy right-to-left */
		bool sbover;
		int sboff;

		srp = (uint32_t *)(ri->ri_bits + row +
		    (((src + num) >> 3) & ~3));
		drp = (uint32_t *)(ri->ri_bits + row +
		    (((dst + num) >> 3) & ~3));
		if (ri->ri_hwbits)
			dhp = (uint32_t *)(ri->ri_hwbits + row +
			    (((dst + num) >> 3) & ~3));

		sb = src & 31;
		sbover = (sb + lnum) >= 32;
		sboff = (src + num) & 31;
		if ((sboff -= rnum) < 0) {
			srp--;
			sboff += 32;
		}

		while (height--) {
			sp = srp;
			dp = drp;
			if (ri->ri_hwbits) {
				hp = dhp;
				DELTA(dhp, ri->ri_stride, uint32_t *);
			}
			DELTA(srp, ri->ri_stride, uint32_t *);
			DELTA(drp, ri->ri_stride, uint32_t *);

			if (rnum) {
				GETBITS(sp, sboff, rnum, tmp);
				PUTBITS(tmp, 0, rnum, dp);
				if (ri->ri_hwbits) {
					PUTBITS(tmp, 0, rnum, hp);
				}
			}

			/* Now aligned to 32-bits wrt dp */
			for (cnt = full; cnt; cnt--) {
				--dp;
				--sp;
				GETBITS(sp, sboff, 32, tmp);
				*dp = tmp;
				if (ri->ri_hwbits) {
					--hp;
					*hp = tmp;
				}
			}

			if (lmask) {
				if (sbover)
					--sp;
				--dp;
				GETBITS(sp, sb, lnum, tmp);
				PUTBITS(tmp, db, lnum, dp);
				if (ri->ri_hwbits)
					PUTBITS(tmp, db, lnum, hp);
 			}
 		}
	} else {
		/* Copy left-to-right */
		srp = (uint32_t *)(ri->ri_bits + row + ((src >> 3) & ~3));
		drp = (uint32_t *)(ri->ri_bits + row + ((dst >> 3) & ~3));
		if (ri->ri_hwbits)
			dhp = (uint32_t *)(ri->ri_hwbits + row +
			    ((dst >> 3) & ~3));
		db = dst & 31;

		while (height--) {
			sb = src & 31;
			sp = srp;
			dp = drp;
			if (ri->ri_hwbits) {
				hp = dhp;
				DELTA(dhp, ri->ri_stride, uint32_t *);
			}
			DELTA(srp, ri->ri_stride, uint32_t *);
			DELTA(drp, ri->ri_stride, uint32_t *);

			if (lmask) {
				GETBITS(sp, sb, lnum, tmp);
				PUTBITS(tmp, db, lnum, dp);
				dp++;
				if (ri->ri_hwbits) {
					PUTBITS(tmp, db, lnum, hp);
					hp++;
				}	

				if ((sb += lnum) > 31) {
					sp++;
					sb -= 32;
				}
			}

			/* Now aligned to 32-bits wrt dp */
			for (cnt = full; cnt; cnt--, sp++) {
				GETBITS(sp, sb, 32, tmp);
				*dp++ = tmp;
				if (ri->ri_hwbits)
					*hp++ = tmp;
			}

			if (rmask) {
				GETBITS(sp, sb, rnum, tmp);
				PUTBITS(tmp, 0, rnum, dp);
				if (ri->ri_hwbits)
					PUTBITS(tmp, 0, rnum, hp);
 			}
 		}
 	}
}

#endif /* _RASOPS_BITOPS_H_ */
