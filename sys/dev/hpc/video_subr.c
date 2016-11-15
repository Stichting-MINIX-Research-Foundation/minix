/*	$NetBSD: video_subr.c,v 1.13 2012/02/12 16:34:11 matt Exp $	*/

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by UCHIYAMA Yasushi.
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
__KERNEL_RCSID(0, "$NetBSD: video_subr.c,v 1.13 2012/02/12 16:34:11 matt Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>

#include <machine/bootinfo.h>

#include <dev/hpc/video_subr.h>

#define BPP2 ({								\
	u_int8_t bitmap;						\
	bitmap = *(volatile u_int8_t*)addr;				\
	*(volatile u_int8_t*)addr =					\
		(bitmap & ~(0x3 << ((3 - (x % 4)) * 2)));		\
})

#define BPP4 ({								\
	u_int8_t bitmap;						\
	bitmap = *(volatile u_int8_t*)addr;				\
	*(volatile u_int8_t*)addr =					\
		(bitmap & ~(0xf << ((1 - (x % 2)) * 4)));		\
})

#define BPP8 ({								\
	*(volatile u_int8_t*)addr = 0xff;				\
})

#define BRESENHAM(a, b, c, d, func) ({					\
	u_int32_t fbaddr = vc->vc_fbvaddr;				\
	u_int32_t fbwidth = vc->vc_fbwidth;				\
	u_int32_t fbdepth = vc->vc_fbdepth;				\
	len = a, step = b -1;						\
	if (step == 0)							\
		return;							\
	kstep = len == 0 ? 0 : 1;					\
	for (i = k = 0, j = step / 2; i <= step; i++) {			\
		x = xbase c;						\
		y = ybase d;						\
		addr = fbaddr + (((y * fbwidth + x) * fbdepth) >> 3);	\
		func;							\
		j -= len;						\
		while (j < 0) {						\
			j += step;					\
			k += kstep;					\
		}							\
	}								\
})

#define DRAWLINE(func) ({						\
	if (x < 0) {							\
		if (y < 0) {						\
			if (_y < _x) {					\
				BRESENHAM(_y, _x, -i, -k, func);	\
			} else {					\
				BRESENHAM(_x, _y, -k, -i, func);	\
			}						\
		} else {						\
			if (_y < _x) {					\
				BRESENHAM(_y, _x, -i, +k, func);	\
			} else {					\
				BRESENHAM(_x, _y, -k, +i, func);	\
			}						\
		}							\
	} else {							\
		if (y < 0) {						\
			if (_y < _x) {					\
				BRESENHAM(_y, _x, +i, -k, func);	\
			} else {					\
				BRESENHAM(_x, _y, +k, -i, func);	\
			}						\
		} else {						\
			if (_y < _x) {					\
				BRESENHAM(_y, _x, +i, +k, func);	\
			} else {					\
				BRESENHAM(_x, _y, +k, +i, func);	\
			}						\
		}							\
	}								\
})

#define LINEFUNC(b)							\
static void								\
linebpp##b(struct video_chip *vc, int x0, int y0, int x1, int y1)	\
{									\
	u_int32_t addr;							\
	int i, j, k, len, step, kstep;					\
	int x, _x, y, _y;						\
	int xbase, ybase;						\
	x = x1 - x0;							\
	y = y1 - y0;							\
	_x = abs(x);							\
	_y = abs(y);							\
	xbase = x0;							\
	ybase = y0;							\
	DRAWLINE(BPP##b);						\
}

#define DOTFUNC(b)							\
static void								\
dotbpp##b(struct video_chip *vc, int x, int y)				\
{									\
	u_int32_t addr;							\
	addr = vc->vc_fbvaddr + (((y * vc->vc_fbwidth + x) *		\
				 vc->vc_fbdepth) >> 3);			\
	BPP##b;								\
}

LINEFUNC(2)
LINEFUNC(4)
LINEFUNC(8)
DOTFUNC(2)
DOTFUNC(4)
DOTFUNC(8)
static void linebpp_unimpl(struct video_chip *, int, int, int, int);
static void dotbpp_unimpl(struct video_chip *, int, int);

int
cmap_work_alloc(u_int8_t **r, u_int8_t **g, u_int8_t **b, u_int32_t **rgb,
    int cnt)
{
	KASSERT(LEGAL_CLUT_INDEX(cnt - 1));

#define	ALLOC_BUF(x, bit)						\
	if (x) {							\
		*x = malloc(cnt * sizeof(u_int ## bit ## _t),		\
		    M_DEVBUF, M_WAITOK);				\
		if (*x == 0)						\
			goto errout;					\
	}
	ALLOC_BUF(r, 8);
	ALLOC_BUF(g, 8);
	ALLOC_BUF(b, 8);
	ALLOC_BUF(rgb, 32);
#undef	ALLOCBUF

	return (0);
errout:
	cmap_work_free(*r, *g, *b, *rgb);

	return (ENOMEM);
}

void
cmap_work_free(u_int8_t *r, u_int8_t *g, u_int8_t *b, u_int32_t *rgb)
{
	if (r)
		free(r, M_DEVBUF);
	if (g)
		free(g, M_DEVBUF);
	if (b)
		free(b, M_DEVBUF);
	if (rgb)
		free(rgb, M_DEVBUF);
}

void
rgb24_compose(u_int32_t *rgb24, u_int8_t *r, u_int8_t *g, u_int8_t *b, int cnt)
{
	int i;
	KASSERT(rgb24 && r && g && b && LEGAL_CLUT_INDEX(cnt - 1));

	for (i = 0; i < cnt; i++) {
		*rgb24++ = RGB24(r[i], g[i], b[i]);
	}
}

void
rgb24_decompose(u_int32_t *rgb24, u_int8_t *r, u_int8_t *g, u_int8_t *b,
    int cnt)
{
	int i;
	KASSERT(rgb24 && r && g && b && LEGAL_CLUT_INDEX(cnt - 1));

	for (i = 0; i < cnt; i++) {
		u_int32_t rgb = *rgb24++;
		*r++ = (rgb >> 16) & 0xff;
		*g++ = (rgb >> 8) & 0xff;
		*b++ = rgb & 0xff;
	}
}

/*
 * Debug routines.
 */
void
video_calibration_pattern(struct video_chip *vc)
{
	int x, y;

	x = vc->vc_fbwidth - 40;
	y = vc->vc_fbheight - 40;
	video_line(vc, 40, 40, x , 40);
	video_line(vc, x , 40, x , y );
	video_line(vc, x , y , 40, y );
	video_line(vc, 40, y , 40, 40);
	video_line(vc, 40, 40, x , y );
	video_line(vc, x,  40, 40, y );
}

static void
linebpp_unimpl(struct video_chip *vc,
	       int x0, int y0,
	       int x1, int y1)
{

	return;
}

static void
dotbpp_unimpl(struct video_chip *vc, int x, int y)
{

	return;
}

void
video_attach_drawfunc(struct video_chip *vc)
{
	switch (vc->vc_fbdepth) {
	default:
		vc->vc_drawline = linebpp_unimpl;
		vc->vc_drawdot = dotbpp_unimpl;
		break;
	case 8:
		vc->vc_drawline = linebpp8;
		vc->vc_drawdot = dotbpp8;
		break;
	case 4:
		vc->vc_drawline = linebpp4;
		vc->vc_drawdot = dotbpp4;
		break;
	case 2:
		vc->vc_drawline = linebpp2;
		vc->vc_drawdot = dotbpp2;
		break;
	}
}

void
video_line(struct video_chip *vc, int x0, int y0, int x1, int y1)
{
	if (vc->vc_drawline)
		vc->vc_drawline(vc, x0, y0, x1, y1);
}

void
video_dot(struct video_chip *vc, int x, int y)
{
	if (vc->vc_drawdot)
		vc->vc_drawdot(vc, x, y);
}

int
video_reverse_color(void)
{
	struct {
		int reverse, normal;
	} ctype[] = {
		{ BIFB_D2_M2L_3,	BIFB_D2_M2L_0	},
		{ BIFB_D2_M2L_3x2,	BIFB_D2_M2L_0x2	},
		{ BIFB_D8_FF,		BIFB_D8_00	},
		{ BIFB_D16_FFFF,	BIFB_D16_0000,	},
		{ -1, -1 } /* terminator */
	}, *ctypep;
	u_int16_t fbtype;

	/* check reverse color */
	fbtype = bootinfo->fb_type;
	for (ctypep = ctype; ctypep->normal != -1 ;  ctypep++) {
		if (fbtype == ctypep->normal) {
			return (0);
		} else if (fbtype == ctypep->reverse) {
			return (1);
		}
	}
	printf(": WARNING unknown frame buffer type 0x%04x.\n", fbtype);
	return (0);
}
