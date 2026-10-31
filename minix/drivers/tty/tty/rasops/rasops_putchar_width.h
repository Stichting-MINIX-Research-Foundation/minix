/* $NetBSD: rasops_putchar_width.h,v 1.9.2.1 2019/08/15 12:21:27 martin Exp $ */

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

#if RASOPS_DEPTH !=  2 && RASOPS_DEPTH !=  4 && RASOPS_DEPTH !=  8 &&	\
    RASOPS_DEPTH != 15 && RASOPS_DEPTH != 24 && RASOPS_DEPTH != 32
#error "Depth not supported"
#endif

#if RASOPS_WIDTH != 8 && RASOPS_WIDTH != 12 && RASOPS_WIDTH != 16
#error "Width not supported"
#endif

#if   RASOPS_DEPTH == 2
#define	STAMP_TYPE	uint8_t
#elif RASOPS_DEPTH == 4
#define	STAMP_TYPE	uint16_t
#else
#define	STAMP_TYPE	uint32_t
#endif

#if   RASOPS_DEPTH <= 8
#define	SUBST_UNIT	1
#elif RASOPS_DEPTH == 15
#define	SUBST_UNIT	2
#elif RASOPS_DEPTH == 24
#define	SUBST_UNIT	3
#elif RASOPS_DEPTH == 32
#define	SUBST_UNIT	4
#endif

#define	SUBST_BYTES	(SUBST_UNIT * (RASOPS_WIDTH / 4) * sizeof(STAMP_TYPE))

#if   RASOPS_DEPTH <= 8
#define	FILLED_STAMP	15
#elif RASOPS_DEPTH == 15
#define	FILLED_STAMP	30
#else
#define	FILLED_STAMP	60
#endif

/* ################################################################### */

#if RASOPS_DEPTH <= 8

#define	SUBST_STAMP1(off, base)						\
	rp[(off) * 1 + 0] = stamp[base]

#define	SUBST_GLYPH1(index, nibble, off)				\
	do {								\
		int so = STAMP_SHIFT(fr[index], nibble) & STAMP_MASK;	\
		rp[(off) * 1 + 0] = STAMP_READ(so);			\
	} while (0 /* CONSTCOND */)

#endif /* RASOPS_DEPTH <= 8 */

/* ################################################################### */

#if RASOPS_DEPTH == 15

#define	SUBST_STAMP1(off, base)						\
	rp[(off) * 2 + 0] = rp[(off) * 2 + 1] = stamp[base]

#define	SUBST_GLYPH1(index, nibble, off)				\
	do {								\
		int so = STAMP_SHIFT(fr[index], nibble) & STAMP_MASK;	\
		rp[(off) * 2 + 0] = STAMP_READ(so);			\
		rp[(off) * 2 + 1] = STAMP_READ(so +  4);		\
	} while (0 /* CONSTCOND */)

#endif /* RASOPS_DEPTH == 15 */

/* ################################################################### */

#if RASOPS_DEPTH == 24

#define	SUBST_STAMP1(off, base)						\
	do {								\
		rp[(off) * 3 + 0] = stamp[(base) + 0];			\
		rp[(off) * 3 + 1] = stamp[(base) + 1];			\
		rp[(off) * 3 + 2] = stamp[(base) + 2];			\
	} while (0 /* CONSTCOND */)

#define	SUBST_GLYPH1(index, nibble, off)				\
	do {								\
		int so = STAMP_SHIFT(fr[index], nibble) & STAMP_MASK;	\
		rp[(off) * 3 + 0] = STAMP_READ(so);			\
		rp[(off) * 3 + 1] = STAMP_READ(so +  4);		\
		rp[(off) * 3 + 2] = STAMP_READ(so +  8);		\
	} while (0 /* CONSTCOND */)

#endif /* RASOPS_DEPTH == 24 */

/* ################################################################### */

#if RASOPS_DEPTH == 32

#define	SUBST_STAMP1(off, base)						\
	rp[(off) * 4 + 0] = rp[(off) * 4 + 1] =				\
	rp[(off) * 4 + 2] = rp[(off) * 4 + 3] = stamp[base]

#define	SUBST_GLYPH1(index, nibble, off)				\
	do {								\
		int so = STAMP_SHIFT(fr[index], nibble) & STAMP_MASK;	\
		rp[(off) * 4 + 0] = STAMP_READ(so);			\
		rp[(off) * 4 + 1] = STAMP_READ(so +  4);		\
		rp[(off) * 4 + 2] = STAMP_READ(so +  8);		\
		rp[(off) * 4 + 3] = STAMP_READ(so + 12);		\
	} while (0 /* CONSTCOND */)

#endif /* RASOPS_DEPTH == 32 */

/* ################################################################### */

#if   RASOPS_WIDTH == 8
#define	SUBST_STAMP(base) 			\
	do {					\
		SUBST_STAMP1(0, base);		\
		SUBST_STAMP1(1, base);		\
	} while (0 /* CONSTCOND */)
#elif RASOPS_WIDTH == 12
#define	SUBST_STAMP(base)			\
	do {					\
		SUBST_STAMP1(0, base);		\
		SUBST_STAMP1(1, base);		\
		SUBST_STAMP1(2, base);		\
	} while (0 /* CONSTCOND */)
#elif RASOPS_WIDTH == 16
#define	SUBST_STAMP(base)			\
	do {					\
		SUBST_STAMP1(0, base);		\
		SUBST_STAMP1(1, base);		\
		SUBST_STAMP1(2, base);		\
		SUBST_STAMP1(3, base);		\
	} while (0 /* CONSTCOND */)
#endif

/* ################################################################### */

#if   RASOPS_WIDTH == 8
#define	SUBST_GLYPH				\
	do {					\
		SUBST_GLYPH1(0, 1, 0);		\
		SUBST_GLYPH1(0, 0, 1);		\
	} while (0 /* CONSTCOND */)
#elif RASOPS_WIDTH == 12
#define	SUBST_GLYPH				\
	do {					\
		SUBST_GLYPH1(0, 1, 0);		\
		SUBST_GLYPH1(0, 0, 1);		\
		SUBST_GLYPH1(1, 1, 2);		\
	} while (0 /* CONSTCOND */)
#elif RASOPS_WIDTH == 16
#define	SUBST_GLYPH				\
	do {					\
		SUBST_GLYPH1(0, 1, 0);		\
		SUBST_GLYPH1(0, 0, 1);		\
		SUBST_GLYPH1(1, 1, 2);		\
		SUBST_GLYPH1(1, 0, 3);		\
	} while (0 /* CONSTCOND */)
#endif

/* ################################################################### */

#define	NAME(depth, width)	NAME1(depth, width)
#define	NAME1(depth, width)	rasops ## depth ## _putchar ## width

#define	PUTCHAR(depth)		PUTCHAR1(depth)
#define	PUTCHAR1(depth)		rasops ## depth ## _putchar

#define	MAKESTAMP(depth)	MAKESTAMP1(depth)
#define	MAKESTAMP1(depth)	rasops ## depth ## _makestamp

/*
 * Width-optimized putchar function.
 */
static void
NAME(RASOPS_DEPTH, RASOPS_WIDTH)(void *cookie, int row, int col, u_int uc,
    long attr)
{
	struct rasops_info *ri = (struct rasops_info *)cookie;
	struct wsdisplay_font *font = PICK_FONT(ri, uc);
	int height;
	uint8_t *fr;
	bool do_ul;
	STAMP_TYPE *rp, *hp;

	hp = NULL; /* XXX GCC */

	/* check if character fits into font limits */
	if (__predict_false(!CHAR_IN_FONT(uc, font)))
		return;

#ifdef RASOPS_CLIPPING
	/* Catches 'row < 0' case too */
	if ((unsigned)row >= (unsigned)ri->ri_rows)
		return;

	if ((unsigned)col >= (unsigned)ri->ri_cols)
		return;
#endif

	/*
	 * We don't care attributions other than back/foreground
	 * colors when using stamp.
	 */
	do_ul = (attr & WSATTR_UNDERLINE) != 0;
	attr &= (ATTR_MASK_BG | ATTR_MASK_FG);

	/* Recompute stamp? */
	if (attr != stamp_attr || __predict_false(ri != stamp_ri))
		MAKESTAMP(RASOPS_DEPTH)(ri, attr);

	height = font->fontheight;

	rp = (STAMP_TYPE *)(ri->ri_bits + FBOFFSET(ri, row, col));
	if (ri->ri_hwbits)
		hp = (STAMP_TYPE *)(ri->ri_hwbits + FBOFFSET(ri, row, col));

	if (uc == ' ') {
		while (height--) {
			SUBST_STAMP(0);
			if (ri->ri_hwbits) {
				memcpy(hp, rp, SUBST_BYTES);
				DELTA(hp, ri->ri_stride, STAMP_TYPE *);
			}
			DELTA(rp, ri->ri_stride, STAMP_TYPE *);
		}
	} else {
		fr = FONT_GLYPH(uc, font, ri);
		while (height--) {
			SUBST_GLYPH;
			fr += font->stride;
			if (ri->ri_hwbits) {
				memcpy(hp, rp, SUBST_BYTES);
				DELTA(hp, ri->ri_stride, STAMP_TYPE *);
			}
			DELTA(rp, ri->ri_stride, STAMP_TYPE *);
		}
	}

	/* Do underline */
	if (do_ul) {
		DELTA(rp, - ri->ri_stride * ri->ri_ul.off, STAMP_TYPE *);
		if (ri->ri_hwbits)
			DELTA(hp, - ri->ri_stride * ri->ri_ul.off,
			    STAMP_TYPE *);

		for (height = ri->ri_ul.height; height; height--) {
			DELTA(rp, - ri->ri_stride, STAMP_TYPE *);
			SUBST_STAMP(FILLED_STAMP);
			if (ri->ri_hwbits) {
				DELTA(hp, - ri->ri_stride, STAMP_TYPE *);
				memcpy(hp, rp, SUBST_BYTES);
			}
		}
	}
}

#undef	STAMP_TYPE

#undef	SUBST_UNIT
#undef	SUBST_BYTES

#undef	FILLED_STAMP

#undef	SUBST_STAMP1
#undef	SUBST_STAMP

#undef	SUBST_GLYPH1
#undef	SUBST_GLYPH

#undef	NAME
#undef	NAME1

#undef	PUTCHAR
#undef	PUTCHAR1

#undef	MAKESTAMP
#undef	MAKESTAMP1
