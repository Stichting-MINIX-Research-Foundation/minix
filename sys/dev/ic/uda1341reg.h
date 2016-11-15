/*-
 * Copyright (c) 2012 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Paul Fleischer <paul@xpg.dk>
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
#ifndef _DEV_IC_UDA1341REG_H_
#define _DEV_IC_UDA1341REG_H_

#define UDA1341_L3_ADDR_DEVICE	0x14 /* Address of the UDA1341 on the L3 bus */
#define UDA1341_L3_ADDR_DATA0	0x00
#define UDA1341_L3_ADDR_DATA1	0x01
#define UDA1341_L3_ADDR_STATUS	0x02

/* Status address has two "banks", 0 and 1.
   The bank is selected as bit 7 of the data written.
 */
#define UDA1341_L3_STATUS0	(0<<7)
#define UDA1341_L3_STATUS1	(1<<7)

/** Status bank 0 **/
#define UDA1341_L3_STATUS0_RST		(1<<6)

/* System clock selection (bit 4 and 5) */
#define UDA1341_L3_STATUS0_SC_512	(0<<4)
#define	UDA1341_L3_STATUS0_SC_384	(1<<4)
#define	UDA1341_L3_STATUS0_SC_256	(2<<4)
#define UDA1341_L3_STATUS0_SC_NA	(3<<4)
#define UDA1341_L3_STATUS0_SC_SHIFT	4

/* Interface format (bit 1, 2, 3)*/
#define UDA1341_L3_STATUS0_IF_I2S	(0<<1)
#define UDA1341_L3_STATUS0_IF_LSB16	(1<<1)
#define UDA1341_L3_STATUS0_IF_LSB18	(2<<1)
#define UDA1341_L3_STATUS0_IF_LSB20	(3<<1)
#define UDA1341_L3_STATUS0_IF_MSB	(4<<1)
#define UDA1341_L3_STATUS0_IF_LSB16_MSB	(5<<1)
#define UDA1341_L3_STATUS0_IF_LSB18_MSB	(6<<1)
#define UDA1341_L3_STATUS0_IF_LSB20_MSB (7<<1)
#define UDA1341_L3_STATUS0_IF_SHIFT	1

/* DC-Filtering */
#define UDA1341_L3_STATUS0_DC_FILTERING (1<<0)

/** Status bank 1**/

/* Output and Input Gain*/
#define UDA1341_L3_STATUS1_OGS_6DB	(1<<6)
#define UDA1341_L3_STATUS1_IGS_6DB	(1<<5)

/* DAC and ADC polarity inversion */
#define UDA1341_L3_STATUS1_PAD_INV	(1<<4)
#define UDA1341_L3_STATUS1_PDA_INV	(1<<3)

/* Double speed playback */
#define UDA1341_L3_STATUS1_DS		(1<<2)

/* Power Control */
#define UDA1341_L3_STATUS1_PC_ADC	(1<<1)
#define UDA1341_L3_STATUS1_PC_DAC	(1<<0)

/*** DATA0 ***/
/*
 * Data0 has five banks: three for direct control, and two for extended access.
 */
#define UDA1341_L3_DATA0_VOLUME		(0<<6)
#define UDA1341_L3_DATA0_VOLUME_MASK	(0x3F)

#define UDA1341_L3_DATA0_BASS_TREBLE	(1<<6)
#define UDA1341_L3_DATA0_BASS_SHIFT	2
#define UDA1341_L3_DATA0_BASS_MASK	0x3C
#define UDA1341_L3_DATA0_TREBLE_SHIFT	0
#define UDA1341_L3_DATA0_TREBLE_MASK	0x03

#define UDA1341_L3_DATA0_SOUNDC		(2<<6)
#define UDA1341_L3_DATA0_SOUNDC_DE_MASK (0x18)
#define UDA1341_L3_DATA0_SOUNDC_DE_SHIFT 3
#define UDA1341_L3_DATA0_SOUNDC_MUTE	(1<<2)
#define UDA1341_L3_DATA0_SOUNDC_MODE_MASK (0x03)

#define UDA1341_L3_DATA0_EA		((3<<6)|0<<5)
#define UDA1341_L3_DATA0_ED		((3<<6)|1<<5)

#define UDA1341_L3_DATA0_MA_MASK	(0x1F)
#define UDA1341_L3_DATA0_MB_MASK	(0x1F)

#define UDA1341_L3_DATA0_MS_MASK	(0x1C)
#define UDA1341_L3_DATA0_MS_SHIFT	2

#define UDA1341_L3_DATA0_MM_MASK	(0x03)

#define UDA1341_L3_DATA0_AGC_SHIFT	4
#define UDA1341_L3_DATA0_AGC_MASK	(0x10)

#define UDA1341_L3_DATA0_IG_LOW_MASK	(0x03)
#define UDA1341_L3_DATA0_IG_HIGH_MASK	(0x1F)

#define UDA1341_L3_DATA0_AL_MASK	(0x03)
#endif
