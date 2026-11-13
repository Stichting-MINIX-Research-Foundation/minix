/* 	$NetBSD: rasops4.c,v 1.20.2.1 2019/08/15 12:21:27 martin Exp $	*/

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
__KERNEL_RCSID(0, "$NetBSD: rasops4.c,v 1.20.2.1 2019/08/15 12:21:27 martin Exp $");

#ifdef _KERNEL_OPT
#include "opt_rasops.h"
#endif

#include <sys/param.h>

#include <machine/endian.h>

#include "../wscons/wsconsio.h"
#include "../wscons/wsdisplayvar.h"

#define	_RASOPS_PRIVATE
#define	RASOPS_DEPTH	4
#include "rasops.h"
#include "rasops_masks.h"

static void	rasops4_copycols(void *, int, int, int, int);
static void	rasops4_erasecols(void *, int, int, int, long);
static void	rasops4_do_cursor(struct rasops_info *);
static void	rasops4_putchar(void *, int, int col, u_int, long);
#ifndef RASOPS_SMALL
static void	rasops4_putchar8(void *, int, int col, u_int, long);
static void	rasops4_putchar12(void *, int, int col, u_int, long);
static void	rasops4_putchar16(void *, int, int col, u_int, long);
static void	rasops4_makestamp(struct rasops_info *, long);
#endif

#ifndef RASOPS_SMALL
/* stamp for optimized character blitting */
static uint16_t			stamp[16];
static long			stamp_attr;
static struct rasops_info	*stamp_ri;

/*
 * offset = STAMP_SHIFT(fontbits, nibble #) & STAMP_MASK
 * destination = STAMP_READ(offset)
 */
#define STAMP_SHIFT(fb, n)	((n) ? (fb) >> 4 : (fb))
#define STAMP_MASK		0xf
#define STAMP_READ(o)		stamp[o]
#endif

/*
 * Initialize rasops_info struct for this colordepth.
 */
void rasops4_init(struct rasops_info *ri)
{

	if ((ri->ri_font->fontwidth & 1) != 0) {
		ri->ri_ops.erasecols = rasops4_erasecols;
		ri->ri_ops.copycols = rasops4_copycols;
		ri->ri_do_cursor = rasops4_do_cursor;
	}

	switch (ri->ri_font->fontwidth) {
#ifndef RASOPS_SMALL
	case 8:
		ri->ri_ops.putchar = rasops4_putchar8;
		break;
	case 12:
		ri->ri_ops.putchar = rasops4_putchar12;
		break;
	case 16:
		ri->ri_ops.putchar = rasops4_putchar16;
		break;
#endif	/* !RASOPS_SMALL */
	default:
		ri->ri_ops.putchar = rasops4_putchar;
		return;
	}

#ifndef RASOPS_SMALL
	stamp_attr = -1;
	stamp_ri = NULL;
#endif
}

#ifndef RASOPS_SMALL
/*
 * Recompute the blitting stamp.
 */
static void rasops4_makestamp(struct rasops_info *ri, long attr)
{
	int i;
	uint32_t bg, fg;

	stamp_attr = attr;
	stamp_ri = ri;

	bg = ATTR_BG(ri, attr) & 0xf;
	fg = ATTR_FG(ri, attr) & 0xf;

	for (i = 0; i < 16; i++) {
#if BYTE_ORDER == LITTLE_ENDIAN
		if ((ri->ri_flg & RI_BSWAP) == 0)
#else
		if ((ri->ri_flg & RI_BSWAP) != 0)
#endif
		{
			/* little endian */
			stamp[i]  = (i & 1 ? fg : bg) << 12;
			stamp[i] |= (i & 2 ? fg : bg) << 8;
			stamp[i] |= (i & 4 ? fg : bg) << 4;
			stamp[i] |= (i & 8 ? fg : bg) << 0;
		} else {
			/* big endian */
			stamp[i]  = (i & 1 ? fg : bg) << 0;
			stamp[i] |= (i & 2 ? fg : bg) << 4;
			stamp[i] |= (i & 4 ? fg : bg) << 8;
			stamp[i] |= (i & 8 ? fg : bg) << 12;
		}
	}
}

/*
 * Width-optimized putchar functions
 */
#define	RASOPS_WIDTH	8
#include "rasops_putchar_width.h"
#undef	RASOPS_WIDTH

#define	RASOPS_WIDTH	12
#include "rasops_putchar_width.h"
#undef	RASOPS_WIDTH

#define	RASOPS_WIDTH	16
#include "rasops_putchar_width.h"
#undef	RASOPS_WIDTH

#endif	/* !RASOPS_SMALL */

/* rasops4_putchar */
#undef	RASOPS_AA
#include "rasops1-4_putchar.h"

/*
 * Grab routines common to depths where (bpp < 8)
 */
#include "rasops_bitops.h"
