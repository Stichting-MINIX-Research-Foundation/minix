/* $NetBSD: rasops_putchar.h,v 1.5.2.1 2019/08/15 12:21:27 martin Exp $ */

/* NetBSD: rasops8.c,v 1.41 2019/07/25 03:02:44 rin Exp  */
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

#if RASOPS_DEPTH != 8 && RASOPS_DEPTH != 15 && RASOPS_DEPTH != 24 && \
    RASOPS_DEPTH != 32
#error "Depth not supported"
#endif

#if   RASOPS_DEPTH == 8
#define	COLOR_TYPE		uint8_t
#elif RASOPS_DEPTH ==  15
#define	COLOR_TYPE		uint16_t
#else
#define	COLOR_TYPE		uint32_t
#endif

#if RASOPS_DEPTH != 24
#define	PIXEL_TYPE		COLOR_TYPE
#define	PIXEL_BYTES		sizeof(PIXEL_TYPE)
#define	SET_COLOR(p, index)	*(p)++ = clr[index]
#define	SET_RGB(p, r, g, b)						\
	*(p)++ = (((r) >> (8 - ri->ri_rnum)) << ri->ri_rpos) |		\
		 (((g) >> (8 - ri->ri_gnum)) << ri->ri_gpos) |		\
		 (((b) >> (8 - ri->ri_bnum)) << ri->ri_bpos)
#endif

#if RASOPS_DEPTH == 24
#define	PIXEL_TYPE		uint8_t
#define	PIXEL_BYTES		3
#define	SET_COLOR(p, index)						\
	do {								\
		COLOR_TYPE c = clr[index];				\
		*(p)++ = c >> 16;					\
		*(p)++ = c >> 8;					\
		*(p)++ = c;						\
	} while (0 /* CONSTCOND */)
#define	SET_RGB(p, r, g, b)						\
	do {								\
		(p)[R_OFF] = (r);					\
		(p)[G_OFF] = (g);					\
		(p)[B_OFF] = (b);					\
		(p) += 3;						\
	} while (0 /* CONSTCOND */)
#endif

#define	AV(p, w)	(((w) * (p)[1] + (0xff - (w)) * (p)[0]) >> 8)

#if BYTE_ORDER == LITTLE_ENDIAN
#define	R_OFF	(ri->ri_rpos / 8)
#define	G_OFF	(ri->ri_gpos / 8)
#define	B_OFF	(ri->ri_bpos / 8)
#else /* BIG_ENDIAN XXX not tested */
#define	R_OFF	(2 - ri->ri_rpos / 8)
#define	G_OFF	(2 - ri->ri_gpos / 8)
#define	B_OFF	(2 - ri->ri_bpos / 8)
#endif

#define NAME(depth)	NAME1(depth)
#ifndef RASOPS_AA
#define NAME1(depth)	rasops ## depth ## _putchar
#else
#define NAME1(depth)	rasops ## depth ## _putchar_aa
#endif

/*
 * Put a single character.
 */
static void
NAME(RASOPS_DEPTH)(void *cookie, int row, int col, u_int uc, long attr)
{
	struct rasops_info *ri = (struct rasops_info *)cookie;
	struct wsdisplay_font *font = PICK_FONT(ri, uc);
	int height, width, cnt;
	uint8_t *fr;
	COLOR_TYPE clr[2];
	PIXEL_TYPE *dp, *rp, *hp;

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

	rp = (PIXEL_TYPE *)(ri->ri_bits + FBOFFSET(ri, row, col));
	if (ri->ri_hwbits)
		hp = (PIXEL_TYPE *)(ri->ri_hwbits + FBOFFSET(ri, row, col));

	clr[0] = (COLOR_TYPE)ATTR_BG(ri, attr);
	clr[1] = (COLOR_TYPE)ATTR_FG(ri, attr);

	if (uc == ' ') {
		while (height--) {
			dp = rp;
			for (cnt = width; cnt; cnt--)
				SET_COLOR(dp, 0);
			if (ri->ri_hwbits) {
				uint16_t bytes = width * PIXEL_BYTES;
					/* XXX GCC */
				memcpy(hp, rp, bytes);
				DELTA(hp, ri->ri_stride, PIXEL_TYPE *);
			}
			DELTA(rp, ri->ri_stride, PIXEL_TYPE *);
		}
	} else {
		fr = FONT_GLYPH(uc, font, ri);

#ifdef RASOPS_AA
		int off[2];
		uint8_t r[2], g[2], b[2];

		/*
	 	 * This is independent to positions/lengths of RGB in pixel.
	 	 */
		off[0] = (((uint32_t)attr >> 16) & 0xf) * 3;
		off[1] = (((uint32_t)attr >> 24) & 0xf) * 3;

		r[0] = rasops_cmap[off[0]];
		r[1] = rasops_cmap[off[1]];
		g[0] = rasops_cmap[off[0] + 1];
		g[1] = rasops_cmap[off[1] + 1];
		b[0] = rasops_cmap[off[0] + 2];
		b[1] = rasops_cmap[off[1] + 2];
#endif

		while (height--) {
			dp = rp;

#ifndef RASOPS_AA
			uint32_t fb = rasops_be32uatoh(fr);
			fr += ri->ri_font->stride;

			for (cnt = width; cnt; cnt--) {
				SET_COLOR(dp, (fb >> 31) & 1);
				fb <<= 1;
			}
#else /* RASOPS_AA */
			for (cnt = width; cnt; cnt--) {
				int w = *fr++;
				if (w == 0xff)
					SET_COLOR(dp, 1);
				else if (w == 0)
					SET_COLOR(dp, 0);
				else
					SET_RGB(dp,
					    AV(r, w), AV(g, w), AV(b, w));
			}
#endif
			if (ri->ri_hwbits) {
				uint16_t bytes = width * PIXEL_BYTES;
					/* XXX GCC */
				memcpy(hp, rp, bytes);
				DELTA(hp, ri->ri_stride, PIXEL_TYPE *);
			}
			DELTA(rp, ri->ri_stride, PIXEL_TYPE *);
		}
	}

	/* Do underline */
	if ((attr & WSATTR_UNDERLINE) != 0) {
		DELTA(rp, - ri->ri_stride * ri->ri_ul.off, PIXEL_TYPE *);
		if (ri->ri_hwbits)
			DELTA(hp, - ri->ri_stride * ri->ri_ul.off,
			    PIXEL_TYPE *);

		for (height = ri->ri_ul.height; height; height--) {
			DELTA(rp, - ri->ri_stride, PIXEL_TYPE *);
			dp = rp;
			for (cnt = width; cnt; cnt--)
				SET_COLOR(dp, 1);
			if (ri->ri_hwbits) {
				DELTA(hp, - ri->ri_stride, PIXEL_TYPE *);
				uint16_t bytes = width * PIXEL_BYTES;
					/* XXX GCC */
				memcpy(hp, rp, bytes);
			}
		}
	}
}

#undef	COLOR_TYPE

#undef	PIXEL_TYPE
#undef	PIXEL_BYTES
#undef	SET_COLOR
#undef	SET_RGB

#undef	AV

#undef	R_OFF
#undef	G_OFF
#undef	B_OFF

#undef	NAME
#undef	NAME1
