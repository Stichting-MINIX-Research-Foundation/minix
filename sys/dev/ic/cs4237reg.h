/*	$NetBSD: cs4237reg.h,v 1.5 2008/04/28 20:23:49 martin Exp $ */

/*
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Rene Hexel (rh@NetBSD.org).
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

/* CS4237/4236B mode3 extended register, added to AD1848 registers */
#define CS_XREG			23	/* mode 3 extended register access */

/* ALT_FEATURE3 - register I23 */
#define ALT_F3_XA4		0x04	/* Extended Register Address bit 4 */
#define ALT_F3_XRAE		0x08	/* Extended Register Access Enable */
#define ALT_F3_XA0		0x10	/* Extended Register Address bit 0 */
#define ALT_F3_XA1		0x20	/* Extended Register Address bit 1 */
#define ALT_F3_XA2		0x40	/* Extended Register Address bit 2 */
#define ALT_F3_XA3		0x80	/* Extended Register Address bit 3 */

/* extended register set, accessed indirectly through I23 */
#define CS_X_LEFT_LINE_ALT_VOL	0x08	/* Left LINE Alternate Volume */
#define CS_X_RIGHT_LINE_ALT_VOL	0x18	/* Right LINE Alternate Volume */
#define CS_X_LEFT_MIC_VOL	0x28	/* Left Microphone Volume */
#define CS_X_RIGHT_MIC_VOL	0x38	/* Right Microphone Volume */
#define CS_X_SYNTHINPUT_CONTROL	0x48	/* Synthesis and Input Mixer Control */
#define CS_X_RIGHTINPUT_CONTROL	0x58	/* Right Input Mixer Control */
#define CS_X_LEFT_FM_SYNTH_VOL	0x68	/* Left FM Synthesis Volume */
#define CS_X_RIGHT_FM_SYNTH_VOL	0x78	/* Right FM Synthesis Volume */
#define CS_X_LEFT_DSP_SER_VOL	0x88	/* Left DSP Serial Port Volume */
#define CS_X_RIGHT_DSP_SER_VOL	0x98	/* Right DSP Serial Port Volume */
#define CS_X_RIGHT_LOOPBACK_VOL	0xa8	/* Right Loopback Monitor Volume */
#define CS_X_DAC_MUTE_IFSE_EN	0xb8	/* DAC Mute and IFSE Enable */
#define CS_X_INDEP_ADC_FREQ	0xc8	/* Independent ADC Sample Freq */
#define CS_X_INDEP_DAC_FREQ	0xd8	/* Independent DAC Sample Freq */
#define CS_X_LEFT_DIGITAL_VOL	0xe8	/* Left Master Digital Audio Volume */
#define CS_X_RIGHT_DIGITAL_VOL	0xf8	/* Right Master Digital Audio Volume */
#define CS_X_LEFT_WAVE_SER_VOL	0x0c	/* Left Wavetable Serial Port Volume */
#define CS_X_RIGHT_WAVE_SER_VOL	0x1c	/* Right Wavetable Serial Port Volume */
#define CS_X_CHIP_VERSION	0x9c	/* Chip Version and ID */

/* CS_X_CHIP_VERSION - register X25 */
#define X_CHIP_VERSIONF_CID	0x1f	/* Chip ID mask */
#define X_CHIP_VERSIONF_REV	0xe0	/* Chip Revision mask */

#define X_CHIP_CID_CS4236BB	0x00	/* CS4236B revision B */
#define X_CHIP_CID_CS4236B	0x0b	/* CS4236B other revision */
#define X_CHIP_CID_CS4237B	0x08	/* CS4237B */
