/* $NetBSD: rasops1_putchar_width.h,v 1.2.2.1 2019/08/15 12:21:27 martin Exp $ */

/* NetBSD: rasops1.c,v 1.28 2019/07/25 03:02:44 rin Exp */
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

#if RASOPS_WIDTH != 8 && RASOPS_WIDTH != 16
#error "Width not supported"
#endif

#if RASOPS_WIDTH == 8
#define	SUBST_UNIT	uint8_t
#define	GET_GLYPH(fr)	(fr)[0]
#endif

#if RASOPS_WIDTH == 16
/*
 * rp and hp are always half-word aligned, whereas
 * fr may not be aligned in half-word boundary.
 */
#define	SUBST_UNIT	uint16_t
#  if BYTE_ORDER == BIG_ENDIAN
#define	GET_GLYPH(fr)	((fr)[0] << 8) | (fr)[1]
#  else
#define	GET_GLYPH(fr)	(fr)[0] | ((fr)[1] << 8)
#  endif
#endif /* RASOPS_WIDTH == 16 */

#define	NAME(width)	NAME1(width)
#define	NAME1(width)	rasops1_putchar ## width

/*
 * Width-optimized putchar function.
 */
static void
NAME(RASOPS_WIDTH)(void *cookie, int row, int col, u_int uc, long attr)
{
	struct rasops_info *ri = (struct rasops_info *)cookie;
	struct wsdisplay_font *font = PICK_FONT(ri, uc);
	int height;
	uint32_t bg, fg;
	uint8_t *fr;
	SUBST_UNIT tmp, *rp, *hp;

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

	rp = (SUBST_UNIT *)(ri->ri_bits + row * ri->ri_yscale +
	    col * sizeof(SUBST_UNIT));
	if (ri->ri_hwbits)
		hp = (SUBST_UNIT *)(ri->ri_hwbits + row * ri->ri_yscale +
		    col * sizeof(SUBST_UNIT));

	bg = ATTR_BG(ri, attr);
	fg = ATTR_FG(ri, attr);

	/* If fg and bg match this becomes a space character */
	if (uc == ' ' || __predict_false(fg == bg)) {
		while (height--) {
			*rp = bg;
			if (ri->ri_hwbits) {
				*hp = bg;
				DELTA(hp, ri->ri_stride, SUBST_UNIT *);
			}
			DELTA(rp, ri->ri_stride, SUBST_UNIT *);
		}
	} else {
		fr = FONT_GLYPH(uc, font, ri);

		while (height--) {
			tmp = GET_GLYPH(fr);
			fr += font->stride;
			if (bg)
				tmp = ~tmp;

			*rp = tmp;

			if (ri->ri_hwbits) {
				*hp = tmp;
				DELTA(hp, ri->ri_stride, SUBST_UNIT *);
			}

			DELTA(rp, ri->ri_stride, SUBST_UNIT *);
		}
	}

	/* Do underline */
	if ((attr & WSATTR_UNDERLINE) != 0) {
		DELTA(rp, - ri->ri_stride * ri->ri_ul.off, SUBST_UNIT *);
		if (ri->ri_hwbits)
			DELTA(hp, - ri->ri_stride * ri->ri_ul.off,
			    SUBST_UNIT *);

		for (height = ri->ri_ul.height; height; height--) {
			DELTA(rp, - ri->ri_stride, SUBST_UNIT *);
			*rp = fg;
			if (ri->ri_hwbits) {
				DELTA(hp, - ri->ri_stride, SUBST_UNIT *);
				*hp = fg;
			}
		}
	}
}

#undef	SUBST_UNIT
#undef	GET_GLYPH

#undef	NAME
#undef	NAME1
