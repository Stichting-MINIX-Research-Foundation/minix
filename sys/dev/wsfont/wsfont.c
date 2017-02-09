/* 	$NetBSD: wsfont.c,v 1.59 2015/05/09 16:40:37 mlelstv Exp $	*/

/*-
 * Copyright (c) 1999, 2000, 2001, 2002 The NetBSD Foundation, Inc.
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
__KERNEL_RCSID(0, "$NetBSD: wsfont.c,v 1.59 2015/05/09 16:40:37 mlelstv Exp $");

#include "opt_wsfont.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/malloc.h>
#include <sys/queue.h>

#include <dev/wscons/wsdisplayvar.h>
#include <dev/wscons/wsconsio.h>
#include <dev/wsfont/wsfont.h>

#include "wsfont_glue.h"	/* NRASOPS_ROTATION */

#undef HAVE_FONT

#ifdef FONT_QVSS8x15
#define HAVE_FONT 1
#include <dev/wsfont/qvss8x15.h>
#endif

#ifdef FONT_GALLANT12x22
#define HAVE_FONT 1
#include <dev/wsfont/gallant12x22.h>
#endif

#ifdef FONT_LUCIDA16x29
#define HAVE_FONT 1
#include <dev/wsfont/lucida16x29.h>
#endif

#ifdef FONT_VT220L8x8
#define HAVE_FONT 1
#include <dev/wsfont/vt220l8x8.h>
#endif

#ifdef FONT_VT220L8x10
#define HAVE_FONT 1
#include <dev/wsfont/vt220l8x10.h>
#endif

#ifdef FONT_VT220L8x16
#define HAVE_FONT 1
#include <dev/wsfont/vt220l8x16.h>
#endif

#ifdef FONT_VT220ISO8x8
#define HAVE_FONT 1
#include <dev/wsfont/vt220iso8x8.h>
#endif

#ifdef FONT_VT220ISO8x16
#define HAVE_FONT 1
#include <dev/wsfont/vt220iso8x16.h>
#endif

#ifdef FONT_VT220KOI8x10_KOI8_R
#define HAVE_FONT 1
#include <dev/wsfont/vt220koi8x10.h>
#endif

#ifdef FONT_VT220KOI8x10_KOI8_U
#define HAVE_FONT 1
#define KOI8_U
#include <dev/wsfont/vt220koi8x10.h>
#undef KOI8_U
#endif

#ifdef FONT_SONY8x16
#define HAVE_FONT 1
#include <dev/wsfont/sony8x16.h>
#endif

#ifdef FONT_SONY12x24
#define HAVE_FONT 1
#include <dev/wsfont/sony12x24.h>
#endif

#ifdef FONT_OMRON12x20
#define HAVE_FONT 1
#include <dev/wsfont/omron12x20.h>
#endif

#ifdef FONT_GLASS10x19
#define HAVE_FONT 1
#include <dev/wsfont/glass10x19.h>
#endif

#ifdef FONT_GLASS10x25
#define HAVE_FONT 1
#include <dev/wsfont/glass10x25.h>
#endif

#ifdef FONT_DEJAVU_SANS_MONO12x22
#include <dev/wsfont/DejaVu_Sans_Mono_12x22.h>
#endif

#ifdef FONT_DROID_SANS_MONO12x22
#include <dev/wsfont/Droid_Sans_Mono_12x22.h>
#endif

#ifdef FONT_DROID_SANS_MONO9x18
#include <dev/wsfont/Droid_Sans_Mono_9x18.h>
#endif

#ifdef FONT_DROID_SANS_MONO19x36
#include <dev/wsfont/Droid_Sans_Mono_19x36.h>
#endif

/* Make sure we always have at least one bitmap font. */
#ifndef HAVE_FONT
#define HAVE_FONT 1
#define FONT_BOLD8x16 1
#endif

#ifdef FONT_BOLD8x16
#include <dev/wsfont/bold8x16.h>
#endif

#define	WSFONT_IDENT_MASK	0xffffff00
#define	WSFONT_IDENT_SHIFT	8
#define	WSFONT_BITO_MASK	0x000000f0
#define	WSFONT_BITO_SHIFT	4
#define	WSFONT_BYTEO_MASK	0x0000000f
#define	WSFONT_BYTEO_SHIFT	0

#define WSFONT_BUILTIN	0x01	/* In wsfont.c */
#define WSFONT_STATIC	0x02	/* Font structures not malloc()ed */
#define WSFONT_COPY	0x04	/* Copy of existing font in table */

/* Placeholder struct used for linked list */
struct font {
	TAILQ_ENTRY(font) chain;
	struct	wsdisplay_font *font;
	u_int	lockcount;
	u_int	cookie;
	u_int	flags;
};

/* Our list of built-in fonts */
static struct font builtin_fonts[] = {
#ifdef FONT_BOLD8x16
	{ { NULL, NULL }, &bold8x16, 0, 0, WSFONT_STATIC | WSFONT_BUILTIN  },
#endif
#ifdef FONT_ISO8x16
	{ { NULL, NULL }, &iso8x16, 0, 0, WSFONT_STATIC | WSFONT_BUILTIN },
#endif
#ifdef FONT_COURIER11x18
	{ { NULL, NULL }, &courier11x18, 0, 0, WSFONT_STATIC | WSFONT_BUILTIN },
#endif
#ifdef FONT_GALLANT12x22
	{ { NULL, NULL }, &gallant12x22, 0, 0, WSFONT_STATIC | WSFONT_BUILTIN },
#endif
#ifdef FONT_LUCIDA16x29
	{ { NULL, NULL }, &lucida16x29, 0, 0, WSFONT_STATIC | WSFONT_BUILTIN },
#endif
#ifdef FONT_QVSS8x15
	{ { NULL, NULL }, &qvss8x15, 0, 0, WSFONT_STATIC | WSFONT_BUILTIN },
#endif
#ifdef FONT_VT220L8x8
	{ { NULL, NULL }, &vt220l8x8, 0, 0, WSFONT_STATIC | WSFONT_BUILTIN },
#endif
#ifdef FONT_VT220L8x10
	{ { NULL, NULL }, &vt220l8x10, 0, 0, WSFONT_STATIC | WSFONT_BUILTIN },
#endif
#ifdef FONT_VT220L8x16
	{ { NULL, NULL }, &vt220l8x16, 0, 0, WSFONT_STATIC | WSFONT_BUILTIN },
#endif
#ifdef FONT_VT220ISO8x8
	{ { NULL, NULL }, &vt220iso8x8, 0, 0, WSFONT_STATIC | WSFONT_BUILTIN },
#endif
#ifdef FONT_VT220ISO8x16
	{ { NULL, NULL }, &vt220iso8x16, 0, 0, WSFONT_STATIC | WSFONT_BUILTIN },
#endif
#ifdef FONT_VT220KOI8x10_KOI8_R
	{ { NULL, NULL }, &vt220kr8x10, 0, 0, WSFONT_STATIC | WSFONT_BUILTIN },
#endif
#ifdef FONT_VT220KOI8x10_KOI8_U
	{ { NULL, NULL }, &vt220ku8x10, 0, 0, WSFONT_STATIC | WSFONT_BUILTIN },
#endif
#ifdef FONT_SONY8x16
	{ { NULL, NULL }, &sony8x16, 0, 0, WSFONT_STATIC | WSFONT_BUILTIN },
#endif
#ifdef FONT_SONY12x24
	{ { NULL, NULL }, &sony12x24, 0, 0, WSFONT_STATIC | WSFONT_BUILTIN },
#endif
#ifdef FONT_OMRON12x20
	{ { NULL, NULL }, &omron12x20, 0, 0, WSFONT_STATIC | WSFONT_BUILTIN },
#endif
#ifdef FONT_GLASS10x19
	{ { NULL, NULL }, &Glass_TTY_VT220_10x19, 0, 0, WSFONT_STATIC | WSFONT_BUILTIN },
#endif
#ifdef FONT_GLASS10x25
	{ { NULL, NULL }, &Glass_TTY_VT220_10x25, 0, 0, WSFONT_STATIC | WSFONT_BUILTIN },
#endif
#ifdef FONT_DEJAVU_SANS_MONO12x22
	{ { NULL, NULL }, &DejaVu_Sans_Mono_12x22, 0, 0, WSFONT_STATIC | WSFONT_BUILTIN },
#endif
#ifdef FONT_DROID_SANS_MONO12x22
	{ { NULL, NULL }, &Droid_Sans_Mono_12x22, 0, 0, WSFONT_STATIC | WSFONT_BUILTIN },
#endif
#ifdef FONT_DROID_SANS_MONO9x18
	{ { NULL, NULL }, &Droid_Sans_Mono_9x18, 0, 0, WSFONT_STATIC | WSFONT_BUILTIN },
#endif
#ifdef FONT_DROID_SANS_MONO19x36
	{ { NULL, NULL }, &Droid_Sans_Mono_19x36, 0, 0, WSFONT_STATIC | WSFONT_BUILTIN },
#endif
	{ { NULL, NULL }, NULL, 0, 0, 0 },
};

static TAILQ_HEAD(,font)	list;
static int	ident;

/* Reverse the bit order in a byte */
static const u_char reverse[256] = {
	0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0,
	0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0,
	0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8,
	0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8,
	0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4,
	0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
	0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec,
	0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc,
	0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2,
	0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2,
	0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea,
	0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
	0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6,
	0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6,
	0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee,
	0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe,
	0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1,
	0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
	0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9,
	0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9,
	0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5,
	0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5,
	0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed,
	0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
	0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3,
	0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3,
	0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb,
	0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb,
	0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7,
	0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
	0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef,
	0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff,
};

static struct	font *wsfont_find0(int, int);
static struct	font *wsfont_add0(struct wsdisplay_font *, int);
static void	wsfont_revbit(struct wsdisplay_font *);
static void	wsfont_revbyte(struct wsdisplay_font *);

int
wsfont_make_cookie(int cident, int bito, int byteo)
{

	return ((cident & WSFONT_IDENT_MASK) |
	    (bito << WSFONT_BITO_SHIFT) |
	    (byteo << WSFONT_BYTEO_SHIFT));
}

static void
wsfont_revbit(struct wsdisplay_font *font)
{
	u_char *p, *m;

	p = (u_char *)font->data;
	m = p + font->stride * font->numchars * font->fontheight;

	for (; p < m; p++)
		*p = reverse[*p];
}

static void
wsfont_revbyte(struct wsdisplay_font *font)
{
	int x, l, r, nr;
	u_char *rp;

	if (font->stride == 1)
		return;

	rp = (u_char *)font->data;
	nr = font->numchars * font->fontheight;

	while (nr--) {
		l = 0;
		r = font->stride - 1;

		while (l < r) {
			x = rp[l];
			rp[l] = rp[r];
			rp[r] = x;
			l++, r--;
		}

		rp += font->stride;
	}
}

void
wsfont_enum(void (*cb)(const char *, int, int, int))
{
	struct wsdisplay_font *f;
	struct font *ent;

	TAILQ_FOREACH(ent, &list, chain) {
		f = ent->font;
		cb(f->name, f->fontwidth, f->fontheight, f->stride);
	}
}

#if NRASOPS_ROTATION > 0

struct wsdisplay_font *wsfont_rotate_cw_internal(struct wsdisplay_font *);
struct wsdisplay_font *wsfont_rotate_ccw_internal(struct wsdisplay_font *);

struct wsdisplay_font *
wsfont_rotate_cw_internal(struct wsdisplay_font *font)
{
	int b, n, r, namelen, newstride;
	struct wsdisplay_font *newfont;
	char *newname, *newbits;

	/* Duplicate the existing font... */
	newfont = malloc(sizeof(*font), M_DEVBUF, M_WAITOK);
	if (newfont == NULL)
		return (NULL);

	*newfont = *font;

	namelen = strlen(font->name) + 4;
	newname = malloc(namelen, M_DEVBUF, M_WAITOK);
	strlcpy(newname, font->name, namelen);
	strlcat(newname, "cw", namelen);
	newfont->name = newname;

	/* Allocate a buffer big enough for the rotated font. */
	newstride = (font->fontheight + 7) / 8;
	newbits = malloc(newstride * font->fontwidth * font->numchars,
	    M_DEVBUF, M_WAITOK|M_ZERO);
	if (newbits == NULL) {
		free(newfont, M_DEVBUF);
		return (NULL);
	}

	/* Rotate the font a bit at a time. */
	for (n = 0; n < font->numchars; n++) {
		unsigned char *ch = (unsigned char *)font->data +
		    (n * font->stride * font->fontheight);

		for (r = 0; r < font->fontheight; r++) {
			for (b = 0; b < font->fontwidth; b++) {
				unsigned char *rb;

				rb = ch + (font->stride * r) + (b / 8);
				if (*rb & (0x80 >> (b % 8))) {
					unsigned char *rrb;

					rrb = newbits + newstride - 1 - (r / 8)
					    + (n * newstride * font->fontwidth)
					    + (newstride * b);
					*rrb |= (1 << (r % 8));
				}
			}
		}
	}

	newfont->data = newbits;

	/* Update font sizes. */
	newfont->stride = newstride;
	newfont->fontwidth = font->fontheight;
	newfont->fontheight = font->fontwidth;

	if (wsfont_add(newfont, 0) != 0) {
		/*
		 * If we seem to have rotated this font already, drop the
		 * new one...
		 */
		free(newbits, M_DEVBUF);
		free(newfont, M_DEVBUF);
		newfont = NULL;
	}

	return (newfont);
}

struct wsdisplay_font *
wsfont_rotate_ccw_internal(struct wsdisplay_font *font)
{
	int b, n, r, namelen, newstride;
	struct wsdisplay_font *newfont;
	char *newname, *newbits;

	/* Duplicate the existing font... */
	newfont = malloc(sizeof(*font), M_DEVBUF, M_WAITOK);
	if (newfont == NULL)
		return (NULL);

	*newfont = *font;

	namelen = strlen(font->name) + 4;
	newname = malloc(namelen, M_DEVBUF, M_WAITOK);
	strlcpy(newname, font->name, namelen);
	strlcat(newname, "ccw", namelen);
	newfont->name = newname;

	/* Allocate a buffer big enough for the rotated font. */
	newstride = (font->fontheight + 7) / 8;
	newbits = malloc(newstride * font->fontwidth * font->numchars,
	    M_DEVBUF, M_WAITOK|M_ZERO);
	if (newbits == NULL) {
		free(newfont, M_DEVBUF);
		return (NULL);
	}

	/* Rotate the font a bit at a time. */
	for (n = 0; n < font->numchars; n++) {
		unsigned char *ch = (unsigned char *)font->data +
		    (n * font->stride * font->fontheight);

		for (r = 0; r < font->fontheight; r++) {
			for (b = 0; b < font->fontwidth; b++) {
				unsigned char *rb;

				rb = ch + (font->stride * r) + (b / 8);
				if (*rb & (0x80 >> (b % 8))) {
					unsigned char *rrb;
					int w = font->fontwidth;

					rrb = newbits + (r / 8)
					    + (n * newstride * w)
					    + (newstride * (w - 1 - b));
					*rrb |= (0x80 >> (r % 8));
				}
			}
		}
	}

	newfont->data = newbits;

	/* Update font sizes. */
	newfont->stride = newstride;
	newfont->fontwidth = font->fontheight;
	newfont->fontheight = font->fontwidth;

	if (wsfont_add(newfont, 0) != 0) {
		/*
		 * If we seem to have rotated this font already, drop the
		 * new one...
		 */
		free(newbits, M_DEVBUF);
		free(newfont, M_DEVBUF);
		newfont = NULL;
	}

	return (newfont);
}

int
wsfont_rotate(int cookie, int rotate)
{
	int s, ncookie;
	struct wsdisplay_font *font;
	struct font *origfont;

	s = splhigh();
	origfont = wsfont_find0(cookie, 0xffffffff);
	splx(s);

	switch (rotate) {
	case WSFONT_ROTATE_CW:
		font = wsfont_rotate_cw_internal(origfont->font);
		if (font == NULL)
			return (-1);
		break;

	case WSFONT_ROTATE_CCW:
		font = wsfont_rotate_ccw_internal(origfont->font);
		if (font == NULL)
			return (-1);
		break;

	case WSFONT_ROTATE_UD:
	default:
		return (-1);
	}
	/* rotation works only with bitmap fonts so far */
	ncookie = wsfont_find(font->name, font->fontwidth, font->fontheight, 
	    font->stride, 0, 0, WSFONT_FIND_BITMAP);

	return (ncookie);
}

#endif	/* NRASOPS_ROTATION */

void
wsfont_init(void)
{
	struct font *ent;
	static int again;
	int i;

	if (again != 0)
		return;
	again = 1;

	TAILQ_INIT(&list);
	ent = builtin_fonts;

	for (i = 0; builtin_fonts[i].font != NULL; i++, ent++) {
		ident += (1 << WSFONT_IDENT_SHIFT);
		ent->cookie = wsfont_make_cookie(ident,
		    ent->font->bitorder, ent->font->byteorder);
		TAILQ_INSERT_TAIL(&list, ent, chain);
	}
}

static struct font *
wsfont_find0(int cookie, int mask)
{
	struct font *ent;

	TAILQ_FOREACH(ent, &list, chain) {
		if ((ent->cookie & mask) == (cookie & mask))
			return (ent);
	}

	return (NULL);
}

int
wsfont_matches(struct wsdisplay_font *font, const char *name,
	       int width, int height, int stride, int flags)
{
	int score = 20000;

	/* first weed out fonts the caller doesn't claim support for */
	if (FONT_IS_ALPHA(font)) {
		if ((flags & WSFONT_FIND_ALPHA) == 0)
			return 0;
	} else {
		if ((flags & WSFONT_FIND_BITMAP) == 0)
			return 0;
	}

	if (height != 0 && font->fontheight != height)
		return (0);

	if (width != 0) {
		if ((flags & WSFONT_FIND_BESTWIDTH) == 0) {
			if (font->fontwidth != width)
				return (0);
		} else {
			if (font->fontwidth > width)
				score -= 10000 + min(font->fontwidth - width, 9999);
			else
				score -= min(width - font->fontwidth, 9999);
		}
	}

	if (stride != 0 && font->stride != stride)
		return (0);

	if (name != NULL && strcmp(font->name, name) != 0)
		return (0);

	return (score);
}

int
wsfont_find(const char *name, int width, int height, int stride, int bito, int byteo, int flags)
{
	struct font *ent, *bestent = NULL;
	int score, bestscore = 0;

	TAILQ_FOREACH(ent, &list, chain) {
		score = wsfont_matches(ent->font, name,
				width, height, stride, flags);
		if (score > bestscore) {
			bestscore = score;
			bestent = ent;
		}
	}

	if (bestent != NULL)
		return (wsfont_make_cookie(bestent->cookie, bito, byteo));

	return (-1);
}

void
wsfont_walk(void (*matchfunc)(struct wsdisplay_font *, void *, int), void *cookie)
{
	struct font *ent;

	TAILQ_FOREACH(ent, &list, chain) {
		matchfunc(ent->font, cookie, ent->cookie);
	}
}

static struct font *
wsfont_add0(struct wsdisplay_font *font, int copy)
{
	struct font *ent;
	size_t size;

	ent = malloc(sizeof(struct font), M_DEVBUF, M_WAITOK | M_ZERO);

	/* Is this font statically allocated? */
	if (!copy) {
		ent->font = font;
		ent->flags = WSFONT_STATIC;
	} else {
		void *data;
		char *name;

		ent->font = malloc(sizeof(struct wsdisplay_font), M_DEVBUF,
		    M_WAITOK);
		memcpy(ent->font, font, sizeof(*ent->font));

		size = font->fontheight * font->numchars * font->stride;
		data = malloc(size, M_DEVBUF, M_WAITOK);
		memcpy(data, font->data, size);
		ent->font->data = data;

		name = malloc(strlen(font->name) + 1, M_DEVBUF, M_WAITOK);
		strlcpy(name, font->name, strlen(font->name) + 1);
		ent->font->name = name;
	}

	TAILQ_INSERT_TAIL(&list, ent, chain);
	return (ent);
}

int
wsfont_add(struct wsdisplay_font *font, int copy)
{
	struct font *ent;

	/* Don't allow exact duplicates */
	if (wsfont_find(font->name, font->fontwidth, font->fontheight,
	    font->stride, 0, 0, WSFONT_FIND_ALL) >= 0)
		return (EEXIST);

	ent = wsfont_add0(font, copy);

	ident += (1 << WSFONT_IDENT_SHIFT);
	ent->cookie = wsfont_make_cookie(ident, font->bitorder,
	    font->byteorder);

	return (0);
}

int
wsfont_remove(int cookie)
{
	struct font *ent;

	if ((ent = wsfont_find0(cookie, 0xffffffff)) == NULL)
		return (ENOENT);

	if ((ent->flags & WSFONT_BUILTIN) != 0 || ent->lockcount != 0)
		return (EBUSY);

	if ((ent->flags & WSFONT_STATIC) == 0) {
		free(ent->font->data, M_DEVBUF);
		free(__UNCONST(ent->font->name), M_DEVBUF); /*XXXUNCONST*/
		free(ent->font, M_DEVBUF);
	}

	TAILQ_REMOVE(&list, ent, chain);
	free(ent, M_DEVBUF);

	return (0);
}

int
wsfont_lock(int cookie, struct wsdisplay_font **ptr)
{
	struct font *ent, *neu;
	int bito, byteo;

	if ((ent = wsfont_find0(cookie, 0xffffffff)) == NULL) {
		if ((ent = wsfont_find0(cookie, WSFONT_IDENT_MASK)) == NULL)
			return (ENOENT);

		bito = (cookie & WSFONT_BITO_MASK) >> WSFONT_BITO_SHIFT;
		byteo = (cookie & WSFONT_BYTEO_MASK) >> WSFONT_BYTEO_SHIFT;

		if (ent->lockcount != 0) {
			neu = wsfont_add0(ent->font, 1);
			neu->flags |= WSFONT_COPY;

			aprint_debug("wsfont: font '%s' bito %d byteo %d "
			    "copied to bito %d byteo %d\n",
			    ent->font->name,
			    ent->font->bitorder, ent->font->byteorder,
			    bito, byteo);

			ent = neu;
		}

		if (bito && bito != ent->font->bitorder) {
			wsfont_revbit(ent->font);
			ent->font->bitorder = bito;
		}

		if (byteo && byteo != ent->font->byteorder) {
			wsfont_revbyte(ent->font);
			ent->font->byteorder = byteo;
		}

		ent->cookie = cookie;
	}

	ent->lockcount++;
	*ptr = ent->font;
	return (0);
}

int
wsfont_unlock(int cookie)
{
	struct font *ent;

	if ((ent = wsfont_find0(cookie, 0xffffffff)) == NULL)
		return (ENOENT);

	if (ent->lockcount == 0)
		panic("wsfont_unlock: font not locked");

	if (--ent->lockcount == 0 && (ent->flags & WSFONT_COPY) != 0)
		wsfont_remove(cookie);

	return (0);
}

/*
 * Unicode to font encoding mappings
 */

/*
 * To save memory, font encoding tables use a two level lookup.  First the
 * high byte of the Unicode is used to lookup the level 2 table, then the
 * low byte indexes that table.  Level 2 tables that are not needed are
 * omitted (NULL), and both level 1 and level 2 tables have base and size
 * attributes to keep their size down.
 */

struct wsfont_level1_glyphmap {
	const struct	wsfont_level2_glyphmap **level2;
	int	base;	/* High byte for first level2 entry	*/
	int	size;	/* Number of level2 entries		*/
};

struct wsfont_level2_glyphmap {
	int	base;	/* Low byte for first character		*/
	int	size;	/* Number of characters			*/
	const void	*chars;	/* Pointer to character number entries  */
	int	width;	/* Size of each entry in bytes (1,2,4)  */
};

#define null16			\
	NULL, NULL, NULL, NULL,	\
	NULL, NULL, NULL, NULL,	\
	NULL, NULL, NULL, NULL,	\
	NULL, NULL, NULL, NULL

/*
 * IBM 437 maps
 */

static const u_int8_t ibm437_chars_0[] = {
	 0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
	16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
	32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
	48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
	64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
	80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95,
	96, 97, 98, 99, 100,101,102,103,104,105,106,107,108,109,110,111,
	112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	255,173,155,156, 0, 157, 0,  0,  0,  0, 166,174,170, 0,  0,  0,
	 0, 241,253, 0,  0,  0,  0, 249, 0,  0, 167,175,172,171, 0, 168,
	 0,  0,  0,  0, 142,143,146,128, 0, 144, 0,  0,  0,  0,  0,  0,
	 0, 165, 0,  0,  0,  0, 153, 0,  0,  0,  0,  0, 154, 0,  0,  0,
	133,160,131, 0, 132,134,145,135,138,130,136,137,141,161,140,139,
	 0, 164,149,162,147, 0, 148,246, 0, 151,163,150,129, 0,  0, 152
};

static const u_int8_t ibm437_chars_1[] = {
	159
};

static const u_int8_t ibm437_chars_3[] = {
	226, 0,  0,  0,  0, 233, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	228, 0,  0, 232, 0,  0, 234, 0,  0,  0,  0,  0,  0,  0, 224,225,
	 0, 235,238, 0,  0,  0,  0,  0,  0, 230, 0,  0,  0, 227, 0,  0,
	229,231
};

static const u_int8_t ibm437_chars_32[] = {
	252, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0,  0, 158
};

static const u_int8_t ibm437_chars_34[] = {
	237, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0, 248,250,251, 0,  0,  0, 236, 0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0, 239, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0, 247, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,240,  0,  0,243,
	242
};

static const u_int8_t ibm437_chars_35[] = {
	169, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	244,245
};

static const u_int8_t ibm437_chars_37[] = {
	196,205,179,186, 0,  0,  0,  0,  0,  0,  0,  0, 218,213,214,201,
	191,184,183,187,192,212,211,200,217,190,189,188,195,198, 0,  0,
	199, 0,  0, 204,180,181, 0,  0, 182, 0,  0, 185,194, 0,  0, 209,
	210, 0,  0, 203,193, 0,  0, 207,208, 0,  0, 202,197, 0,  0, 216,
	 0,  0, 215, 0,  0,  0,  0,  0,  0,  0,  0, 206, 0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	223, 0,  0,  0, 220, 0,  0,  0, 219, 0,  0,  0, 221, 0,  0,  0,
	222,176,177,178, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	254
};

static const struct wsfont_level2_glyphmap ibm437_level2_0 =
    { 0, 256, ibm437_chars_0, 1 };

static const struct wsfont_level2_glyphmap ibm437_level2_1 =
    { 146, 1, ibm437_chars_1, 1 };

static const struct wsfont_level2_glyphmap ibm437_level2_3 =
    { 147, 50, ibm437_chars_3, 1 };

static const struct wsfont_level2_glyphmap ibm437_level2_32 =
    { 127, 41, ibm437_chars_32, 1 };

static const struct wsfont_level2_glyphmap ibm437_level2_34 =
    { 5, 97, ibm437_chars_34, 1 };

static const struct wsfont_level2_glyphmap ibm437_level2_35 =
    { 16, 18, ibm437_chars_35, 1 };

static const struct wsfont_level2_glyphmap ibm437_level2_37 =
    { 0, 161, ibm437_chars_37, 1 };

static const struct wsfont_level2_glyphmap *ibm437_level1[] = {
	&ibm437_level2_0, &ibm437_level2_1, NULL, &ibm437_level2_3,
	NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL,
	&ibm437_level2_32, NULL, &ibm437_level2_34, &ibm437_level2_35,
	NULL, &ibm437_level2_37
};

/*
 * ISO-8859-7 maps
 */
static const u_int8_t iso7_chars_0[] = {
	 0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
	16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
	32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
	48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
	64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
	80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95,
	96, 97, 98, 99, 100,101,102,103,104,105,106,107,108,109,110,111,
	112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,
	128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,
	144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,
	160, 0,  0, 163, 0,  0, 166,167,168,169, 0, 171,172,173, 0,  0,
	176,177,178,179,180, 0,  0, 183, 0,  0,  0, 187, 0, 189
};

static const u_int8_t iso7_chars_3[] = {
	182, 0, 184,185,186, 0, 188, 0, 190,191,192,193,194,195,196,197,
	198,199,200,201,202,203,204,205,206,207,208,209, 0, 211,212,213,
	214,215,216,217,218,219,220,221,222,223,224,225,226,227,228,229,
	230,231,232,233,234,235,236,237,238,239,240,241,242,243,244,245,
	246,247,248,249,250,251,252,253,254, 0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 181
};

/* 
 * map all variants of the box drawing characters to the same basic shapes for
 * now, encoded like this:
 *
 *    1
 *    1
 * 888 222
 *    4
 *    4
 *
 * so an upright line would be 0x05
 */
#define FL |WSFONT_FLAG_OPT
static const u_int32_t netbsd_boxes[] = {
/*00*/ 0x0a FL, 0x0a FL, 0x05 FL, 0x05 FL, 0x0a FL, 0x0a FL, 0x05 FL, 0x05 FL,
/*08*/ 0x0a FL, 0x0a FL, 0x05 FL, 0x05 FL, 0x06 FL, 0x06 FL, 0x06 FL, 0x06 FL,
/*10*/ 0x0c FL, 0x0c FL, 0x0c FL, 0x0c FL, 0x03 FL, 0x03 FL, 0x03 FL, 0x03 FL,
/*18*/ 0x09 FL, 0x09 FL, 0x09 FL, 0x09 FL, 0x07 FL, 0x07 FL, 0x07 FL, 0x07 FL,
/*20*/ 0x07 FL, 0x07 FL, 0x07 FL, 0x07 FL, 0x0d FL, 0x0d FL, 0x0d FL, 0x0d FL,
/*28*/ 0x0d FL, 0x0d FL, 0x0d FL, 0x0d FL, 0x0e FL, 0x0e FL, 0x0e FL, 0x0e FL,
/*30*/ 0x0e FL, 0x0e FL, 0x0e FL, 0x0e FL, 0x0b FL, 0x0b FL, 0x0b FL, 0x0b FL,
/*38*/ 0x0b FL, 0x0b FL, 0x0b FL, 0x0b FL, 0x0f FL, 0x0f FL, 0x0f FL, 0x0f FL,
/*40*/ 0x0f FL, 0x0f FL, 0x0f FL, 0x0f FL, 0x0f FL, 0x0f FL, 0x0f FL, 0x0f FL,
/*48*/ 0x0f FL, 0x0f FL, 0x0f FL, 0x0f FL, 0x0a FL, 0x0a FL, 0x05 FL, 0x05 FL,
/*50*/ 0x0a FL, 0x05 FL, 0x06 FL, 0x06 FL, 0x06 FL, 0x0c FL, 0x0c FL, 0x0c FL,
/*58*/ 0x03 FL, 0x03 FL, 0x03 FL, 0x09 FL, 0x09 FL, 0x09 FL, 0x07 FL, 0x07 FL,
/*60*/ 0x07 FL, 0x0d FL, 0x0d FL, 0x0d FL, 0x0e FL, 0x0e FL, 0x0e FL, 0x0b FL,
/*68*/ 0x0b FL, 0x0b FL, 0x0f FL, 0x0f FL, 0x0f FL, 0x06 FL, 0x0c FL, 0x09 FL,
/*70*/ 0x03 FL,    0 FL,    0 FL,    0 FL, 0x08 FL, 0x01 FL, 0x02 FL, 0x04 FL,
/*78*/ 0x08 FL, 0x01 FL, 0x02 FL, 0x04 FL, 0x0a FL, 0x05 FL, 0x0a FL, 0x05 FL
};
#undef FL

static const u_int8_t iso7_chars_32[] = {
	175, 0,  0,  0,  0, 162, 0, 161
};

static const struct wsfont_level2_glyphmap iso7_level2_0 =
    { 0, 190, iso7_chars_0, 1 };

static const struct wsfont_level2_glyphmap iso7_level2_3 =
    { 134, 111, iso7_chars_3, 1 };

static const struct wsfont_level2_glyphmap iso7_level2_32 =
    { 20, 8, iso7_chars_32, 1 };

static const struct wsfont_level2_glyphmap netbsd_box_drawing =
    { 0, 128, netbsd_boxes, 4 };

static const struct wsfont_level2_glyphmap *iso7_level1[] = {
	&iso7_level2_0, NULL, NULL, &iso7_level2_3,
	NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL,
	&iso7_level2_32, NULL, NULL, NULL,
	NULL, &netbsd_box_drawing
};

static const struct wsfont_level2_glyphmap *iso_level1[] = {
	NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL,
	NULL, &netbsd_box_drawing
};

static const struct wsfont_level1_glyphmap encodings[] = {
	{ iso_level1, 0, 0x26 },	/* WSDISPLAY_FONTENC_ISO */
	{ ibm437_level1, 0, 38 },	/* WSDISPLAY_FONTENC_IBM */
	{ NULL, 0, 0 },			/* WSDISPLAY_FONTENC_PCVT */
	{ iso7_level1, 0, 0x26 },	/* WSDISPLAY_FONTENC_ISO7 */
};

#define MAX_ENCODING (sizeof(encodings) / sizeof(encodings[0]))

/*
 * Remap Unicode character to glyph
 */
int
wsfont_map_unichar(struct wsdisplay_font *font, int c)
{
	const struct wsfont_level1_glyphmap *map1;
	const struct wsfont_level2_glyphmap *map2;
	int hi, lo;

	if (font->encoding < 0 || font->encoding >= MAX_ENCODING)
		return (-1);

	hi = (c >> 8);
	lo = c & 255;
	map1 = &encodings[font->encoding];

	if (hi < map1->base || hi >= map1->base + map1->size)
		return (-1);

	map2 = map1->level2[hi - map1->base];

	/* so we don't need an identical level 2 table for hi == 0 */
	if (hi == 0 && font->encoding == WSDISPLAY_FONTENC_ISO)
		return lo;
 
	if (map2 == NULL || lo < map2->base || lo >= map2->base + map2->size)
		return (-1);

	lo -= map2->base;

	switch(map2->width) {
	case 1:
		c = (((const u_int8_t *)map2->chars)[lo]);
		break;
	case 2:
		c = (((const u_int16_t *)map2->chars)[lo]);
		break;
	case 4:
		c = (((const u_int32_t *)map2->chars)[lo]);
		break;
	}

	if (c == 0 && lo != 0)
		return (-1);

	return (c);
}
