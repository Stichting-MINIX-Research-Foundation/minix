/* 	$NetBSD: rasops2.c,v 1.18 2013/04/21 04:28:05 kiyohara Exp $	*/

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
__KERNEL_RCSID(0, "$NetBSD: rasops2.c,v 1.18 2013/04/21 04:28:05 kiyohara Exp $");

#include "opt_rasops.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <machine/endian.h>

#include <dev/wscons/wsdisplayvar.h>
#include <dev/wscons/wsconsio.h>
#include <dev/rasops/rasops.h>
#include <dev/rasops/rasops_masks.h>

static void	rasops2_copycols(void *, int, int, int, int);
static void	rasops2_erasecols(void *, int, int, int, long);
static void	rasops2_do_cursor(struct rasops_info *);
static void	rasops2_putchar(void *, int, int col, u_int, long);
#ifndef RASOPS_SMALL
static void	rasops2_putchar8(void *, int, int col, u_int, long);
static void	rasops2_putchar12(void *, int, int col, u_int, long);
static void	rasops2_putchar16(void *, int, int col, u_int, long);
static void	rasops2_makestamp(struct rasops_info *, long);

/*
 * 4x1 stamp for optimized character blitting
 */
static int8_t	stamp[16];
static long	stamp_attr;
static int	stamp_mutex;	/* XXX see note in README */
#endif

/*
 * Initialize rasops_info struct for this colordepth.
 */
void
rasops2_init(struct rasops_info *ri)
{

	switch (ri->ri_font->fontwidth) {
#ifndef RASOPS_SMALL
	case 8:
		ri->ri_ops.putchar = rasops2_putchar8;
		break;
	case 12:
		ri->ri_ops.putchar = rasops2_putchar12;
		break;
	case 16:
		ri->ri_ops.putchar = rasops2_putchar16;
		break;
#endif	/* !RASOPS_SMALL */
	default:
		panic("fontwidth not 8/12/16 or RASOPS_SMALL - fixme!");
		ri->ri_ops.putchar = rasops2_putchar;
		break;
	}

	if ((ri->ri_font->fontwidth & 3) != 0) {
		ri->ri_ops.erasecols = rasops2_erasecols;
		ri->ri_ops.copycols = rasops2_copycols;
		ri->ri_do_cursor = rasops2_do_cursor;
	}
}

#ifdef notyet
/*
 * Paint a single character. This is the generic version, this is ugly.
 */
static void
rasops2_putchar(void *cookie, int row, int col, u_int uc, long attr)
{
	int height, width, fs, rs, fb, bg, fg, lmask, rmask;
	struct rasops_info *ri = (struct rasops_info *)cookie;
	struct wsdisplay_font *font = PICK_FONT(ri, uc);
	int32_t *rp;
	u_char *fr;

#ifdef RASOPS_CLIPPING
	/* Catches 'row < 0' case too */
	if ((unsigned)row >= (unsigned)ri->ri_rows)
		return;

	if ((unsigned)col >= (unsigned)ri->ri_cols)
		return;
#endif

	width = font->fontwidth << 1;
	height = font->fontheight;
	col *= width;
	rp = (int32_t *)(ri->ri_bits + row * ri->ri_yscale + ((col >> 3) & ~3));
	col = col & 31;
	rs = ri->ri_stride;

	bg = ri->ri_devcmap[(attr >> 16) & 0xf];
	fg = ri->ri_devcmap[(attr >> 24) & 0xf];

	/* If fg and bg match this becomes a space character */
	if (fg == bg || uc == ' ') {
		uc = (u_int)-1;
		fr = 0;		/* shutup gcc */
		fs = 0;		/* shutup gcc */
	} else {
		uc -= font->firstchar;
		fr = (u_char *)font->data + uc * ri->ri_fontscale;
		fs = font->stride;
	}

	/* Single word, one mask */
	if ((col + width) <= 32) {
		rmask = rasops_pmask[col][width];
		lmask = ~rmask;

		if (uc == (u_int)-1) {
			bg &= rmask;

			while (height--) {
				*rp = (*rp & lmask) | bg;
				DELTA(rp, rs, int32_t *);
			}
		} else {
			while (height--) {
				/* get bits, mask */
				/* compose sl */
				/* mask sl */
				/* put word */
			}
		}

		/* Do underline */
		if (attr & 1) {
			DELTA(rp, -(ri->ri_stride << 1), int32_t *);
			*rp = (*rp & lmask) | (fg & rmask);
		}
	} else {
		lmask = ~rasops_lmask[col];
		rmask = ~rasops_rmask[(col + width) & 31];

		if (uc == (u_int)-1) {
			bg = bg & ~lmask;
			width = bg & ~rmask;

			while (height--) {
				rp[0] = (rp[0] & lmask) | bg;
				rp[1] = (rp[1] & rmask) | width;
				DELTA(rp, rs, int32_t *);
			}
		} else {
			width = 32 - col;

			/* NOT fontbits if bg is white */
			while (height--) {
				fb = ~(fr[3] | (fr[2] << 8) |
				    (fr[1] << 16) | (fr[0] << 24));

				rp[0] = (rp[0] & lmask)
				    | MBE((u_int)fb >> col);

				rp[1] = (rp[1] & rmask)
				   | (MBE((u_int)fb << width) & ~rmask);

				fr += fs;
				DELTA(rp, rs, int32_t *);
			}
		}

		/* Do underline */
		if (attr & 1) {
			DELTA(rp, -(ri->ri_stride << 1), int32_t *);
			rp[0] = (rp[0] & lmask) | (fg & ~lmask);
			rp[1] = (rp[1] & rmask) | (fg & ~rmask);
		}
	}
}
#endif

/*
 * Put a single character. This is the generic version.
 */
static void
rasops2_putchar(void *cookie, int row, int col, u_int uc, long attr)
{

	/* XXX punt */
}

#ifndef RASOPS_SMALL
/*
 * Recompute the blitting stamp.
 */
static void
rasops2_makestamp(struct rasops_info *ri, long attr)
{
	int i, fg, bg;

	fg = ri->ri_devcmap[(attr >> 24) & 0xf] & 3;
	bg = ri->ri_devcmap[(attr >> 16) & 0xf] & 3;
	stamp_attr = attr;

	for (i = 0; i < 16; i++) {
#if BYTE_ORDER == BIG_ENDIAN
#define NEED_LITTLE_ENDIAN_STAMP RI_BSWAP
#else
#define NEED_LITTLE_ENDIAN_STAMP 0
#endif
		if ((ri->ri_flg & RI_BSWAP) == NEED_LITTLE_ENDIAN_STAMP) {
			/* littel endian */
			stamp[i] = (i & 8 ? fg : bg);
			stamp[i] |= (i & 4 ? fg : bg) << 2;
			stamp[i] |= (i & 2 ? fg : bg) << 4;
			stamp[i] |= (i & 1 ? fg : bg) << 6;
		} else {
			/* big endian */
			stamp[i] = (i & 1 ? fg : bg);
			stamp[i] |= (i & 2 ? fg : bg) << 2;
			stamp[i] |= (i & 4 ? fg : bg) << 4;
			stamp[i] |= (i & 8 ? fg : bg) << 6;
		}
	}
}

/*
 * Put a single character. This is for 8-pixel wide fonts.
 */
static void
rasops2_putchar8(void *cookie, int row, int col, u_int uc, long attr)
{
	struct rasops_info *ri = (struct rasops_info *)cookie;
	struct wsdisplay_font *font = PICK_FONT(ri, uc);
	int height, fs, rs;
	u_char *fr, *rp;

	/* Can't risk remaking the stamp if it's already in use */
	if (stamp_mutex++) {
		stamp_mutex--;
		rasops2_putchar(cookie, row, col, uc, attr);
		return;
	}

#ifdef RASOPS_CLIPPING
	/* Catches 'row < 0' case too */
	if ((unsigned)row >= (unsigned)ri->ri_rows) {
		stamp_mutex--;
		return;
	}

	if ((unsigned)col >= (unsigned)ri->ri_cols) {
		stamp_mutex--;
		return;
	}
#endif

	rp = ri->ri_bits + row * ri->ri_yscale + col * ri->ri_xscale;
	height = font->fontheight;
	rs = ri->ri_stride;

	/* Recompute stamp? */
	if (attr != stamp_attr)
		rasops2_makestamp(ri, attr);

	if (uc == ' ') {
		int8_t c = stamp[0];
		while (height--) {
			*(int16_t *)rp = c;
			rp += rs;
		}
	} else {
		uc -= font->firstchar;
		fr = (u_char *)font->data + uc * ri->ri_fontscale;
		fs = font->stride;

		while (height--) {
			rp[0] = stamp[(*fr >> 4) & 0xf];
			rp[1] = stamp[*fr & 0xf];
			fr += fs;
			rp += rs;
		}
	}

	/* Do underline */
	if ((attr & 1) != 0)
		*(int16_t *)(rp - (ri->ri_stride << 1)) = stamp[15];

	stamp_mutex--;
}

/*
 * Put a single character. This is for 12-pixel wide fonts.
 */
static void
rasops2_putchar12(void *cookie, int row, int col, u_int uc, long attr)
{
	struct rasops_info *ri = (struct rasops_info *)cookie;
	struct wsdisplay_font *font = PICK_FONT(ri, uc);
	int height, fs, rs;
	u_char *fr, *rp;

	/* Can't risk remaking the stamp if it's already in use */
	if (stamp_mutex++) {
		stamp_mutex--;
		rasops2_putchar(cookie, row, col, uc, attr);
		return;
	}

#ifdef RASOPS_CLIPPING
	/* Catches 'row < 0' case too */
	if ((unsigned)row >= (unsigned)ri->ri_rows) {
		stamp_mutex--;
		return;
	}

	if ((unsigned)col >= (unsigned)ri->ri_cols) {
		stamp_mutex--;
		return;
	}
#endif

	rp = ri->ri_bits + row * ri->ri_yscale + col * ri->ri_xscale;
	height = font->fontheight;
	rs = ri->ri_stride;

	/* Recompute stamp? */
	if (attr != stamp_attr)
		rasops2_makestamp(ri, attr);

	if (uc == ' ') {
		int8_t c = stamp[0];
		while (height--) {
			rp[0] = rp[1] = rp[2] = c;
			rp += rs;
		}
	} else {
		uc -= font->firstchar;
		fr = (u_char *)font->data + uc * ri->ri_fontscale;
		fs = font->stride;

		while (height--) {
			rp[0] = stamp[(fr[0] >> 4) & 0xf];
			rp[1] = stamp[fr[0] & 0xf];
			rp[2] = stamp[(fr[1] >> 4) & 0xf];
			fr += fs;
			rp += rs;
		}
	}

	/* Do underline */
	if ((attr & 1) != 0) {
		rp -= ri->ri_stride << 1;
		rp[0] = rp[1] = rp[2] = stamp[15];
	}

	stamp_mutex--;
}

/*
 * Put a single character. This is for 16-pixel wide fonts.
 */
static void
rasops2_putchar16(void *cookie, int row, int col, u_int uc, long attr)
{
	struct rasops_info *ri = (struct rasops_info *)cookie;
	struct wsdisplay_font *font = PICK_FONT(ri, uc);
	int height, fs, rs;
	u_char *fr, *rp;

	/* Can't risk remaking the stamp if it's already in use */
	if (stamp_mutex++) {
		stamp_mutex--;
		rasops2_putchar(cookie, row, col, uc, attr);
		return;
	}

#ifdef RASOPS_CLIPPING
	/* Catches 'row < 0' case too */
	if ((unsigned)row >= (unsigned)ri->ri_rows) {
		stamp_mutex--;
		return;
	}

	if ((unsigned)col >= (unsigned)ri->ri_cols) {
		stamp_mutex--;
		return;
	}
#endif

	rp = ri->ri_bits + row * ri->ri_yscale + col * ri->ri_xscale;
	height = font->fontheight;
	rs = ri->ri_stride;

	/* Recompute stamp? */
	if (attr != stamp_attr)
		rasops2_makestamp(ri, attr);

	if (uc == ' ') {
		int8_t c = stamp[0];
		while (height--) {
			*(int32_t *)rp = c;
			rp += rs;
		}
	} else {
		uc -= font->firstchar;
		fr = (u_char *)font->data + uc * ri->ri_fontscale;
		fs = font->stride;

		while (height--) {
			rp[0] = stamp[(fr[0] >> 4) & 0xf];
			rp[1] = stamp[fr[0] & 0xf];
			rp[2] = stamp[(fr[1] >> 4) & 0xf];
			rp[3] = stamp[fr[1] & 0xf];
			fr += fs;
			rp += rs;
		}
	}

	/* Do underline */
	if ((attr & 1) != 0)
		*(int32_t *)(rp - (ri->ri_stride << 1)) = stamp[15];

	stamp_mutex--;
}
#endif	/* !RASOPS_SMALL */

/*
 * Grab routines common to depths where (bpp < 8)
 */
#define NAME(ident)	rasops2_##ident
#define PIXEL_SHIFT	1

#include <dev/rasops/rasops_bitops.h>
