/* $NetBSD: splash.c,v 1.12 2012/06/02 14:24:00 martin Exp $ */

/*-
 * Copyright (c) 2006 Jared D. McNeill <jmcneill@invisible.ca>
 * All rights reserved.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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
__KERNEL_RCSID(0, "$NetBSD: splash.c,v 1.12 2012/06/02 14:24:00 martin Exp $");

#include "opt_splash.h"

/* XXX */
#define NSPLASH8  1
#define	NSPLASH16 1
#define	NSPLASH32 1

#include <sys/param.h>
#include <sys/device.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/kernel.h>
#include <sys/kthread.h>

#include <dev/splash/splash.h>
#include <dev/stbi/stbi.h>

#ifdef SPLASHSCREEN

static struct {
	const u_char	*data;
	size_t		datalen;
} splash_image = { NULL, 0 };

#define SPLASH_INDEX(r, g, b)						\
	((((r) >> 6) << 4) | (((g) >> 6) << 2) | (((b) >> 6) << 0))

static uint8_t splash_palette[SPLASH_CMAP_SIZE][3] = {
	{ 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0x55 },
	{ 0x00, 0x00, 0xaa },
	{ 0x00, 0x00, 0xff },
	{ 0x00, 0x55, 0x00 },
	{ 0x00, 0x55, 0x55 },
	{ 0x00, 0x55, 0xaa },
	{ 0x00, 0x55, 0xff },
	{ 0x00, 0xaa, 0x00 },
	{ 0x00, 0xaa, 0x55 },
	{ 0x00, 0xaa, 0xaa },
	{ 0x00, 0xaa, 0xff },
	{ 0x00, 0xff, 0x00 },
	{ 0x00, 0xff, 0x55 },
	{ 0x00, 0xff, 0xaa },
	{ 0x00, 0xff, 0xff },
	{ 0x55, 0x00, 0x00 },
	{ 0x55, 0x00, 0x55 },
	{ 0x55, 0x00, 0xaa },
	{ 0x55, 0x00, 0xff },
	{ 0x55, 0x55, 0x00 },
	{ 0x55, 0x55, 0x55 },
	{ 0x55, 0x55, 0xaa },
	{ 0x55, 0x55, 0xff },
	{ 0x55, 0xaa, 0x00 },
	{ 0x55, 0xaa, 0x55 },
	{ 0x55, 0xaa, 0xaa },
	{ 0x55, 0xaa, 0xff },
	{ 0x55, 0xff, 0x00 },
	{ 0x55, 0xff, 0x55 },
	{ 0x55, 0xff, 0xaa },
	{ 0x55, 0xff, 0xff },
	{ 0xaa, 0x00, 0x00 },
	{ 0xaa, 0x00, 0x55 },
	{ 0xaa, 0x00, 0xaa },
	{ 0xaa, 0x00, 0xff },
	{ 0xaa, 0x55, 0x00 },
	{ 0xaa, 0x55, 0x55 },
	{ 0xaa, 0x55, 0xaa },
	{ 0xaa, 0x55, 0xff },
	{ 0xaa, 0xaa, 0x00 },
	{ 0xaa, 0xaa, 0x55 },
	{ 0xaa, 0xaa, 0xaa },
	{ 0xaa, 0xaa, 0xff },
	{ 0xaa, 0xff, 0x00 },
	{ 0xaa, 0xff, 0x55 },
	{ 0xaa, 0xff, 0xaa },
	{ 0xaa, 0xff, 0xff },
	{ 0xff, 0x00, 0x00 },
	{ 0xff, 0x00, 0x55 },
	{ 0xff, 0x00, 0xaa },
	{ 0xff, 0x00, 0xff },
	{ 0xff, 0x55, 0x00 },
	{ 0xff, 0x55, 0x55 },
	{ 0xff, 0x55, 0xaa },
	{ 0xff, 0x55, 0xff },
	{ 0xff, 0xaa, 0x00 },
	{ 0xff, 0xaa, 0x55 },
	{ 0xff, 0xaa, 0xaa },
	{ 0xff, 0xaa, 0xff },
	{ 0xff, 0xff, 0x00 },
	{ 0xff, 0xff, 0x55 },
	{ 0xff, 0xff, 0xaa },
	{ 0xff, 0xff, 0xff },
};

#if NSPLASH8 > 0
static void	splash_render8(struct splash_info *, const char *, int,
			       int, int, int, int);
#endif
#if NSPLASH16 > 0
static void	splash_render16(struct splash_info *, const char *, int,
				int, int, int, int);
#endif
#if NSPLASH32 > 0
static void	splash_render32(struct splash_info *, const char *, int,
				int, int, int, int);
#endif

int
splash_setimage(const void *imgdata, size_t imgdatalen)
{
	if (splash_image.data != NULL) {
		aprint_debug("WARNING: %s: already initialized\n", __func__);
		return EBUSY;
	}

	aprint_verbose("%s: splash image @ %p, %zu bytes\n",
	    __func__, imgdata, imgdatalen);
	splash_image.data = imgdata;
	splash_image.datalen = imgdatalen;

	return 0;
}

int
splash_get_cmap(int index, uint8_t *r, uint8_t *g, uint8_t *b)
{
	if (index < SPLASH_CMAP_OFFSET ||
	    index >= SPLASH_CMAP_OFFSET + SPLASH_CMAP_SIZE)
		return ERANGE;

	*r = splash_palette[index - SPLASH_CMAP_OFFSET][0];
	*g = splash_palette[index - SPLASH_CMAP_OFFSET][1];
	*b = splash_palette[index - SPLASH_CMAP_OFFSET][2];

	return 0;
}

int
splash_render(struct splash_info *si, int flg)
{
	char *data = NULL;
	int xoff, yoff, width, height, comp;
	int error = 0;

	if (splash_image.data == NULL) {
		aprint_error("WARNING: %s: not initialized\n", __func__);
		return ENXIO;
	}

	data = stbi_load_from_memory(splash_image.data,
	    splash_image.datalen, &width, &height, &comp, STBI_rgb);
	if (data == NULL) {
		aprint_error("WARNING: couldn't load splash image: %s\n",
		    stbi_failure_reason());
		return EINVAL;
	}
	aprint_debug("%s: splash loaded, width %d height %d comp %d\n",
	    __func__, width, height, comp);

	/* XXX */
	if (flg & SPLASH_F_CENTER) {
		xoff = (si->si_width - width) / 2;
		yoff = (si->si_height - height) / 2;
	} else
		xoff = yoff = 0;

	switch (si->si_depth) {
#if NSPLASH8 > 0
	case 8:
		splash_render8(si, data, xoff, yoff, width, height, flg);
		break;
#endif
#if NSPLASH16 > 0
	case 16:
		splash_render16(si, data, xoff, yoff, width, height, flg);
		break;
#endif
#if NSPLASH32 > 0
	case 32:
		splash_render32(si, data, xoff, yoff, width, height, flg);
		break;
#endif
	default:
		aprint_error("WARNING: Splash not supported at %dbpp\n",
		    si->si_depth);
		error = EINVAL;
	}

	if (data)
		stbi_image_free(data);

	return error;
}

#if NSPLASH8 > 0

static void
splash_render8(struct splash_info *si, const char *data, int xoff, int yoff,
	       int swidth, int sheight, int flg)
{
	const char *d;
	u_char *fb, *p;
	u_char pix[3];
	int x, y, i;
	int filled;

	fb = si->si_bits;

	if (flg & SPLASH_F_FILL)
		filled = 0;
	else
		filled = 1;

	d = data;
	fb += xoff + yoff * si->si_stride;

	for (y = 0; y < sheight; y++) {
		for (x = 0; x < swidth; x++) {
			pix[0] = *d++;
			pix[1] = *d++;
			pix[2] = *d++;
			if (filled == 0) {
				p = si->si_bits;
				i = 0;
				while (i < si->si_height*si->si_stride) {
					p[i] = SPLASH_INDEX(
					    pix[0], pix[1], pix[2]) +
					    SPLASH_CMAP_OFFSET;
					i++;
				}
				filled = 1;
			}
			fb[x] = SPLASH_INDEX(pix[0], pix[1], pix[2]) +
				    SPLASH_CMAP_OFFSET;
		}
		fb += si->si_stride;
	}

	/* If we've just written to the shadow fb, copy it to the display */
	if (si->si_hwbits) {
		if (flg & SPLASH_F_FILL) {
			memcpy(si->si_hwbits, si->si_bits,
			    si->si_height*si->si_width);
		} else {
			u_char *rp, *hrp;

			rp = si->si_bits + xoff + (yoff * si->si_width);
			hrp = si->si_hwbits + xoff + (yoff * si->si_width);

			for (y = 0; y < sheight; y++) {
				memcpy(hrp, rp, swidth);
				rp += si->si_stride;
				hrp += si->si_stride;
			}
		}
	}

	return;
}
#endif /* !NSPLASH8 > 0 */

#if NSPLASH16 > 0
#define RGBTO16(b, o, x, c)					\
	do {							\
		uint16_t *_ptr = (uint16_t *)(&(b)[(o)]);	\
		*_ptr = (((c)[(x)*3+0] / 8) << 11) |		\
			(((c)[(x)*3+1] / 4) << 5) |		\
			(((c)[(x)*3+2] / 8) << 0);		\
	} while (0)

static void
splash_render16(struct splash_info *si, const char *data, int xoff, int yoff,
		int swidth, int sheight, int flg)
{
	const char *d;
	u_char *fb, *p;
	u_char pix[3];
	int x, y, i;
	int filled;

	fb = si->si_bits;

	if (flg & SPLASH_F_FILL)
		filled = 0;
	else
		filled = 1;

	d = data;
	fb += xoff * 2 + yoff * si->si_stride;

	for (y = 0; y < sheight; y++) {
		for (x = 0; x < swidth; x++) {
			pix[0] = *d++;
			pix[1] = *d++;
			pix[2] = *d++;
			if (filled == 0) {
				p = si->si_bits;
				i = 0;
				while (i < si->si_height*si->si_stride) {
					RGBTO16(p, i, 0, pix);
					i += 2;
				}
				filled = 1;
			}
			RGBTO16(fb, x*2, 0, pix);
		}
		fb += si->si_stride;
	}

	/* If we've just written to the shadow fb, copy it to the display */
	if (si->si_hwbits) {
		if (flg & SPLASH_F_FILL) {
			memcpy(si->si_hwbits, si->si_bits,
			    si->si_height*si->si_stride);
		} else {
			u_char *rp, *hrp;

			rp = si->si_bits + (xoff * 2) + (yoff * si->si_stride);
			hrp = si->si_hwbits + (xoff * 2) +
			    (yoff * si->si_stride);

			for (y = 0; y < sheight; y++) {
				memcpy(hrp, rp, swidth * 2);
				rp += si->si_stride;
				hrp += si->si_stride;
			}
		}
	}

	return;
}
#undef RGBTO16
#endif /* !NSPLASH16 > 0 */

#if NSPLASH32 > 0
static void
splash_render32(struct splash_info *si, const char *data, int xoff, int yoff,
		int swidth, int sheight, int flg)
{
	const char *d;
	u_char *fb, *p;
	u_char pix[3];
	int x, y, i;
	int filled;

	fb = si->si_bits;

	if (flg & SPLASH_F_FILL)
		filled = 0;
	else
		filled = 1;

	d = data;
	fb += xoff * 4 + yoff * si->si_stride;

	for (y = 0; y < sheight; y++) {
		for (x = 0; x < swidth; x++) {
			pix[0] = *d++;
			pix[1] = *d++;
			pix[2] = *d++;
			if (filled == 0) {
				p = si->si_bits;
				i = 0;
				while (i < si->si_height*si->si_stride) {
					p[i++] = pix[2];
					p[i++] = pix[1];
					p[i++] = pix[0];
					p[i++] = 0;
				}
				filled = 1;
			}
			fb[x*4+0] = pix[2];
			fb[x*4+1] = pix[1];
			fb[x*4+2] = pix[0];
			fb[x*4+3] = 0;
		}
		fb += si->si_stride;
	}

	/* If we've just written to the shadow fb, copy it to the display */
	if (si->si_hwbits) {
		if (flg & SPLASH_F_FILL) {
			memcpy(si->si_hwbits, si->si_bits,
			    si->si_height*si->si_stride);
		} else {
			u_char *rp, *hrp;

			rp = si->si_bits + (xoff * 4) + (yoff * si->si_stride);
			hrp = si->si_hwbits + (xoff * 4) +
			    (yoff * si->si_stride);

			for (y = 0; y < sheight; y++) {
				memcpy(hrp, rp, swidth * 4);
				rp += si->si_stride;
				hrp += si->si_stride;
			}
		}
	}

	return;
}
#endif /* !NSPLASH32 > 0 */

#endif /* !SPLASHSCREEN */
