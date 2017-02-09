/*	$NetBSD: auconv.h,v 1.16 2011/11/23 23:07:31 jmcneill Exp $	*/

/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson.
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

#ifndef _SYS_DEV_AUCONV_H_
#define _SYS_DEV_AUCONV_H_
#include <dev/audio_if.h>

/* common routines for stream_filter_t */
extern void stream_filter_set_fetcher(stream_filter_t *, stream_fetcher_t *);
extern void stream_filter_set_inputbuffer(stream_filter_t *, audio_stream_t *);
extern stream_filter_t *auconv_nocontext_filter_factory
	(int (*)(struct audio_softc *, stream_fetcher_t *, audio_stream_t *, int));
extern void auconv_nocontext_filter_dtor(struct stream_filter *);
#define FILTER_LOOP_PROLOGUE(SRC, SRCFRAME, DST, DSTFRAME, MAXUSED) \
do { \
	const uint8_t *s; \
	uint8_t *d; \
	s = (SRC)->outp; \
	d = (DST)->inp; \
	for (; audio_stream_get_used(DST) < MAXUSED \
		&& audio_stream_get_used(SRC) >= SRCFRAME; \
		s = audio_stream_add_outp(SRC, s, SRCFRAME), \
		d = audio_stream_add_inp(DST, d, DSTFRAME))
#define FILTER_LOOP_EPILOGUE(SRC, DST)	\
	(SRC)->outp = s; \
	(DST)->inp = d; \
} while (/*CONSTCOND*/0)


/* Convert between signed and unsigned. */
extern stream_filter_factory_t change_sign8;
extern stream_filter_factory_t change_sign16;
/* Convert between little and big endian. */
extern stream_filter_factory_t swap_bytes;
extern stream_filter_factory_t swap_bytes_change_sign16;
/* Byte expansion/contraction */
extern stream_filter_factory_t linear8_to_linear16;
extern stream_filter_factory_t linear16_to_linear8;
/* sampling rate conversion (aurateconv.c) */
extern stream_filter_factory_t aurateconv;

struct audio_format {
	/**
	 * Device-dependent audio drivers may use this field freely.
	 */
	void *driver_data;

	/**
	 * combination of AUMODE_PLAY and AUMODE_RECORD
	 */
	int32_t mode;

	/**
	 * Encoding type.  AUDIO_ENCODING_*.
	 * Don't use AUDIO_ENCODING_SLINEAR/ULINEAR/LINEAR/LINEAR8
	 */
	u_int encoding;

	/**
	 * The size of valid bits in one sample.
	 * It must be <= precision.
	 */
	u_int validbits;

	/**
	 * The bit size of one sample.
	 * It must be >= validbits, and is usualy a multiple of 8.
	 */
	u_int precision;

	/**
	 * The number of channels.  >= 1
	 */
	u_int channels;

	u_int channel_mask;
#define	AUFMT_UNKNOWN_POSITION		0U
#define	AUFMT_FRONT_LEFT		0x00001U /* USB audio compatible */
#define	AUFMT_FRONT_RIGHT		0x00002U /* USB audio compatible */
#define	AUFMT_FRONT_CENTER		0x00004U /* USB audio compatible */
#define	AUFMT_LOW_FREQUENCY		0x00008U /* USB audio compatible */
#define	AUFMT_BACK_LEFT			0x00010U /* USB audio compatible */
#define	AUFMT_BACK_RIGHT		0x00020U /* USB audio compatible */
#define	AUFMT_FRONT_LEFT_OF_CENTER	0x00040U /* USB audio compatible */
#define	AUFMT_FRONT_RIGHT_OF_CENTER	0x00080U /* USB audio compatible */
#define	AUFMT_BACK_CENTER		0x00100U /* USB audio compatible */
#define	AUFMT_SIDE_LEFT			0x00200U /* USB audio compatible */
#define	AUFMT_SIDE_RIGHT		0x00400U /* USB audio compatible */
#define	AUFMT_TOP_CENTER		0x00800U /* USB audio compatible */
#define	AUFMT_TOP_FRONT_LEFT		0x01000U
#define	AUFMT_TOP_FRONT_CENTER		0x02000U
#define	AUFMT_TOP_FRONT_RIGHT		0x04000U
#define	AUFMT_TOP_BACK_LEFT		0x08000U
#define	AUFMT_TOP_BACK_CENTER		0x10000U
#define	AUFMT_TOP_BACK_RIGHT		0x20000U

#define	AUFMT_MONAURAL		AUFMT_FRONT_CENTER
#define	AUFMT_STEREO		(AUFMT_FRONT_LEFT | AUFMT_FRONT_RIGHT)
#define	AUFMT_SURROUND4		(AUFMT_STEREO | AUFMT_BACK_LEFT \
				| AUFMT_BACK_RIGHT)
#define	AUFMT_DOLBY_5_1		(AUFMT_SURROUND4 | AUFMT_FRONT_CENTER \
				| AUFMT_LOW_FREQUENCY)

	/**
	 * 0: frequency[0] is lower limit, and frequency[1] is higher limit.
	 * 1-16: frequency[0] to frequency[frequency_type-1] are valid.
	 */
	u_int frequency_type;

#define	AUFMT_MAX_FREQUENCIES	16
	/**
	 * sampling rates
	 */
	u_int frequency[AUFMT_MAX_FREQUENCIES];
};

#define	AUFMT_INVALIDATE(fmt)	(fmt)->mode |= 0x80000000
#define	AUFMT_VALIDATE(fmt)	(fmt)->mode &= 0x7fffffff
#define	AUFMT_IS_VALID(fmt)	(((fmt)->mode & 0x80000000) == 0)

struct audio_encoding_set;
extern int auconv_set_converter(const struct audio_format *, int,
				int, const audio_params_t *, int,
				stream_filter_list_t *);
extern int auconv_create_encodings(const struct audio_format *, int,
				   struct audio_encoding_set **);
extern int auconv_delete_encodings(struct audio_encoding_set *);
extern int auconv_query_encoding(const struct audio_encoding_set *,
				 audio_encoding_t *);

#endif /* !_SYS_DEV_AUCONV_H_ */
