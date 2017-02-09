/*	$NetBSD: aurateconv.c,v 1.19 2011/11/23 23:07:31 jmcneill Exp $	*/

/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by TAMURA Kent
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
__KERNEL_RCSID(0, "$NetBSD: aurateconv.c,v 1.19 2011/11/23 23:07:31 jmcneill Exp $");

#include <sys/systm.h>
#include <sys/types.h>
#include <sys/device.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/select.h>
#include <sys/audioio.h>

#include <dev/audio_if.h>
#include <dev/audiovar.h>
#include <dev/auconv.h>

#ifndef _KERNEL
#include <stdio.h>
#include <string.h>
#endif

/* #define AURATECONV_DEBUG */
#ifdef AURATECONV_DEBUG
#define DPRINTF(x)	printf x
#else
#define DPRINTF(x)
#endif

typedef struct aurateconv {
	stream_filter_t base;
	audio_params_t from;
	audio_params_t to;
	long	count;
	int32_t	prev[AUDIO_MAX_CHANNELS];
	int32_t	next[AUDIO_MAX_CHANNELS];
} aurateconv_t;

static int aurateconv_fetch_to(struct audio_softc *, stream_fetcher_t *,
			       audio_stream_t *, int);
static void aurateconv_dtor(stream_filter_t *);
static int aurateconv_slinear16_LE(aurateconv_t *, audio_stream_t *,
				   int, int, int);
static int aurateconv_slinear24_LE(aurateconv_t *, audio_stream_t *,
				   int, int, int);
static int aurateconv_slinear32_LE(aurateconv_t *, audio_stream_t *,
				   int, int, int);
static int aurateconv_slinear16_BE(aurateconv_t *, audio_stream_t *,
				   int, int, int);
static int aurateconv_slinear24_BE(aurateconv_t *, audio_stream_t *,
				   int, int, int);
static int aurateconv_slinear32_BE(aurateconv_t *, audio_stream_t *,
				   int, int, int);

static int32_t int32_mask[33] = {
	0x0, 0x80000000, 0xc0000000, 0xe0000000,
	0xf0000000, 0xf8000000, 0xfc000000, 0xfe000000,
	0xff000000, 0xff800000, 0xffc00000, 0xffe00000,
	0xfff00000, 0xfff80000, 0xfffc0000, 0xfffe0000,
	0xffff0000, 0xffff8000, 0xffffc000, 0xffffe000,
	0xfffff000, 0xfffff800, 0xfffffc00, 0xfffffe00,
	0xffffff00, 0xffffff80, 0xffffffc0, 0xffffffe0,
	0xfffffff0, 0xfffffff8, 0xfffffffc, 0xfffffffe,
	0xffffffff
};

stream_filter_t *
aurateconv(struct audio_softc *sc, const audio_params_t *from,
	   const audio_params_t *to)
{
	aurateconv_t *this;

	DPRINTF(("Construct '%s' filter: rate=%u:%u chan=%u:%u prec=%u/%u:%u/"
		 "%u enc=%u:%u\n", __func__, from->sample_rate,
		 to->sample_rate, from->channels, to->channels,
		 from->validbits, from->precision, to->validbits,
		 to->precision, from->encoding, to->encoding));
#ifdef DIAGNOSTIC
	/* check from/to */
	if (from->channels == to->channels
	    && from->sample_rate == to->sample_rate)
		printf("%s: no conversion\n", __func__); /* No conversion */

	if (from->encoding != to->encoding
	    || from->precision != to->precision
	    || from->validbits != to->validbits) {
		printf("%s: encoding/precision must not be changed\n", __func__);
		return NULL;
	}
	if ((from->encoding != AUDIO_ENCODING_SLINEAR_LE
	     && from->encoding != AUDIO_ENCODING_SLINEAR_BE)
	    || (from->precision != 16 && from->precision != 24 && from->precision != 32)) {
		printf("%s: encoding/precision must be SLINEAR_LE 16/24/32bit, "
		       "or SLINEAR_BE 16/24/32bit", __func__);
		return NULL;
	}

	if (from->channels > AUDIO_MAX_CHANNELS || from->channels <= 0
	    || to->channels > AUDIO_MAX_CHANNELS || to->channels <= 0) {
		printf("%s: invalid channels: from=%u to=%u\n",
		       __func__, from->channels, to->channels);
		return NULL;
	}

	if (from->sample_rate <= 0 || to->sample_rate <= 0) {
		printf("%s: invalid sampling rate: from=%u to=%u\n",
		       __func__, from->sample_rate, to->sample_rate);
		return NULL;
	}
#endif

	/* initialize context */
	this = malloc(sizeof(aurateconv_t), M_DEVBUF, M_WAITOK | M_ZERO);
	this->count = from->sample_rate < to->sample_rate
		? to->sample_rate + from->sample_rate : 0;
	this->from = *from;
	this->to = *to;

	/* initialize vtbl */
	this->base.base.fetch_to = aurateconv_fetch_to;
	this->base.dtor = aurateconv_dtor;
	this->base.set_fetcher = stream_filter_set_fetcher;
	this->base.set_inputbuffer = stream_filter_set_inputbuffer;
	return &this->base;
}

static void
aurateconv_dtor(struct stream_filter *this)
{
	if (this != NULL)
		free(this, M_DEVBUF);
}

static int
aurateconv_fetch_to(struct audio_softc *sc, stream_fetcher_t *self,
		    audio_stream_t *dst, int max_used)
{
	aurateconv_t *this;
	int m, err, frame_dst, frame_src;

	this = (aurateconv_t *)self;
	frame_dst = (this->to.precision / 8) * this->to.channels;
	frame_src = (this->from.precision / 8) * this->from.channels;
	max_used = max_used / frame_dst * frame_dst;
	if (max_used <= 0)
		max_used = frame_dst;
	/* calculate required input size for output max_used bytes */
	m = max_used / frame_dst;
	m *= this->from.sample_rate;
	m /= this->to.sample_rate;
	m *= frame_src;
	if (m <= 0)
		m = frame_src;

	if ((err = this->base.prev->fetch_to(sc, this->base.prev, this->base.src, m)))
	    return err;
	m = (dst->end - dst->start) / frame_dst * frame_dst;
	m = min(m, max_used);

	switch (this->from.encoding) {
	case AUDIO_ENCODING_SLINEAR_LE:
		switch (this->from.precision) {
		case 16:
			return aurateconv_slinear16_LE(this, dst, m,
						       frame_src, frame_dst);
		case 24:
			return aurateconv_slinear24_LE(this, dst, m,
						       frame_src, frame_dst);
		case 32:
			return aurateconv_slinear32_LE(this, dst, m,
						       frame_src, frame_dst);
		}
		break;
	case AUDIO_ENCODING_SLINEAR_BE:
		switch (this->from.precision) {
		case 16:
			return aurateconv_slinear16_BE(this, dst, m,
						       frame_src, frame_dst);
		case 24:
			return aurateconv_slinear24_BE(this, dst, m,
						       frame_src, frame_dst);
		case 32:
			return aurateconv_slinear32_BE(this, dst, m,
						       frame_src, frame_dst);
		}
		break;
	}
	printf("%s: internal error: unsupported encoding: enc=%u prec=%u\n",
	       __func__, this->from.encoding, this->from.precision);
	return 0;
}


#define READ_S8LE(P)		*(const int8_t*)(P)
#define WRITE_S8LE(P, V)	*(int8_t*)(P) = V
#define READ_S8BE(P)		*(const int8_t*)(P)
#define WRITE_S8BE(P, V)	*(int8_t*)(P) = V
#if BYTE_ORDER == LITTLE_ENDIAN
# define READ_S16LE(P)		*(const int16_t*)(P)
# define WRITE_S16LE(P, V)	*(int16_t*)(P) = V
# define READ_S16BE(P)		(int16_t)((P)[0] | ((P)[1]<<8))
# define WRITE_S16BE(P, V)	\
	do { \
		int vv = V; \
		(P)[0] = vv; \
		(P)[1] = vv >> 8; \
	} while (/*CONSTCOND*/ 0)
# define READ_S32LE(P)		*(const int32_t*)(P)
# define WRITE_S32LE(P, V)	*(int32_t*)(P) = V
# define READ_S32BE(P)		(int32_t)((P)[3] | ((P)[2]<<8) | ((P)[1]<<16) | (((int8_t)((P)[0]))<<24))
# define WRITE_S32BE(P, V)	\
	do { \
		int vvv = V; \
		(P)[0] = vvv >> 24; \
		(P)[1] = vvv >> 16; \
		(P)[2] = vvv >> 8; \
		(P)[3] = vvv; \
	} while (/*CONSTCOND*/ 0)
#else  /* !LITTLE_ENDIAN */
# define READ_S16LE(P)		(int16_t)((P)[0] | ((P)[1]<<8))
# define WRITE_S16LE(P, V)	\
	do { \
		int vv = V; \
		(P)[0] = vv; \
		(P)[1] = vv >> 8; \
	} while (/*CONSTCOND*/ 0)
# define READ_S16BE(P)		*(const int16_t*)(P)
# define WRITE_S16BE(P, V)	*(int16_t*)(P) = V
# define READ_S32LE(P)		(int32_t)((P)[0] | ((P)[1]<<8) | ((P)[2]<<16) | (((int8_t)((P)[3]))<<24))
# define WRITE_S32LE(P, V)	\
	do { \
		int vvv = V; \
		(P)[0] = vvv; \
		(P)[1] = vvv >> 8; \
		(P)[2] = vvv >> 16; \
		(P)[3] = vvv >> 24; \
	} while (/*CONSTCOND*/ 0)
# define READ_S32BE(P)		*(const int32_t*)(P)
# define WRITE_S32BE(P, V)	*(int32_t*)(P) = V
#endif /* !LITTLE_ENDIAN */
#define READ_S24LE(P)		(int32_t)((P)[0] | ((P)[1]<<8) | (((int8_t)((P)[2]))<<16))
#define WRITE_S24LE(P, V)	\
	do { \
		int vvv = V; \
		(P)[0] = vvv; \
		(P)[1] = vvv >> 8; \
		(P)[2] = vvv >> 16; \
	} while (/*CONSTCOND*/ 0)
#define READ_S24BE(P)		(int32_t)((P)[2] | ((P)[1]<<8) | (((int8_t)((P)[0]))<<16))
#define WRITE_S24BE(P, V)	\
	do { \
		int vvv = V; \
		(P)[0] = vvv >> 16; \
		(P)[1] = vvv >> 8; \
		(P)[2] = vvv; \
	} while (/*CONSTCOND*/ 0)

#define READ_Sn(BITS, EN, V, STREAM, RP, PAR)	\
	do { \
		int j; \
		for (j = 0; j < (PAR)->channels; j++) { \
			(V)[j] = READ_S##BITS##EN(RP); \
			RP = audio_stream_add_outp(STREAM, RP, (BITS) / NBBY); \
		} \
	} while (/*CONSTCOND*/ 0)
#define WRITE_Sn(BITS, EN, V, STREAM, WP, FROM, TO)	\
	do { \
		if ((FROM)->channels == 2 && (TO)->channels == 1) { \
			WRITE_S##BITS##EN(WP, ((V)[0] + (V)[1]) / 2); \
			WP = audio_stream_add_inp(STREAM, WP, (BITS) / NBBY); \
		} else if (from->channels <= to->channels) { \
			int j; \
			for (j = 0; j < (FROM)->channels; j++) { \
				WRITE_S##BITS##EN(WP, (V)[j]); \
				WP = audio_stream_add_inp(STREAM, WP, (BITS) / NBBY); \
			} \
			if (j == 1 && 1 < (TO)->channels) { \
				WRITE_S##BITS##EN(WP, (V)[0]); \
				WP = audio_stream_add_inp(STREAM, WP, (BITS) / NBBY); \
				j++; \
			} \
			for (; j < (TO)->channels; j++) { \
				WRITE_S##BITS##EN(WP, 0); \
				WP = audio_stream_add_inp(STREAM, WP, (BITS) / NBBY); \
			} \
		} else {	/* from->channels < to->channels */ \
			int j; \
			for (j = 0; j < (TO)->channels; j++) { \
				WRITE_S##BITS##EN(WP, (V)[j]); \
				WP = audio_stream_add_inp(STREAM, WP, (BITS) / NBBY); \
			} \
		} \
	} while (/*CONSTCOND*/ 0)

/*
 * Function template
 *
 *   Don't use this for 32bit data because this linear interpolation overflows
 *   for 32bit data.
 */
#define AURATECONV_SLINEAR(BITS, EN)	\
static int \
aurateconv_slinear##BITS##_##EN (aurateconv_t *this, audio_stream_t *dst, \
				 int m, int frame_src, int frame_dst) \
{ \
	uint8_t *w; \
	const uint8_t *r; \
	const audio_params_t *from, *to; \
	audio_stream_t *src; \
	int32_t v[AUDIO_MAX_CHANNELS]; \
	int32_t *prev, *next, c256; \
	int i, values_size; \
 \
	src = this->base.src; \
	w = dst->inp; \
	r = src->outp; \
	DPRINTF(("%s: ENTER w=%p r=%p dst->used=%d src->used=%d\n", \
		__func__, w, r, dst->used, src->used)); \
	from = &this->from; \
	to = &this->to; \
	if (this->from.sample_rate == this->to.sample_rate) { \
		while (dst->used < m && src->used >= frame_src) { \
			READ_Sn(BITS, EN, v, src, r, from); \
			WRITE_Sn(BITS, EN, v, dst, w, from, to); \
		} \
	} else if (to->sample_rate < from->sample_rate) { \
		while (dst->used < m && src->used >= frame_src) { \
			READ_Sn(BITS, EN, v, src, r, from); \
			this->count += to->sample_rate; \
			if (this->count >= from->sample_rate) { \
				this->count -= from->sample_rate; \
				WRITE_Sn(BITS, EN, v, dst, w, from, to); \
			} \
		} \
	} else { \
		/* Initial value of this->count >= to->sample_rate */ \
		values_size = sizeof(int32_t) * from->channels; \
		prev = this->prev; \
		next = this->next; \
		while (dst->used < m \
		       && ((this->count >= to->sample_rate && src->used >= frame_src) \
			   || this->count < to->sample_rate)) { \
			if (this->count >= to->sample_rate) { \
				this->count -= to->sample_rate; \
				memcpy(prev, next, values_size); \
				READ_Sn(BITS, EN, next, src, r, from); \
			} \
			c256 = this->count * 256 / to->sample_rate; \
			for (i = 0; i < from->channels; i++) \
				v[i] = (c256 * next[i] + (256 - c256) * prev[i]) >> 8; \
			WRITE_Sn(BITS, EN, v, dst, w, from, to); \
			this->count += from->sample_rate; \
		} \
	} \
	DPRINTF(("%s: LEAVE w=%p r=%p dst->used=%d src->used=%d\n", \
		__func__, w, r, dst->used, src->used)); \
	dst->inp = w; \
	src->outp = r; \
	return 0; \
}

/*
 * Function template for 32bit container
 */
#define AURATECONV_SLINEAR32(EN)	\
static int \
aurateconv_slinear32_##EN (aurateconv_t *this, audio_stream_t *dst, \
			   int m, int frame_src, int frame_dst) \
{ \
	uint8_t *w; \
	const uint8_t *r; \
	const audio_params_t *from, *to; \
	audio_stream_t *src; \
	int32_t v[AUDIO_MAX_CHANNELS]; \
	int32_t *prev, *next; \
	int64_t c256, mask; \
	int i, values_size, used_src, used_dst; \
 \
	src = this->base.src; \
	w = dst->inp; \
	r = src->outp; \
	used_dst = audio_stream_get_used(dst); \
	used_src = audio_stream_get_used(src); \
	from = &this->from; \
	to = &this->to; \
	if (this->from.sample_rate == this->to.sample_rate) { \
		while (used_dst < m && used_src >= frame_src) { \
			READ_Sn(32, EN, v, src, r, from); \
			used_src -= frame_src; \
			WRITE_Sn(32, EN, v, dst, w, from, to); \
			used_dst += frame_dst; \
		} \
	} else if (to->sample_rate < from->sample_rate) { \
		while (used_dst < m && used_src >= frame_src) { \
			READ_Sn(32, EN, v, src, r, from); \
			used_src -= frame_src; \
			this->count += to->sample_rate; \
			if (this->count >= from->sample_rate) { \
				this->count -= from->sample_rate; \
				WRITE_Sn(32, EN, v, dst, w, from, to); \
				used_dst += frame_dst; \
			} \
		} \
	} else { \
		/* Initial value of this->count >= to->sample_rate */ \
		values_size = sizeof(int32_t) * from->channels; \
		mask = int32_mask[to->validbits]; \
		prev = this->prev; \
		next = this->next; \
		while (used_dst < m \
		       && ((this->count >= to->sample_rate && used_src >= frame_src) \
			   || this->count < to->sample_rate)) { \
			if (this->count >= to->sample_rate) { \
				this->count -= to->sample_rate; \
				memcpy(prev, next, values_size); \
				READ_Sn(32, EN, next, src, r, from); \
				used_src -= frame_src; \
			} \
			c256 = this->count * 256 / to->sample_rate; \
			for (i = 0; i < from->channels; i++) \
				v[i] = (int32_t)((c256 * next[i] + (INT64_C(256) - c256) * prev[i]) >> 8) & mask; \
			WRITE_Sn(32, EN, v, dst, w, from, to); \
			used_dst += frame_dst; \
			this->count += from->sample_rate; \
		} \
	} \
	dst->inp = w; \
	src->outp = r; \
	return 0; \
}

AURATECONV_SLINEAR(16, LE)
AURATECONV_SLINEAR(24, LE)
AURATECONV_SLINEAR32(LE)
AURATECONV_SLINEAR(16, BE)
AURATECONV_SLINEAR(24, BE)
AURATECONV_SLINEAR32(BE)
