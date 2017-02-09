/* 	$NetBSD: rasops1.c,v 1.23 2010/05/04 04:57:34 macallan Exp $	*/

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
__KERNEL_RCSID(0, "$NetBSD: rasops1.c,v 1.23 2010/05/04 04:57:34 macallan Exp $");

#include "opt_rasops.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <machine/endian.h>

#include <dev/wscons/wsdisplayvar.h>
#include <dev/wscons/wsconsio.h>
#include <dev/rasops/rasops.h>
#include <dev/rasops/rasops_masks.h>

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
void
rasops1_init(struct rasops_info *ri)
{

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

	if ((ri->ri_font->fontwidth & 7) != 0) {
		ri->ri_ops.erasecols = rasops1_erasecols;
		ri->ri_ops.copycols = rasops1_copycols;
		ri->ri_do_cursor = rasops1_do_cursor;
	}
}

/*
 * Paint a single character. This is the generic version, this is ugly.
 */
static void
rasops1_putchar(void *cookie, int row, int col, u_int uc, long attr)
{
	u_int fs, rs, fb, bg, fg, lmask, rmask;
	u_int32_t height, width;
	struct rasops_info *ri = (struct rasops_info *)cookie;
	struct wsdisplay_font *font = PICK_FONT(ri, uc);
	int32_t *rp, *hrp = NULL, tmp, tmp2;
	u_char *fr;

#ifdef RASOPS_CLIPPING
	/* Catches 'row < 0' case too */
	if ((unsigned)row >= (unsigned)ri->ri_rows)
		return;

	if ((unsigned)col >= (unsigned)ri->ri_cols)
		return;
#endif

	col *= ri->ri_font->fontwidth;
	rp = (int32_t *)(ri->ri_bits + row * ri->ri_yscale + ((col >> 3) & ~3));
	if (ri->ri_hwbits)
		hrp = (int32_t *)(ri->ri_hwbits + row * ri->ri_yscale +
		    ((col >> 3) & ~3));
	height = font->fontheight;
	width = font->fontwidth;
	col = col & 31;
	rs = ri->ri_stride;

	bg = (attr & 0x000f0000) ? ri->ri_devcmap[1] : ri->ri_devcmap[0];
	fg = (attr & 0x0f000000) ? ri->ri_devcmap[1] : ri->ri_devcmap[0];

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
				tmp = (*rp & lmask) | bg;
				*rp = tmp;
				DELTA(rp, rs, int32_t *);
				if (ri->ri_hwbits) {
					*hrp = tmp;
					DELTA(hrp, rs, int32_t *);
				}
			}
		} else {
			/* NOT fontbits if bg is white */
			if (bg) {
				while (height--) {
					fb = ~(fr[3] | (fr[2] << 8) |
					    (fr[1] << 16) | (fr[0] << 24));
					tmp = (*rp & lmask)
					    | (MBE(fb >> col) & rmask);
					*rp = tmp;

					fr += fs;
					DELTA(rp, rs, int32_t *);
					if (ri->ri_hwbits) {
						*hrp = tmp;
						DELTA(hrp, rs, int32_t *);
					}
				}
			} else {
				while (height--) {
					fb = (fr[3] | (fr[2] << 8) |
					    (fr[1] << 16) | (fr[0] << 24));
					tmp = (*rp & lmask)
					    | (MBE(fb >> col) & rmask);
					*rp = tmp;

					fr += fs;
					DELTA(rp, rs, int32_t *);
					if (ri->ri_hwbits) {
						*hrp = tmp;
						DELTA(hrp, rs, int32_t *);
					}
				}
			}
		}

		/* Do underline */
		if ((attr & 1) != 0) {
			DELTA(rp, -(ri->ri_stride << 1), int32_t *);
			tmp = (*rp & lmask) | (fg & rmask);
			*rp = tmp;
			if (ri->ri_hwbits) {
				DELTA(hrp, -(ri->ri_stride << 1), int32_t *);
				*hrp = tmp;
			}
		}
	} else {
		lmask = ~rasops_lmask[col];
		rmask = ~rasops_rmask[(col + width) & 31];

		if (uc == (u_int)-1) {
			width = bg & ~rmask;
			bg = bg & ~lmask;

			while (height--) {
				tmp = (rp[0] & lmask) | bg;
				tmp2 = (rp[1] & rmask) | width;
				rp[0] = tmp;
				rp[1] = tmp2;
				DELTA(rp, rs, int32_t *);
				if (ri->ri_hwbits) {
					hrp[0] = tmp;
					hrp[1] = tmp2;
					DELTA(hrp, rs, int32_t *);
				}
			}
		} else {
			width = 32 - col;

			/* NOT fontbits if bg is white */
			if (bg) {
				while (height--) {
					fb = ~(fr[3] | (fr[2] << 8) |
					    (fr[1] << 16) | (fr[0] << 24));

					tmp = (rp[0] & lmask)
					    | MBE((u_int)fb >> col);

					tmp2 = (rp[1] & rmask)
					    | (MBE((u_int)fb << width) & ~rmask);
					rp[0] = tmp;
					rp[1] = tmp2;
					fr += fs;
					DELTA(rp, rs, int32_t *);
					if (ri->ri_hwbits) {
						hrp[0] = tmp;
						hrp[1] = tmp2;
						DELTA(hrp, rs, int32_t *);
					}
				}
			} else {
				while (height--) {
					fb = (fr[3] | (fr[2] << 8) |
					    (fr[1] << 16) | (fr[0] << 24));

					tmp = (rp[0] & lmask)
					    | MBE(fb >> col);

					tmp2 = (rp[1] & rmask)
					    | (MBE(fb << width) & ~rmask);
					rp[0] = tmp;
					rp[1] = tmp2;
					fr += fs;
					DELTA(rp, rs, int32_t *);
					if (ri->ri_hwbits) {
						hrp[0] = tmp;
						hrp[1] = tmp2;
						DELTA(hrp, rs, int32_t *);
					}
				}
			}
		}

		/* Do underline */
		if ((attr & 1) != 0) {
			DELTA(rp, -(ri->ri_stride << 1), int32_t *);
			tmp = (rp[0] & lmask) | (fg & ~lmask);
			tmp2 = (rp[1] & rmask) | (fg & ~rmask);
			rp[0] = tmp;
			rp[1] = tmp2;
			if (ri->ri_hwbits) {
				DELTA(hrp, -(ri->ri_stride << 1), int32_t *);
				hrp[0] = tmp;
				hrp[1] = tmp2;
			}
		}
	}
}

#ifndef RASOPS_SMALL
/*
 * Paint a single character. This is for 8-pixel wide fonts.
 */
static void
rasops1_putchar8(void *cookie, int row, int col, u_int uc, long attr)
{
	int height, fs, rs, bg, fg;
	struct rasops_info *ri = (struct rasops_info *)cookie;
	struct wsdisplay_font *font = PICK_FONT(ri, uc);
	u_char *fr, *rp, *hrp = NULL;

#ifdef RASOPS_CLIPPING
	/* Catches 'row < 0' case too */
	if ((unsigned)row >= (unsigned)ri->ri_rows)
		return;

	if ((unsigned)col >= (unsigned)ri->ri_cols)
		return;
#endif

	rp = ri->ri_bits + row * ri->ri_yscale + col * ri->ri_xscale;
	if (ri->ri_hwbits)
		hrp = ri->ri_hwbits + row * ri->ri_yscale + col * ri->ri_xscale;
	height = font->fontheight;
	rs = ri->ri_stride;

	bg = (attr & 0x000f0000) ? ri->ri_devcmap[1] : ri->ri_devcmap[0];
	fg = (attr & 0x0f000000) ? ri->ri_devcmap[1] : ri->ri_devcmap[0];

	/* If fg and bg match this becomes a space character */
	if (fg == bg || uc == ' ') {
		while (height--) {
			*rp = bg;
			rp += rs;
			if (ri->ri_hwbits) {
				*hrp = bg;
				hrp += rs;
			}
		}
	} else {
		uc -= font->firstchar;
		fr = (u_char *)font->data + uc * ri->ri_fontscale;
		fs = font->stride;

		/* NOT fontbits if bg is white */
		if (bg) {
			while (height--) {
				*rp = ~*fr;
				rp += rs;
				if (ri->ri_hwbits) {
					*hrp = ~*fr;
					hrp += rs;
				}
				fr += fs;
					
			}
		} else {
			while (height--) {
				*rp = *fr;
				rp += rs;
				if (ri->ri_hwbits) {
					*hrp = *fr;
					hrp += rs;
				}
				fr += fs;
			}
		}

	}

	/* Do underline */
	if ((attr & 1) != 0) {
		rp[-(ri->ri_stride << 1)] = fg;
		if (ri->ri_hwbits) {
			hrp[-(ri->ri_stride << 1)] = fg;
		}
	}
}

/*
 * Paint a single character. This is for 16-pixel wide fonts.
 */
static void
rasops1_putchar16(void *cookie, int row, int col, u_int uc, long attr)
{
	int height, fs, rs, bg, fg;
	struct rasops_info *ri = (struct rasops_info *)cookie;
	struct wsdisplay_font *font = PICK_FONT(ri, uc);
	u_char *fr, *rp, *hrp = NULL;

#ifdef RASOPS_CLIPPING
	/* Catches 'row < 0' case too */
	if ((unsigned)row >= (unsigned)ri->ri_rows)
		return;

	if ((unsigned)col >= (unsigned)ri->ri_cols)
		return;
#endif

	rp = ri->ri_bits + row * ri->ri_yscale + col * ri->ri_xscale;
	if (ri->ri_hwbits)
		hrp = ri->ri_hwbits + row * ri->ri_yscale + col * ri->ri_xscale;
	height = font->fontheight;
	rs = ri->ri_stride;

	bg = (attr & 0x000f0000) ? ri->ri_devcmap[1] : ri->ri_devcmap[0];
	fg = (attr & 0x0f000000) ? ri->ri_devcmap[1] : ri->ri_devcmap[0];

	/* If fg and bg match this becomes a space character */
	if (fg == bg || uc == ' ') {
		while (height--) {
			/* XXX alignment?! */
			*(int16_t *)rp = bg;
			rp += rs;
			if (ri->ri_hwbits) {
				*(int16_t *)hrp = bg;
				hrp += rs;
			}
		}
	} else {
		uc -= font->firstchar;
		fr = (u_char *)font->data + uc * ri->ri_fontscale;
		fs = font->stride;

		/* NOT fontbits if bg is white */
		if (bg) {
			while (height--) {
				rp[0] = ~fr[0];
				rp[1] = ~fr[1];
				rp += rs;
				if (ri->ri_hwbits) {
					hrp[0] = ~fr[0];
					hrp[1] = ~fr[1];
					hrp += rs;
				}
				fr += fs;
			}
		} else {
			while (height--) {
				rp[0] = fr[0];
				rp[1] = fr[1];
				rp += rs;
				if (ri->ri_hwbits) {
					hrp[0] = fr[0];
					hrp[1] = fr[1];
					hrp += rs;
				}
				fr += fs;
			}
		}
	}

	/* Do underline */
	if ((attr & 1) != 0) {
		/* XXX alignment?! */
		*(int16_t *)(rp - (ri->ri_stride << 1)) = fg;
		if (ri->ri_hwbits) {
			*(int16_t *)(hrp - (ri->ri_stride << 1)) = fg;
		}
	}
}
#endif	/* !RASOPS_SMALL */

/*
 * Grab routines common to depths where (bpp < 8)
 */
#define NAME(ident)	rasops1_##ident
#define PIXEL_SHIFT	0

#include <dev/rasops/rasops_bitops.h>
