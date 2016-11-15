/*	$NetBSD: cs4215reg.h,v 1.4 2008/05/05 00:21:47 jmcneill Exp $	*/

/*
 * Copyright (c) 2001 Jared D. McNeill <jmcneill@NetBSD.org>
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

/* time slot 1: status register */
#define	CS4215_CLB	(1<<2)	/* control latch bit */
#define	CS4215_MLB	(1<<4)	/* 1: mic: 20 dB gain disabled */
#define	CS4215_RSRVD_1	(1<<5)

/* time slot 2: data format register */
#define	CS4215_DFR_LINEAR16	0
#define	CS4215_DFR_ULAW		1
#define	CS4215_DFR_ALAW		2
#define	CS4215_DFR_LINEAR8	3
#define	CS4215_DFR_STEREO	(1<<2)
const struct {
	unsigned short	freq;
	unsigned char	xtal;
	unsigned char	csval;
} CS4215_FREQ[] = {
	{ 8000, 	(1<<4), (0<<3) },
	{ 16000, 	(1<<4), (1<<3) },
	{ 27429, 	(1<<4), (2<<3) },	/* actually 24428.57 */
	{ 32000, 	(1<<4), (3<<3) },
		/* NA		(4<<3) */
		/* NA		(5<<3) */
	{ 48000, 	(1<<4), (6<<3) },
	{ 9600, 	(1<<4), (7<<3) },
	{ 5513, 	(2<<4), (0<<3) },	/* actually 5512.5 */
	{ 11025, 	(2<<4), (1<<3) },
	{ 18900, 	(2<<4), (2<<3) },
	{ 22050, 	(2<<4), (3<<3) },
	{ 37800, 	(2<<4), (4<<3) },
	{ 44100, 	(2<<4), (5<<3) },
	{ 33075, 	(2<<4), (6<<3) },
	{ 6615, 	(2<<4), (7<<3) },
	{ 0, 		0, 	0 }
};

/* time slot 3: serial port control register */
#define	CS4215_XCLK	(1<<1)		/* 0: enable serial output */
#define	CS4215_BSEL_128	(1<<2)		/* bitrate: 128 bits per frame */

/* time slot 5: output setting */
#define	CS4215_LO(v)	(v)	/* left output attenuation 0x3f: -94.5 dB */
#define	CS4215_LE	(1<<6)	/* line out enable */
#define	CS4215_HE	(1<<7)	/* headphone enable */

/* time slot 6: output setting */
#define CS4215_RO(v)	(v)	/* right output attenuation 0x3f: -94.5 dB */
#define	CS4215_SE	(1<<6)	/* speaker enable */
#define CS4215_ADI	(1<<7)	/* a/d data invalid: busy in calibration */

/* time slot 7: input setting */
#define	CS4215_LG(v)	(v)	/* left gain setting 0xf: 22.5 dB */
#define	CS4215_IS	(1<<4)	/* input select: 1 = mic, 0 = line */
#define	CS4215_PIO0	(1<<6)	/* parallel I/O 0 */
#define	CS4215_PIO1	(1<<7)	/* parallel I/O 1 */

/* time slot 8: input setting */
#define	CS4215_RG(v)	(v)	/* right gain setting 0xf: 22.5 dB */
#define	CS4215_MA(v)	((v)<<4)	/* monitor path attenuation 0xf: mute */

