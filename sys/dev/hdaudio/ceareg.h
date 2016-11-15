/* $NetBSD: ceareg.h,v 1.1 2015/03/28 14:09:59 jmcneill Exp $ */

/*
 * Copyright (c) 2011 Jared D. McNeill <jmcneill@invisible.ca>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _CEAREG_H
#define _CEAREG_H

/* short audio descriptor */
struct cea_sad {
	uint8_t	flags1;
	uint8_t sample_rates;
	uint8_t flags2;
} __packed;

#define	CEA_AUDIO_FORMAT(desc)	(((desc)->flags1 >> 3) & 0x0f)
#define	 CEA_AUDIO_FORMAT_LPCM		1
#define	 CEA_AUDIO_FORMAT_AC3		2
#define	 CEA_AUDIO_FORMAT_MPEG1_L12	3
#define	 CEA_AUDIO_FORMAT_MPEG1_L3	4
#define	 CEA_AUDIO_FORMAT_MPEG2		5
#define	 CEA_AUDIO_FORMAT_AAC		6
#define	 CEA_AUDIO_FORMAT_DTS		7
#define	 CEA_AUDIO_FORMAT_ATRAC		8
#define	CEA_MAX_CHANNELS(desc)	((((desc)->flags1 >> 0) & 0x07) + 1)
#define	CEA_SAMPLE_RATE(desc)	((desc)->sample_rates)
#define	 CEA_SAMPLE_RATE_192K		0x40
#define	 CEA_SAMPLE_RATE_176K		0x20
#define	 CEA_SAMPLE_RATE_96K		0x10
#define	 CEA_SAMPLE_RATE_88K		0x08
#define	 CEA_SAMPLE_RATE_48K		0x04
#define	 CEA_SAMPLE_RATE_44K		0x02
#define	 CEA_SAMPLE_RATE_32K		0x01
/* uncompressed */
#define	CEA_PRECISION(desc)	((desc)->flags2 & 0x07)
#define	 CEA_PRECISION_24BIT		0x4
#define	 CEA_PRECISION_20BIT		0x2
#define	 CEA_PRECISION_16BIT		0x1
/* compressed */
#define	CEA_MAX_BITRATE(desc)	((uint32_t)(desc)->flags2 * 8000)

#endif /* !_CEAREG_H */
