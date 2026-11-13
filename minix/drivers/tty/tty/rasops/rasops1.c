/* 	$NetBSD: rasops1.c,v 1.31.2.1 2019/08/15 12:21:27 martin Exp $	*/

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
__KERNEL_RCSID(0, "$NetBSD: rasops1.c,v 1.31.2.1 2019/08/15 12:21:27 martin Exp $");

#ifdef _KERNEL_OPT
#include "opt_rasops.h"
#endif

#include <sys/param.h>
#include <sys/proc.h>

#include <machine/endian.h>

#include "../wscons/wsconsio.h"
#include "../wscons/wsdisplayvar.h"

#define	_RASOPS_PRIVATE
#define	RASOPS_DEPTH	1
#include "rasops.h"
#include "rasops_masks.h"

static void	rasops1_copycols(void *, int, int, int, int);
static void	rasops1_erasecols(void *, int, int, int, long);
static void	rasops1_do_cursor(struct rasops_info *);
static void	rasops1_putchar(void *, int, int col, u_int, long);
#ifndef RASOPS_SMALL
static void	rasops1_putchar8(void *, int, int col, u_int, long);
static void	rasops1_putchar16(void *, int, int col, u_int, long);
#endif

/*
 * Initialize rasops_info struct for this colordepth.
 */
void rasops1_init(struct rasops_info *ri)
{

	if ((ri->ri_font->fontwidth & 7) != 0) {
		ri->ri_ops.erasecols = rasops1_erasecols;
		ri->ri_ops.copycols = rasops1_copycols;
		ri->ri_do_cursor = rasops1_do_cursor;
	}

	switch (ri->ri_font->fontwidth) {
#ifndef RASOPS_SMALL
	case 8:
		ri->ri_ops.putchar = rasops1_putchar8;
		break;
	case 16:
		ri->ri_ops.putchar = rasops1_putchar16;
		break;
#endif
	default:
		ri->ri_ops.putchar = rasops1_putchar;
		break;
	}
}

/*
 * Paint a single character. This is the generic version, this is ugly.
 */
static void rasops1_putchar(void *cookie, int row, int col, u_int uc, long attr)
{
	struct rasops_info *ri = (struct rasops_info *)cookie;
	struct wsdisplay_font *font = PICK_FONT(ri, uc);
	int height, width;
	uint32_t bg, fg, lbg, rbg, fb, lmask, rmask, tmp, tmp0, tmp1;
	uint32_t *rp, *hp;
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
	width = font->fontwidth;
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
			while (height--) {
				fb = rasops_be32uatoh(fr);
				fr += font->stride;
				if (bg)
					fb = ~fb;

				tmp = *rp & lmask;
				tmp |= (MBE(fb >> col) & rmask);
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
	} else {
		/* Word boundary, two masks needed */
		lmask = ~rasops_lmask[col];
		rmask = ~rasops_rmask[(col + width) & 31];

		if (space) {
			lbg = bg & ~lmask;
			rbg = bg & ~rmask;

			while (height--) {
				tmp0 = (rp[0] & lmask) | lbg;
				tmp1 = (rp[1] & rmask) | rbg;

				rp[0] = tmp0;
				rp[1] = tmp1;

				if (ri->ri_hwbits) {
					hp[0] = tmp0;
					hp[1] = tmp1;
					DELTA(hp, ri->ri_stride, uint32_t *);
				}

				DELTA(rp, ri->ri_stride, uint32_t *);
			}
		} else {
			width = 32 - col;

			while (height--) {
				fb = rasops_be32uatoh(fr);
				fr += font->stride;
				if (bg)
					fb = ~fb;

				tmp0 = rp[0] & lmask;
				tmp0 |= MBE(fb >> col);

				tmp1 = rp[1] & rmask;
				tmp1 |= (MBE(fb << width) & ~rmask);

				rp[0] = tmp0;
				rp[1] = tmp1;

				if (ri->ri_hwbits) {
					hp[0] = tmp0;
					hp[1] = tmp1;
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
				tmp0 = (rp[0] & lmask) | (fg & ~lmask);
				tmp1 = (rp[1] & rmask) | (fg & ~rmask);
				rp[0] = tmp0;
				rp[1] = tmp1;
				if (ri->ri_hwbits) {
					DELTA(hp, - ri->ri_stride, uint32_t *);
					hp[0] = tmp0;
					hp[1] = tmp1;
				}
			}
		}
	}
}

#ifndef RASOPS_SMALL
/*
 * Width-optimized putchar functions
 */
#define	RASOPS_WIDTH	8
#include "rasops1_putchar_width.h"
#undef	RASOPS_WIDTH

#define	RASOPS_WIDTH	16
#include "rasops1_putchar_width.h"
#undef	RASOPS_WIDTH

#endif	/* !RASOPS_SMALL */

/*
 * Grab routines common to depths where (bpp < 8)
 */
#include "rasops_bitops.h"
