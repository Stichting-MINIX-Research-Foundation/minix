/*	 $NetBSD: rasops32.c,v 1.29 2013/09/15 09:39:47 martin Exp $	*/

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
__KERNEL_RCSID(0, "$NetBSD: rasops32.c,v 1.29 2013/09/15 09:39:47 martin Exp $");

#include "opt_rasops.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>

#include <dev/wscons/wsdisplayvar.h>
#include <dev/wscons/wsconsio.h>
#include <dev/rasops/rasops.h>

static void 	rasops32_putchar(void *, int, int, u_int, long attr);
static void 	rasops32_putchar_aa(void *, int, int, u_int, long attr);

/*
 * Initialize a 'rasops_info' descriptor for this depth.
 */
void
rasops32_init(struct rasops_info *ri)
{

	if (ri->ri_rnum == 0) {
		ri->ri_rnum = 8;
		ri->ri_rpos = 0;
		ri->ri_gnum = 8;
		ri->ri_gpos = 8;
		ri->ri_bnum = 8;
		ri->ri_bpos = 16;
	}

	if (FONT_IS_ALPHA(ri->ri_font)) {
		ri->ri_ops.putchar = rasops32_putchar_aa;
	} else
		ri->ri_ops.putchar = rasops32_putchar;
}

/*
 * Paint a single character.
 */

static void
rasops32_putchar(void *cookie, int row, int col, u_int uc, long attr)
{
	int width, height, cnt, fs, fb, clr[2];
	struct rasops_info *ri = (struct rasops_info *)cookie;
	struct wsdisplay_font *font = PICK_FONT(ri, uc);
	int32_t *dp, *rp, *hp, *hrp;
	u_char *fr;

	hp = hrp = NULL;

#ifdef RASOPS_CLIPPING
	/* Catches 'row < 0' case too */
	if ((unsigned)row >= (unsigned)ri->ri_rows)
		return;

	if ((unsigned)col >= (unsigned)ri->ri_cols)
		return;
#endif

	/* check if character fits into font limits */
	if (!CHAR_IN_FONT(uc, font))
		return;

	rp = (int32_t *)(ri->ri_bits + row*ri->ri_yscale + col*ri->ri_xscale);
	if (ri->ri_hwbits)
		hrp = (int32_t *)(ri->ri_hwbits + row*ri->ri_yscale +
		    col*ri->ri_xscale);

	height = font->fontheight;
	width = font->fontwidth;

	clr[0] = ri->ri_devcmap[(attr >> 16) & 0xf];
	clr[1] = ri->ri_devcmap[(attr >> 24) & 0xf];

	if (uc == ' ') {
		while (height--) {
			dp = rp;
			DELTA(rp, ri->ri_stride, int32_t *);
			if (ri->ri_hwbits) {
				hp = hrp;
				DELTA(hrp, ri->ri_stride, int32_t *);
			}

			for (cnt = width; cnt; cnt--) {
				*dp++ = clr[0];
				if (ri->ri_hwbits)
					*hp++ = clr[0];
			}
		}
	} else {
		fr = WSFONT_GLYPH(uc, font);
		fs = font->stride;

		while (height--) {
			dp = rp;
			fb = fr[3] | (fr[2] << 8) | (fr[1] << 16) |
			    (fr[0] << 24);
			fr += fs;
			DELTA(rp, ri->ri_stride, int32_t *);
			if (ri->ri_hwbits) {
				hp = hrp;
				DELTA(hrp, ri->ri_stride, int32_t *);
			}

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
		DELTA(rp, -(ri->ri_stride << 1), int32_t *);
		if (ri->ri_hwbits)
			DELTA(hrp, -(ri->ri_stride << 1), int32_t *);

		while (width--) {
			*rp++ = clr[1];
			if (ri->ri_hwbits)
				*hrp++ = clr[1];
		}
	}
}

static void
rasops32_putchar_aa(void *cookie, int row, int col, u_int uc, long attr)
{
	int width, height, cnt, clr[2];
	struct rasops_info *ri = (struct rasops_info *)cookie;
	struct wsdisplay_font *font = PICK_FONT(ri, uc);
	int32_t *dp, *rp;
	uint8_t *rrp;
	u_char *fr;
	uint32_t buffer[64]; /* XXX */
	int x, y, r, g, b, aval;
	int r1, g1, b1, r0, g0, b0;

#ifdef RASOPS_CLIPPING
	/* Catches 'row < 0' case too */
	if ((unsigned)row >= (unsigned)ri->ri_rows)
		return;

	if ((unsigned)col >= (unsigned)ri->ri_cols)
		return;
#endif

	/* check if character fits into font limits */
	if (!CHAR_IN_FONT(uc, font))
		return;

	rrp = (ri->ri_bits + row*ri->ri_yscale + col*ri->ri_xscale);
	rp = (int32_t *)rrp;

	height = font->fontheight;
	width = font->fontwidth;

	clr[0] = ri->ri_devcmap[(attr >> 16) & 0xf];
	clr[1] = ri->ri_devcmap[(attr >> 24) & 0xf];

	if (uc == ' ') {
	        for (cnt = 0; cnt < width; cnt++)
	                buffer[cnt] = clr[0];
		while (height--) {
			dp = rp;
			DELTA(rp, ri->ri_stride, int32_t *);
			memcpy(dp, buffer, width << 2);
		}
	} else {
		fr = WSFONT_GLYPH(uc, font);

		r0 = (clr[0] >> 16) & 0xff;
		r1 = (clr[1] >> 16) & 0xff;
		g0 = (clr[0] >> 8) & 0xff;
		g1 = (clr[1] >> 8) & 0xff;
		b0 =  clr[0] & 0xff;
		b1 =  clr[1] & 0xff;

		for (y = 0; y < height; y++) {
			dp = (uint32_t *)(rrp + ri->ri_stride * y);
			for (x = 0; x < width; x++) {
				aval = *fr;
				if (aval == 0) {
					buffer[x] = clr[0];
				} else if (aval == 255) {
					buffer[x] = clr[1];
				} else {
					r = aval * r1 + (255 - aval) * r0;
					g = aval * g1 + (255 - aval) * g0;
					b = aval * b1 + (255 - aval) * b0;
					buffer[x] = (r & 0xff00) << 8 | 
					      (g & 0xff00) | 
					      (b & 0xff00) >> 8;
				}
				fr++;
			}
			memcpy(dp, buffer, width << 2);
		}
	}

	/* Do underline */
	if ((attr & 1) != 0) {
	        rp = (uint32_t *)rrp;                         
		DELTA(rp, (ri->ri_stride * (height - 2)), int32_t *);
		while (width--)
			*rp++ = clr[1];
	}
}
