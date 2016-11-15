/*	$NetBSD: bicons.c,v 1.14 2011/07/17 20:54:51 joerg Exp $	*/

/*-
 * Copyright (c) 1999-2001
 *         Shin Takemura and PocketBSD Project. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the PocketBSD project
 *	and its contributors.
 * 4. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: bicons.c,v 1.14 2011/07/17 20:54:51 joerg Exp $");

#define HALF_FONT

#include <sys/param.h>
#include <sys/device.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <dev/cons.h>

#include <machine/bootinfo.h>
#include <sys/bus.h>
#include <machine/platid.h>

#include <dev/hpc/biconsvar.h>
#include <dev/hpc/bicons.h>
extern u_int8_t font_clR8x8_data[];
#define FONT_HEIGHT	8
#define FONT_WIDTH	1

static void put_oxel_D2_M2L_3(u_int8_t *, u_int8_t, u_int8_t);
static void put_oxel_D2_M2L_3x2(u_int8_t *, u_int8_t, u_int8_t);
static void put_oxel_D2_M2L_0(u_int8_t *, u_int8_t, u_int8_t);
static void put_oxel_D2_M2L_0x2(u_int8_t *, u_int8_t, u_int8_t);
static void put_oxel_D4_M2L_F(u_int8_t *, u_int8_t, u_int8_t);
static void put_oxel_D4_M2L_Fx2(u_int8_t *, u_int8_t, u_int8_t);
static void put_oxel_D4_M2L_0(u_int8_t *, u_int8_t, u_int8_t);
static void put_oxel_D4_M2L_0x2(u_int8_t *, u_int8_t, u_int8_t);
static void put_oxel_D8_00(u_int8_t *, u_int8_t, u_int8_t);
static void put_oxel_D8_FF(u_int8_t *, u_int8_t, u_int8_t);
static void put_oxel_D16_0000(u_int8_t *, u_int8_t, u_int8_t);
static void put_oxel_D16_FFFF(u_int8_t *, u_int8_t, u_int8_t);

static const struct {
	int type;
	const char *name;
	void (*func)(u_int8_t *, u_int8_t, u_int8_t);
	u_int8_t clear_byte;
	int16_t oxel_bytes;
} fb_table[] = {
	{ BIFB_D2_M2L_3,	BIFBN_D2_M2L_3,
	  put_oxel_D2_M2L_3,	0,	2	},
	{ BIFB_D2_M2L_3x2,	BIFBN_D2_M2L_3x2,
	  put_oxel_D2_M2L_3x2,	0,	1	},
	{ BIFB_D2_M2L_0,	BIFBN_D2_M2L_0,
	  put_oxel_D2_M2L_0,	0xff,	2	},
	{ BIFB_D2_M2L_0x2,	BIFBN_D2_M2L_0x2,
	  put_oxel_D2_M2L_0x2,	0xff,	1	},
	{ BIFB_D4_M2L_F,	BIFBN_D4_M2L_F,
	  put_oxel_D4_M2L_F,	0x00,	4	},
	{ BIFB_D4_M2L_Fx2,	BIFBN_D4_M2L_Fx2,
	  put_oxel_D4_M2L_Fx2,	0x00,	2	},
	{ BIFB_D4_M2L_0,	BIFBN_D4_M2L_0,
	  put_oxel_D4_M2L_0,	0xff,	4	},
	{ BIFB_D4_M2L_0x2,	BIFBN_D4_M2L_0x2,
	  put_oxel_D4_M2L_0x2,	0xff,	2	},
	{ BIFB_D8_00,		BIFBN_D8_00,
	  put_oxel_D8_00,	0xff,	8	},
	{ BIFB_D8_FF,		BIFBN_D8_FF,
	  put_oxel_D8_FF,	0x00,	8	},
	{ BIFB_D16_0000,	BIFBN_D16_0000,
	  put_oxel_D16_0000,	0xff,	16	},
	{ BIFB_D16_FFFF,	BIFBN_D16_FFFF,
	  put_oxel_D16_FFFF,	0x00,	16	},
};
#define FB_TABLE_SIZE (sizeof(fb_table) / sizeof(*fb_table))

static u_int8_t	*fb_vram;
static int16_t	fb_line_bytes;
static u_int8_t	fb_clear_byte;
int16_t	bicons_ypixel;
int16_t	bicons_xpixel;
#ifdef HALF_FONT
static int16_t	fb_oxel_bytes	= 1;
int16_t	bicons_width	= 80;
void	(*fb_put_oxel)(u_int8_t *, u_int8_t, u_int8_t) = put_oxel_D2_M2L_3x2;
#else /* HALF_FONT */
static int16_t	fb_oxel_bytes	= 2;
int16_t	bicons_width	= 40;
void	(*fb_put_oxel)(u_int8_t *, u_int8_t, u_int8_t) = put_oxel_D2_M2L_3;
#endif /* HALF_FONT */
int16_t bicons_height;
static int16_t curs_x;
static int16_t curs_y;

static int bicons_priority;
int biconscninit(struct consdev *);
void biconscnprobe(struct consdev *);
void biconscnputc(dev_t, int);
int biconscngetc(dev_t);	/* harmless place holder */

static void draw_char(int, int, int);
static void clear(int, int);
static void scroll(int, int, int);
static void bicons_puts(const char *);
static void bicons_printf(const char *, ...) __unused;

int
bicons_init(struct consdev *cndev)
{

	if (biconscninit(cndev) != 0)
		return (1);

	biconscnprobe(cndev);

	return (0);	/* success */
}

int
biconscninit(struct consdev *cndev)
{
	int fb_index = -1;

	if (bootinfo->fb_addr == 0) {
		/* Bootinfo don't have frame buffer address */
		return (1);
	}

	for (fb_index = 0; fb_index < FB_TABLE_SIZE; fb_index++)
		if (fb_table[fb_index].type == bootinfo->fb_type)
			break;

	if (FB_TABLE_SIZE <= fb_index || fb_index == -1) {
		/* Unknown frame buffer type, don't enable bicons. */
		return (1);
	}

	fb_vram = (u_int8_t *)bootinfo->fb_addr;
	fb_line_bytes = bootinfo->fb_line_bytes;
	bicons_xpixel = bootinfo->fb_width;
	bicons_ypixel = bootinfo->fb_height;

	fb_put_oxel = fb_table[fb_index].func;
	fb_clear_byte = fb_table[fb_index].clear_byte;
	fb_oxel_bytes = fb_table[fb_index].oxel_bytes;

	bicons_width = bicons_xpixel / (8 * FONT_WIDTH);
	bicons_height = bicons_ypixel / FONT_HEIGHT;
	clear(0, bicons_ypixel);

	curs_x = 0;
	curs_y = 0;

	bicons_puts("builtin console type = ");
	bicons_puts(fb_table[fb_index].name);
	bicons_puts("\n");

	return (0);
}

void
biconscnprobe(struct consdev *cndev)
{
	extern const struct cdevsw biconsdev_cdevsw;
	int maj;

	/* locate the major number */
	maj = cdevsw_lookup_major(&biconsdev_cdevsw);

	cndev->cn_dev = makedev(maj, 0);
	cndev->cn_pri = bicons_priority;
}

void
bicons_set_priority(int priority)
{
	bicons_priority = priority;
}

int
biconscngetc(dev_t dev)
{
	printf("no input method. reboot me.\n");
	while (1)
		;
	/* NOTREACHED */
	return 0;
}

void
biconscnputc(dev_t dev, int c)
{
	int line_feed = 0;

	switch (c) {
	case 0x08: /* back space */
		if (--curs_x < 0) {
			curs_x = 0;
		}
		/* erase character ar cursor position */
		draw_char(curs_x, curs_y, ' ');
		break;
	case '\r':
		curs_x = 0;
		break;
	case '\n':
		curs_x = 0;
		line_feed = 1;
		break;
	default:
		draw_char(curs_x, curs_y, c);
		if (bicons_width <= ++curs_x) {
			curs_x = 0;
			line_feed = 1;
		}
	}

	if (line_feed) {
		if (bicons_height <= ++curs_y) {
			/* scroll up */
			scroll(FONT_HEIGHT, (bicons_height - 1) * FONT_HEIGHT,
			       - FONT_HEIGHT);
			clear((bicons_height - 1) * FONT_HEIGHT, FONT_HEIGHT);
			curs_y--;
		}
	}
}

void
bicons_puts(const char *s)
{
	while (*s)
		biconscnputc(0, *s++);
}


void
bicons_putn(const char *s, int n)
{
	while (0 < n--)
		biconscnputc(0, *s++);
}

void
#ifdef __STDC__
bicons_printf(const char *fmt, ...)
#else
bicons_printf(fmt, va_alist)
	char *fmt;
	va_dcl
#endif
{
	va_list ap;
	char buf[0x100];

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	bicons_puts(buf);
}

static void
draw_char(int x, int y, int c)
{
	int i;
	u_int8_t *p;

	if (!fb_vram)
		return;

	p = &fb_vram[(y * FONT_HEIGHT * fb_line_bytes) +
		    x * FONT_WIDTH * fb_oxel_bytes];
	for (i = 0; i < FONT_HEIGHT; i++) {
		(*fb_put_oxel)(p, font_clR8x8_data
			       [FONT_WIDTH * (FONT_HEIGHT * c + i)], 0xff);
		p += (fb_line_bytes);
	}
}

static void
clear(int y, int height)
{
	u_int8_t *p;

	if (!fb_vram)
		return;

	p = &fb_vram[y * fb_line_bytes];

	while (0 < height--) {
		memset(p, fb_clear_byte,
		       bicons_width * fb_oxel_bytes * FONT_WIDTH);
		p += fb_line_bytes;
	}
}

static void
scroll(int y, int height, int d)
{
	u_int8_t *from, *to;

	if (!fb_vram)
		return;

	if (d < 0) {
		from = &fb_vram[y * fb_line_bytes];
		to = from + d * fb_line_bytes;
		while (0 < height--) {
			memcpy(to, from, bicons_width * fb_oxel_bytes);
			from += fb_line_bytes;
			to += fb_line_bytes;
		}
	} else {
		from = &fb_vram[(y + height - 1) * fb_line_bytes];
		to = from + d * fb_line_bytes;
		while (0 < height--) {
			memcpy(to, from, bicons_xpixel * fb_oxel_bytes / 8);
			from -= fb_line_bytes;
			to -= fb_line_bytes;
		}
	}
}

/*=============================================================================
 *
 *	D2_M2L_3
 *
 */
static void
put_oxel_D2_M2L_3(u_int8_t *xaddr, u_int8_t data, u_int8_t mask)
{
#if 1
	u_int16_t *addr = (u_int16_t *)xaddr;
	static u_int16_t map0[] = {
		0x0000, 0x0300, 0x0c00, 0x0f00, 0x3000, 0x3300, 0x3c00, 0x3f00,
		0xc000, 0xc300, 0xcc00, 0xcf00, 0xf000, 0xf300, 0xfc00, 0xff00,
	};
	static u_int16_t map1[] = {
		0x0000, 0x0003, 0x000c, 0x000f, 0x0030, 0x0033, 0x003c, 0x003f,
		0x00c0, 0x00c3, 0x00cc, 0x00cf, 0x00f0, 0x00f3, 0x00fc, 0x00ff,
	};
	*addr = (map1[data >> 4] | map0[data & 0x0f]);
#else
	static u_int8_t map[] = {
		0x00, 0x03, 0x0c, 0x0f, 0x30, 0x33, 0x3c, 0x3f,
		0xc0, 0xc3, 0xcc, 0xcf, 0xf0, 0xf3, 0xfc, 0xff,
	};
	u_int8_t *addr = xaddr;

	*addr++ = (map[(data >> 4) & 0x0f] & map[(mask >> 4) & 0x0f]) |
		(*addr & ~map[(mask >> 4) & 0x0f]);
	*addr   = (map[(data >> 0) & 0x0f] & map[(mask >> 0) & 0x0f]) |
		(*addr & ~map[(mask >> 0) & 0x0f]);
#endif
}

/*=============================================================================
 *
 *	D2_M2L_3x2
 *
 */
static void
put_oxel_D2_M2L_3x2(u_int8_t *xaddr, u_int8_t data, u_int8_t mask)
{
	register u_int8_t odd = (data & 0xaa);
	register u_int8_t even = (data & 0x55);

	*xaddr = (odd | (even << 1)) | ((odd >> 1) & even);
}

/*=============================================================================
 *
 *	D2_M2L_0
 *
 */
static void
put_oxel_D2_M2L_0(u_int8_t *xaddr, u_int8_t data, u_int8_t mask)
{
#if 1
	u_int16_t *addr = (u_int16_t *)xaddr;
	static u_int16_t map0[] = {
		0xff00, 0xfc00, 0xf300, 0xf000, 0xcf00, 0xcc00, 0xc300, 0xc000,
		0x3f00, 0x3c00, 0x3300, 0x3000, 0x0f00, 0x0c00, 0x0300, 0x0000,
	};
	static u_int16_t map1[] = {
		0x00ff, 0x00fc, 0x00f3, 0x00f0, 0x00cf, 0x00cc, 0x00c3, 0x00c0,
		0x003f, 0x003c, 0x0033, 0x0030, 0x000f, 0x000c, 0x0003, 0x0000,
	};
	*addr = (map1[data >> 4] | map0[data & 0x0f]);
#else
	static u_int8_t map[] = {
		0x00, 0x03, 0x0c, 0x0f, 0x30, 0x33, 0x3c, 0x3f,
		0xc0, 0xc3, 0xcc, 0xcf, 0xf0, 0xf3, 0xfc, 0xff,
	};
	u_int8_t *addr = xaddr;

	*addr++ = (~(map[(data >> 4) & 0x0f] & map[(mask >> 4) & 0x0f])) |
		(*addr & ~map[(mask >> 4) & 0x0f]);
	*addr   = (~(map[(data >> 0) & 0x0f] & map[(mask >> 0) & 0x0f])) |
		(*addr & ~map[(mask >> 0) & 0x0f]);
#endif
}

/*=============================================================================
 *
 *	D2_M2L_0x2
 *
 */
static void
put_oxel_D2_M2L_0x2(u_int8_t *xaddr, u_int8_t data, u_int8_t mask)
{
	register u_int8_t odd = (data & 0xaa);
	register u_int8_t even = (data & 0x55);

	*xaddr = ~((odd | (even << 1)) | ((odd >> 1) & even));
}

/*=============================================================================
 *
 *	D4_M2L_F
 *
 */
static void
put_oxel_D4_M2L_F(u_int8_t *xaddr, u_int8_t data, u_int8_t mask)
{
	u_int32_t *addr = (u_int32_t *)xaddr;
	static u_int32_t map[] = {
		0x0000, 0x0f00, 0xf000, 0xff00, 0x000f, 0x0f0f, 0xf00f, 0xff0f,
		0x00f0, 0x0ff0, 0xf0f0, 0xfff0, 0x00ff, 0x0fff, 0xf0ff, 0xffff,
	};
	*addr = (map[data >> 4] | (map[data & 0x0f] << 16));
}

/*=============================================================================
 *
 *	D4_M2L_Fx2
 *
 */
static void
put_oxel_D4_M2L_Fx2(u_int8_t *xaddr, u_int8_t data, u_int8_t mask)
{
	u_int16_t *addr = (u_int16_t *)xaddr;
	static u_int16_t map[] = {
		0x00, 0x08, 0x08, 0x0f, 0x80, 0x88, 0x88, 0x8f,
		0x80, 0x88, 0x88, 0x8f, 0xf0, 0xf8, 0xf8, 0xff,
	};

	*addr = (map[data >> 4] | (map[data & 0x0f] << 8));
}

/*=============================================================================
 *
 *	D4_M2L_0
 *
 */
static void
put_oxel_D4_M2L_0(u_int8_t *xaddr, u_int8_t data, u_int8_t mask)
{
	u_int32_t *addr = (u_int32_t *)xaddr;
	static u_int32_t map[] = {
		0xffff, 0xf0ff, 0x0fff, 0x00ff, 0xfff0, 0xf0f0, 0x0ff0, 0x00f0,
		0xff0f, 0xf00f, 0x0f0f, 0x000f, 0xff00, 0xf000, 0x0f00, 0x0000,
	};
	*addr = (map[data >> 4] | (map[data & 0x0f] << 16));
}

/*=============================================================================
 *
 *	D4_M2L_0x2
 *
 */
static void
put_oxel_D4_M2L_0x2(u_int8_t *xaddr, u_int8_t data, u_int8_t mask)
{
	u_int16_t *addr = (u_int16_t *)xaddr;
	static u_int16_t map[] = {
		0xff, 0xf8, 0xf8, 0xf0, 0x8f, 0x88, 0x88, 0x80,
		0x8f, 0x88, 0x88, 0x80, 0x0f, 0x08, 0x08, 0x00,
	};

	*addr = (map[data >> 4] | (map[data & 0x0f] << 8));
}

/*=============================================================================
 *
 *	D8_00
 *
 */
static void
put_oxel_D8_00(u_int8_t *xaddr, u_int8_t data, u_int8_t mask)
{
	int i;
	u_int8_t *addr = xaddr;

	for (i = 0; i < 8; i++) {
		if (mask & 0x80) {
			*addr = (data & 0x80) ? 0x00 : 0xFF;
		}
		addr++;
		data <<= 1;
		mask <<= 1;
	}
}

/*=============================================================================
 *
 *	D8_FF
 *
 */
static void
put_oxel_D8_FF(u_int8_t *xaddr, u_int8_t data, u_int8_t mask)
{
	int i;
	u_int8_t *addr = xaddr;

	for (i = 0; i < 8; i++) {
		if (mask & 0x80) {
			*addr = (data & 0x80) ? 0xFF : 0x00;
		}
		addr++;
		data <<= 1;
		mask <<= 1;
	}
}

/*=============================================================================
 *
 *	D16_0000
 *
 */
static void
put_oxel_D16_0000(u_int8_t *xaddr, u_int8_t data, u_int8_t mask)
{
	int i;
	u_int16_t *addr = (u_int16_t *)xaddr;

	for (i = 0; i < 8; i++) {
		if (mask & 0x80) {
			*addr = (data & 0x80) ? 0x0000 : 0xFFFF;
		}
		addr++;
		data <<= 1;
		mask <<= 1;
	}
}

/*=============================================================================
 *
 *	D16_FFFF
 *
 */
static void
put_oxel_D16_FFFF(u_int8_t *xaddr, u_int8_t data, u_int8_t mask)
{
	int i;
	u_int16_t *addr = (u_int16_t *)xaddr;

	for (i = 0; i < 8; i++) {
		if (mask & 0x80) {
			*addr = (data & 0x80) ? 0xFFFF : 0x0000;
		}
		addr++;
		data <<= 1;
		mask <<= 1;
	}
}
