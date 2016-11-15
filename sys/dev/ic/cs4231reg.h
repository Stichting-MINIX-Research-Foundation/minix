/* $NetBSD: cs4231reg.h,v 1.12 2008/04/28 20:23:49 martin Exp $ */

/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Ken Hornstein and John Kohl.
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

/*
 * Register defs for Crystal Semiconductor CS4231 Audio Codec/mixer
 * chip, used on Gravis UltraSound MAX cards.
 *
 * Block diagram:
 *             +----------------------------------------------------+
 *             |                                                    |
 *             |   +----------------------------------------------+ |
 *             |   |mixed in       +-+                            | |
 *             |   +------------>--| |                            | |
 *             | mic in            | |                            | |
 *   Mic --+-->| --------- GAIN ->-| |                            | |
 *         |   | AUX 1 in          |M|                            | |
 *   GF1 --)-->| -------------+-->-|U|                            | |
 *         |   | Line in      |    |X|---- GAIN ----------+       | |
 *  Line --)-->| ---------+---)-->-| |                    |       | |
 *         |   |          |   |    | |                    |       | |
 *         |   |          |   |    +-+                   ADC      | |
 *         |   |          |   |                           |       | |
 *         |   |          |   |                           |       | |
 *         |   |          |   +--- L/M --\                |       | | AMP-->
 *         |   |          |               \               |       | |  |
 *         |   |          |                \              |       | |  |
 *         |   |          +---- L/M -------O-->--+--------)-------+-|--+-> line
 *         |   |   mono in                /|     |        |         |
 *         +---|-->------------ L/M -----/ |     |        |         |
 *             |   AUX 2 in                |     |        |         |
 *  CD --------|-->------------ L/M -------+    L/M       |         |
 *             |                                 |        v         |
 *             |                                 |        |         |
 *             |                                DAC       |         |
 *             |                                 |        |         |
 *             +----------------------------------------------------+
 *                                               |        |
 *                                               |        |
 *                                               v        v
 *                                                Pc BUS (DISK) ???
 *
 */

/* CS4231/AD1845 mode2 registers; added to AD1848 registers */
#define CS_ALT_FEATURE1		16
#define CS_ALT_FEATURE2		17
#define CS_LEFT_LINE_CONTROL	18
#define CS_RIGHT_LINE_CONTROL	19
#define CS_TIMER_LOW		20
#define CS_TIMER_HIGH		21
#define CS_UPPER_FREQUENCY_SEL	22
#define CS_LOWER_FREQUENCY_SEL	23
#define CS_IRQ_STATUS		24
#define CS_VERSION_ID		25
#define CS_MONO_IO_CONTROL	26
#define CS_POWERDOWN_CONTROL	27
#define CS_LEFT_OUT		27 /* 4232 */
#define CS_REC_FORMAT		28
#define CS_RIGHT_OUT		29 /* 4232 */
#define CS_XTAL_SELECT		29
#define CS_UPPER_REC_CNT	30
#define CS_LOWER_REC_CNT	31

/* ALT_FEATURE1 - register I16 */
#define ALT_F1_DACZ		0x01	/* 1: hold sample during underrun */
#define ALT_F1_SPE		0x02	/* 1: Serial port enable */
#define ALT_F1_SFORMAT		0x06	/* Serial port format */
#define ALT_F1_PMCE		0x10	/* Playback mode change enable */
#define ALT_F1_CMCE		0x20	/* Capture mode change enable */
#define ALT_F1_TE		0x40	/* Timer enable */
#define ALT_F1_OLB		0x80	/* Output level bit */

/* ALT_FEATURE2 - register I17 */
#define ALT_F2_HPF		0x01	/* High pass filter */
#define ALT_F2_XTALE		0x02	/* Crytal enable */
#define ALT_F2_APAR		0x04	/* ADPCM playback accumulator reset */
#define ALT_F2_RES		0x08	/* reserved */
#define ALT_F2_TEST		0xf0	/* Factory test bits */

/* LINE_CONTROL (LEFT & RIGHT) - register I18,I19 */
#define LINE_INPUT_ATTEN_BITS	0x1f
#define LINE_INPUT_ATTEN_MASK	0xe0
#define LINE_INPUT_MUTE		0x80
#define LINE_INPUT_MUTE_MASK	(~LINE_INPUT_MUTE & 0xff)

/* ALT_FEATURE3 - register I23 */
#define ALT_F3_ACF		0x01	/* ADPCM capture freeze */

/* ALT_FEATURE_STATUS (aka CS_IRQ_STATUS) - register I24 */
#define CS_IRQ_PU		0x01	/* Playback Underrun */
#define CS_IRQ_PO		0x02	/* Playback Overrun */
#define CS_IRQ_CO		0x04	/* Capture Overrun */
#define CS_IRQ_CU		0x08	/* Capture Underrun */
#define CS_IRQ_PI		0x10	/* Playback Interrupt */
#define CS_IRQ_CI		0x20	/* Capture Interrupt */
#define CS_IRQ_TI		0x40	/* Timer Interrupt */
#define CS_IRQ_RES		0x80	/* reserved */

#define CS_I24_BITS		"\20\1PU\2PO\3CO\4CU\5PI\6CI\7TI"

/* VERSION - register I25 */
#define CS_VERSION_NUMBER	0xe0	/* Version number:
					 *	0x101 - 4231 rev. A
					 *	0x100 - 4231 previous revs
					 *	0x100 - 4232 (unreleased?)
					 *	0x101 - 4232 rev. C
					 */
#define CS_VERSION_CHIPID	0x07	/* Chip Identification.
					 * Currently know values:
					 *	0x000 - CS4231
					 *	0x010 - CS4232
					 */

/* MONO_IO_CONTROL - register I26 */
#define MONO_INPUT_ATTEN_BITS	0x0f
#define MONO_INPUT_ATTEN_MASK	0xf0
#define MONO_BYPASS		0x20
#define MONO_OUTPUT_MUTE	0x40
#define MONO_OUTPUT_MUTE_MASK	(~MONO_OUTPUT_MUTE & 0xff)
#define MONO_INPUT_MUTE		0x80
#define MONO_INPUT_MUTE_MASK	(~MONO_INPUT_MUTE & 0xff)

/* CS_LEFT_OUT - register I27 */
#define LEFT_OUT_ATTEN_BITS	0x0f
#define LEFT_OUT_ATTEN_MASK	0xf0
#define LEFT_OUT_MUTE		0x80
#define LEFT_OUT_MUTE_MASK	(~LEFT_OUT_MUTE & 0xff)

/* CS_REC_FORMAT - register I28 */
#define REC_FMT_reserved	0x0f	/* reserved */
#define REC_FMT_SM		0x10	/* 0: mono, 1: stereo */
#define REC_FMT_CL		0x20	/* 0: linear, 1: companded */
#define REC_FMT_FMT0		0x40	/* See register I8 for valid */
#define REC_FMT_FMT1		0x80	/* combination of the FMT and CL bits */

/* CS_RIGHT_OUT - register I27 */
#define RIGHT_OUT_ATTEN_BITS	0x0f
#define RIGHT_OUT_ATTEN_MASK	0xf0
#define RIGHT_OUT_MUTE		0x80
#define RIGHT_OUT_MUTE_MASK	(~RIGHT_OUT_MUTE & 0xff)
