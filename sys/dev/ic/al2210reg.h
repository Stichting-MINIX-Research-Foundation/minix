/* $NetBSD: al2210reg.h,v 1.5 2009/10/19 23:19:39 rmind Exp $ */

/*
 * Copyright (c) 2004 David Young.  All rights reserved.
 *
 * This code was written by David Young.
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
 * THIS SOFTWARE IS PROVIDED BY David Young ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL David
 * Young BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 */

#ifndef _DEV_IC_AL2210REG_H_
#define	_DEV_IC_AL2210REG_H_

/*
 * Register definitions for the Airoha AL2210 2.4GHz 802.11b
 * transceiver.
 */

/* NOTE WELL: These register definitions, in spite of being derived
 * from an "official" Airoha AL2210 datasheet, contain a lot of
 * "magic." Comparing with the magic in this header file with a
 * reference driver that also contains AL2210 magic, the magic does
 * not match!
 */

/*
 * Serial bus format for Airoha AL2210 2.4GHz transceiver.
 */
#define	AL2210_TWI_DATA_MASK	__BITS(23, 4)
#define	AL2210_TWI_ADDR_MASK	__BITS(3, 0)

/*
 * Registers for Airoha AL2210.
 */

/* The synthesizer magic should be decipherable, but I'm not going
 * to waste my time right now.
 */
#define AL2210_CHANNEL		0x0
#define		AL2210_CHANNEL_B_MASK		__BITS(10, 5)	/* Counter B */
#define		AL2210_CHANNEL_B_2412MHZ	0x396
#define		AL2210_CHANNEL_B_2417MHZ	0x396
#define		AL2210_CHANNEL_B_2422MHZ	0x396
#define		AL2210_CHANNEL_B_2427MHZ	0x396
#define		AL2210_CHANNEL_B_2432MHZ	0x398
#define		AL2210_CHANNEL_B_2437MHZ	0x398
#define		AL2210_CHANNEL_B_2442MHZ	0x398
#define		AL2210_CHANNEL_B_2447MHZ	0x398
#define		AL2210_CHANNEL_B_2452MHZ	0x398
#define		AL2210_CHANNEL_B_2457MHZ	0x398
#define		AL2210_CHANNEL_B_2462MHZ	0x398
#define		AL2210_CHANNEL_B_2467MHZ	0x39a
#define		AL2210_CHANNEL_B_2472MHZ	0x39a
#define		AL2210_CHANNEL_B_2484MHZ	0x39b
#define		AL2210_CHANNEL_A_MASK		__BITS(4, 0)	/* Counter A */
#define		AL2210_CHANNEL_A_2412MHZ	0x0c
#define		AL2210_CHANNEL_A_2417MHZ	0x11
#define		AL2210_CHANNEL_A_2422MHZ	0x16
#define		AL2210_CHANNEL_A_2427MHZ	0x1b
#define		AL2210_CHANNEL_A_2432MHZ	0x00
#define		AL2210_CHANNEL_A_2437MHZ	0x05
#define		AL2210_CHANNEL_A_2442MHZ	0x0a
#define		AL2210_CHANNEL_A_2447MHZ	0x0f
#define		AL2210_CHANNEL_A_2452MHZ	0x14
#define		AL2210_CHANNEL_A_2457MHZ	0x10
#define		AL2210_CHANNEL_A_2462MHZ	0x1e
#define		AL2210_CHANNEL_A_2467MHZ	0x03
#define		AL2210_CHANNEL_A_2472MHZ	0x08
#define		AL2210_CHANNEL_A_2484MHZ	0x14

#define AL2210_SYNTHESIZER	0x1
#define		AL2210_SYNTHESIZER_R_MASK	__BITS(4, 0)	/* Reference
								 * divider
								 */
#define AL2210_RECEIVER		0x2
/* Rx VAGC Detector Negative Edge Threshold */
#define		AL2210_RECEIVER_AGCDET_P_MASK	__BITS(16, 15)
#define		AL2210_RECEIVER_AGCDET_P_0_4V	0	/* 0.4V */
#define		AL2210_RECEIVER_AGCDET_P_0_3V	1	/* 0.3V */
#define		AL2210_RECEIVER_AGCDET_P_0_2V	2	/* 0.2V */
#define		AL2210_RECEIVER_AGCDET_P_RSVD	3	/* reserved */
/* Rx VAGC Detector Negative Edge Threshold */
#define		AL2210_RECEIVER_AGCDET_N_MASK	__BITS(14, 13)
#define		AL2210_RECEIVER_AGCDET_N_0_4V	0	/* 0.4V */
#define		AL2210_RECEIVER_AGCDET_N_0_3V	1	/* 0.3V */
#define		AL2210_RECEIVER_AGCDET_N_0_2V	2	/* 0.2V */
#define		AL2210_RECEIVER_AGCDET_N_RSVD	3	/* reserved */
/* AGC detector control, 1: enable, 0: disable. */
#define		AL2210_RECEIVER_AGCDETENA	__BIT(11)
/* Rx filter bandwidth select */
#define		AL2210_RECEIVER_BW_SEL_MASK	__BITS(4, 2)
#define		AL2210_RECEIVER_BW_SEL_9_5MHZ	0
#define		AL2210_RECEIVER_BW_SEL_9MHZ	1
#define		AL2210_RECEIVER_BW_SEL_8_5MHZ	2
#define		AL2210_RECEIVER_BW_SEL_8MHZ	3
#define		AL2210_RECEIVER_BW_SEL_7_5MHZ	4
#define		AL2210_RECEIVER_BW_SEL_7MHZ	5
#define		AL2210_RECEIVER_BW_SEL_6_5MHZ	6
#define		AL2210_RECEIVER_BW_SEL_6MHZ	7

#define AL2210_TRANSMITTER	0x3
/* 2nd-stage power amplifier current control.  Units of 20uA.
 * "Full scale" current is 300uA.  (Is full-scale at PABIAS2 = 0 or
 * at PABIAS2 = 15?)
 */
#define		AL2210_TRANSMITTER_PABIAS2_MASK	__BITS(7, 4)
/* 1st-stage power amplifier current control.  Units of 20uA.
 * "Full scale" current is 300uA.  (Is full-scale at PABIAS2 = 0 or
 * at PABIAS2 = 15?)
 */
#define		AL2210_TRANSMITTER_PABIAS1_MASK	__BITS(3, 0)

#define AL2210_CONFIG1		0x4

#define AL2210_CONFIG2		0x5
/* Regulator power.  0: on, 1: off. */
#define		AL2210_CONFIG2_REGPD_MASK	__BIT(19)
/* XO clock setting.   0: 44MHz, 1: 22MHz. */
#define		AL2210_CONFIG2_XTAL_SC_MASK	__BIT(10)

/* DC Offset Calibration (DCOC) */
#define AL2210_CONFIG3		0x6
/* Select 1MHz DCOC timing. */
#define		AL2210_CONFIG3_AGC_DET_PATT_1MHZ	__BIT(17)
/* Select 100kHz DCOC timing. */
#define		AL2210_CONFIG3_AGC_DET_PATT_100KHZ	__BIT(16)
#define		AL2210_CONFIG3_LNA_GAIN_PATT_1MHZ	__BITS(15)
#define		AL2210_CONFIG3_LNA_GAIN_PATT_100KHZ	__BITS(14)
#define		AL2210_CONFIG3_RXON_PATT_1MHZ		__BITS(13)
#define		AL2210_CONFIG3_RXON_PATT_1OOKHZ		__BITS(12)
/* 1MHz DCOC duration?  Microseconds. */
#define		AL2210_CONFIG3_CNT_1M_AGC_MASK		__BITS(11, 8)
#define		AL2210_CONFIG3_CNT_1M_LNA_MASK		__BITS(7, 4)
#define		AL2210_CONFIG3_CNT_1M_RXON_MASK		__BITS(3, 0)

#define AL2210_CONFIG4		0x7
/* 100kHz DCOC duration?  Microseconds. */
#define		AL2210_CONFIG4_CNT_100K_AGC_MASK	__BITS(11, 8)
#define		AL2210_CONFIG4_CNT_100K_LNA_MASK	__BITS(7, 4)
#define		AL2210_CONFIG4_CNT_100K_RXON_MASK	__BITS(3, 0)

#define AL2210_CONFIG5		0x8
#define		AL2210_CONFIG5_TXF_BW_MASK		__BITS(9, 8)
#define		AL2210_CONFIG5_TXF_BW_12MHZ		3
#define		AL2210_CONFIG5_TXF_BW_11MHZ		2
#define		AL2210_CONFIG5_TXF_BW_10MHZ		1
#define		AL2210_CONFIG5_TXF_BW_9MHZ		0

#define AL2210_CONFIG6		0x9
#define	AL2210_CONFIG6_DEFAULT	0x2c0009	/* magic */

#define AL2210_CONFIG7		0xa
#define	AL2210_CONFIG7_DEFAULT	0x001c0a	/* magic */

#define AL2210_CONFIG8		0xb
#define	AL2210_CONFIG8_DEFAULT	0x01000b	/* magic */

#endif /* _DEV_IC_AL2210REG_H_ */
