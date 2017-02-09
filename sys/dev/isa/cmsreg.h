/* $NetBSD: cmsreg.h,v 1.4 2008/04/28 20:23:52 martin Exp $ */

/*
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
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

#define CMS_NVOICES 12
#define CMS_FIRST_NOTE 30

/* direct registers */

#define CMS_DATA0 0x00 /* for chip 0, voices 0-5 */
#define CMS_ADDR0 0x01

#define CMS_DATA1 0x02 /* for chip 1, voices 6-11 */
#define CMS_ADDR1 0x03

#define CMS_MREG  0x04 /* always returns 0x7f */
#define CMS_WREG  0x07 /* writable register */
#define CMS_RREG  0x0b /* readable register */

#define CMS_IOSIZE 16

/*
 * Note that for each register, if ports CMS_DATA1/CMS_ADDR1 are used
 * then the first voice is modified.  If ports CMS_DATA2/CMS_ADDR2 are
 * used then the second voice is modified.
 */

/*
 * Each voice can have a volume between 0 and 15 on both left and
 * right channels.  The high-order nibble is the right channel volume,
 * and the low-order nibble is the left channel volume.
 */

#define CMS_IREG_VOL0 0x00
#define CMS_IREG_VOL1 0x01
#define CMS_IREG_VOL2 0x02
#define CMS_IREG_VOL3 0x03
#define CMS_IREG_VOL4 0x04
#define CMS_IREG_VOL5 0x05

/* Frequency registers */
#define CMS_IREG_FREQ0 0x08
#define CMS_IREG_FREQ1 0x09
#define CMS_IREG_FREQ2 0x0a
#define CMS_IREG_FREQ3 0x0b
#define CMS_IREG_FREQ4 0x0c
#define CMS_IREG_FREQ5 0x0d

/*
 * Octave Registers: To get tones in higher octaves the octave
 * register for the voice must be set.  Each octave register stores
 * the octave number for two voices.  The high-order nibble is for
 * first voice and the low-order nibble is for the second voice.
 */

#define CMS_IREG_OCTAVE_1_0 0x10
#define CMS_IREG_OCTAVE_3_2 0x11
#define CMS_IREG_OCTAVE_5_4 0x12

#define CMS_IREG_FREQ_CTL 0x14 /* voice frequencies */
#define CMS_IREG_FREQ_ENBL0 0x01 /* setting the bit enables the voice */
#define CMS_IREG_FREQ_ENBL1 0x02 /* clearing the bit disables the voice */
#define CMS_IREG_FREQ_ENBL2 0x04
#define CMS_IREG_FREQ_ENBL3 0x08
#define CMS_IREG_FREQ_ENBL4 0x10
#define CMS_IREG_FREQ_ENBL5 0x20

/*
 * There are 4 noise generators, each noise generator can be connected
 * up to any of three voices:
 *
 * Noise generator 0: connected to voices 0,1,2
 *                 1: connected to voices 3,4,5
 *                 2: connected to voices 6,7,8
 *                 3: connected to voices 0,10,11
 *
 * CMS_DATA1/CMS_ADDR1 access noise generators 0 and 1.  Each noise
 * generator has two bits which control the noise generator rate.
 */

#define CMS_IREG_NOISE_CTL 0x15 /* noises */
#define CMS_IREG_NOISE_ENBL0 0x01
#define CMS_IREG_NOISE_ENBL1 0x02
#define CMS_IREG_NOISE_ENBL2 0x04
#define CMS_IREG_NOISE_ENBL3 0x08
#define CMS_IREG_NOISE_ENBL4 0x10
#define CMS_IREG_NOISE_ENBL5 0x20

#define CMS_IREG_NOISE_BW 0x16
#define CMS_IREG_NOISE_MASK0 0x03 /* bits for noise generator 0 */
#define CMS_IREG_NOISE_MASK1 0x30 /* bits for noise generator 1 */
/* the bits in the mask have the following meaning */
#define CMS_IREG_NOISE_MASK_28k 0 /* 28kHz */
#define CMS_IREG_NOISE_MASK_14k 1 /* 14kHz */
#define CMS_IREG_NOISE_MASK_7k  2 /* 6.8kHz */

#define CMS_IREG_SYS_CTL 0x1c
#define CMS_IREG_SYS_ENBL  0x01 /* enable all channels */
#define CMS_IREG_SYS_RESET 0x02 /* reset and synchronise generators */


/*
 * Some useful macros
 */

#define CMS_WRITE(sc, chip, reg, val)					\
do {									\
	(sc)->sc_shadowregs[((chip)<<5) + (reg)] = val;			\
	bus_space_write_1((sc)->sc_iot, (sc)->sc_ioh,			\
		CMS_ADDR0 + ((chip)<<1), (reg));			\
	bus_space_write_1((sc)->sc_iot, (sc)->sc_ioh,			\
		CMS_DATA0 + ((chip)<<1), (val));			\
} while (0)

#define CMS_READ(sc, chip, reg) ((sc)->sc_shadowregs[((chip)<<5) + (reg)])

#define CHAN_TO_CHIP(chan) ((chan)>5)
#define CHAN_TO_VOICE(chan) ((chan)%6)
#define OCTAVE_OFFSET(voice) ((voice)>>1)
#define OCTAVE_SHIFT(voice) (((voice)&1)<<2)
