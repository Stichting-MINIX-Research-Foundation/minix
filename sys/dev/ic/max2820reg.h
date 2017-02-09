/* $NetBSD: max2820reg.h,v 1.8 2009/10/19 23:19:39 rmind Exp $ */

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

#ifndef _DEV_IC_MAX2820REG_H_
#define	_DEV_IC_MAX2820REG_H_

/*
 * Serial bus format for Maxim MAX2820/MAX2820A/MAX2821/MAX2821A
 * 2.4GHz 802.11b Zero-IF Transceivers
 */
#define MAX2820_TWI_ADDR_MASK	__BITS(15,12)
#define MAX2820_TWI_DATA_MASK	__BITS(11,0)

/*
 * Registers for Maxim MAX2820/MAX2820A/MAX2821/MAX2821A 2.4GHz
 * 802.11b Zero-IF Transceivers
 */
#define MAX2820_TEST		0		/* Test Register */
#define MAX2820_TEST_DEFAULT	__BITS(2,0)	/* Always set to this value. */

#define MAX2820_ENABLE		1		/* Block-Enable Register */
#define	MAX2820_ENABLE_RSVD1	__BIT(11)	/* reserved */
#define	MAX2820_ENABLE_PAB	__BIT(10)	/* Transmit Baseband Filters
						 * Enable
						 * PAB_EN = SHDNB &&
						 *   (MAX2820_ENABLE_PAB ||
						 *    TX_ON)
						 */
#define	MAX2820_ENABLE_TXFLT	__BIT(9)	/* Transmit Baseband Filters
						 * Enable
						 * TXFLT_EN = SHDNB &&
						 *   (MAX2820_ENABLE_TXFLT ||
						 *    TX_ON)
						 */
#define	MAX2820_ENABLE_TXUVD	__BIT(8)	/* Tx Upconverter, VGA, and
						 * Driver Amp Enable
						 * TXUVD_EN = SHDNB &&
						 *   (MAX2820_ENABLE_TXUVD ||
						 *    TX_ON)
						 */
#define	MAX2820_ENABLE_DET	__BIT(7)	/* Receive Detector Enable
						 * DET_EN = SHDNB &&
						 *   (MAX2820_ENABLE_DET ||
						 *    RX_ON)
						 */
#define	MAX2820_ENABLE_RXDFA	__BIT(6)	/* Rx Downconverter, Filters,
						 * and AGC Amps Enable
						 * RXDFA_EN = SHDNB &&
						 *   (MAX2820_ENABLE_RXDFA ||
						 *    RX_ON)
						 */
#define	MAX2820_ENABLE_RXLNA	__BIT(5)	/* Receive LNA Enable
						 * AT_EN = SHDNB &&
						 *   (MAX2820_ENABLE_RXLNA ||
						 *    RX_ON)
						 */
#define	MAX2820_ENABLE_AT	__BIT(4)	/* Auto-tuner Enable
						 * AT_EN = SHDNB &&
						 *   (MAX2820_ENABLE_AT ||
						 *    RX_ON || TX_ON)
						 */
#define	MAX2820_ENABLE_CP	__BIT(3)	/* PLL Charge-Pump Enable
						 * CP_EN = SHDNB
						 *   && MAX2820_ENABLE_CP
						 */
#define	MAX2820_ENABLE_PLL	__BIT(2)	/* PLL Enable
						 * PLL_EN = SHDNB
						 *   && MAX2820_ENABLE_PLL
						 */
#define	MAX2820_ENABLE_VCO	__BIT(1)	/* VCO Enable
						 * VCO_EN = SHDNB
						 *   && MAX2820_ENABLE_VCO
						 */
#define	MAX2820_ENABLE_RSVD0	__BIT(0)	/* reserved */
#define	MAX2820_ENABLE_DEFAULT	(MAX2820_ENABLE_AT|MAX2820_ENABLE_CP|\
				 MAX2820_ENABLE_PLL|MAX2820_ENABLE_VCO)

#define MAX2820_SYNTH		2		/* Synthesizer Register */
#define	MAX2820_SYNTH_RSVD0	__BITS(11,7)	/* reserved */
#define	MAX2820_SYNTH_ICP	__BIT(6)	/* Charge-Pump Current Select
						 * 0 = +/-1mA
						 * 1 = +/-2mA
						 */
#define	MAX2820_SYNTH_R_MASK	__BITS(5,0)	/* Reference Frequency Divider
						 * 0 = 22MHz
						 * 1 = 44MHz
						 */
#define	MAX2820_SYNTH_R_22MHZ	__SHIFTIN(0, MAX2820_SYNTH_R_MASK)
#define	MAX2820_SYNTH_R_44MHZ	__SHIFTIN(1, MAX2820_SYNTH_R_MASK)
#define	MAX2820_SYNTH_ICP_DEFAULT	MAX2820_SYNTH_ICP
#define	MAX2820_SYNTH_R_DEFAULT		__SHIFTIN(0, MAX2820_SYNTH_R_MASK)

#define MAX2820_CHANNEL		3		/* Channel Frequency Register */
#define	MAX2820_CHANNEL_RSVD	__BITS(11,7)	/* reserved */
#define	MAX2820_CHANNEL_CF_MASK	__BITS(6,0)	/* Channel Frequency Select
						 * fLO = 2400MHz + CF * 1MHz
						 */
#define	MAX2820_CHANNEL_RSVD_DEFAULT	__SHIFTIN(0, MAX2820_CHANNEL_RSVD)
#define	MAX2820_CHANNEL_CF_DEFAULT	__SHIFTIN(37, MAX2820_CHANNEL_CF_MASK)

#define MAX2820_RECEIVE		4		/* Receiver Settings Register
						 * MAX2820/MAX2821
						 */
#define	MAX2820_RECEIVE_2C_MASK	__BITS(11,9)	/* VGA DC Offset Nulling
						 * Parameter 2
						 */
#define	MAX2820_RECEIVE_1C_MASK	__BITS(8,6)	/* VGA DC Offset Nulling
						 * Parameter 1
						 */
#define	MAX2820_RECEIVE_DL_MASK	__BITS(5,4)	/* Rx Level Detector Midpoint
						 * Select
						 * 11, 01 = 50.2mVp
						 * 10     = 70.9mVp
						 * 00     = 35.5mVp
						 */
#define	MAX2820_RECEIVE_SF	__BIT(3)	/* Special Function Select
						 * 0 = OFF
						 * 1 = ON
						 */
#define	MAX2820_RECEIVE_BW_MASK	__BITS(2,0)	/* Receive Filter -3dB Frequency
						 * Select (all frequencies are
						 * approximate)
						 */
/* 8.5MHz */
#define	MAX2820_RECEIVE_BW_8_5MHZ	__SHIFTIN(0, MAX2820_RECEIVE_BW_MASK)
#define	MAX2820_RECEIVE_BW_8MHZ		__SHIFTIN(1, MAX2820_RECEIVE_BW_MASK)
#define	MAX2820_RECEIVE_BW_7_5MHZ	__SHIFTIN(2, MAX2820_RECEIVE_BW_MASK)
#define	MAX2820_RECEIVE_BW_7MHZ		__SHIFTIN(3, MAX2820_RECEIVE_BW_MASK)
#define	MAX2820_RECEIVE_BW_6_5MHZ	__SHIFTIN(4, MAX2820_RECEIVE_BW_MASK)
#define	MAX2820_RECEIVE_BW_6MHZ		__SHIFTIN(5, MAX2820_RECEIVE_BW_MASK)
#define	MAX2820_RECEIVE_2C_DEFAULT	__SHIFTIN(7, MAX2820_RECEIVE_2C_MASK)
#define	MAX2820_RECEIVE_1C_DEFAULT	__SHIFTIN(7, MAX2820_RECEIVE_1C_MASK)
#define	MAX2820_RECEIVE_DL_DEFAULT	__SHIFTIN(1, MAX2820_RECEIVE_DL_MASK)
#define	MAX2820_RECEIVE_SF_DEFAULT	__SHIFTIN(0, MAX2820_RECEIVE_SF)
#define	MAX2820_RECEIVE_BW_DEFAULT	MAX2820_RECEIVE_BW_7_5MHZ

#define MAX2820A_RECEIVE	4		/* Receiver Settings Register,
						 * MAX2820A/MAX2821A
						 */
/* VGA DC Offset Nulling Parameter 2 */
#define	MAX2820A_RECEIVE_2C_MASK	__BITS(11,9)
#define	MAX2820A_RECEIVE_2C_DEFAULT	__SHIFTIN(7, MAX2820A_RECEIVE_2C_MASK)
/* VGA DC Offset Nulling Parameter 1 */
#define	MAX2820A_RECEIVE_1C_MASK	__BITS(8,6)
#define	MAX2820A_RECEIVE_1C_DEFAULT	__SHIFTIN(7, MAX2820A_RECEIVE_1C_MASK)
#define	MAX2820A_RECEIVE_RSVD0_MASK	__BITS(5,3)
#define	MAX2820A_RECEIVE_RSVD0_DEFAULT	__SHIFTIN(2, MAX2820A_RECEIVE_RSVD0_MASK)
#define	MAX2820A_RECEIVE_RSVD1_MASK	__BITS(2,0)
#define	MAX2820A_RECEIVE_RSVD1_DEFAULT	__SHIFTIN(2,MAX2820_RECEIVE_RSVD1_MASK)

#define MAX2820_TRANSMIT	5		/* Transmitter Settings Reg. */
#define MAX2820_TRANSMIT_RSVD_MASK	__BITS(11,4)	/* reserved */
#define MAX2820_TRANSMIT_PA_MASK	__BITS(3,0)	/* PA Bias Select
							 * 15 = Highest
							 * 0 = Lowest
							 */
#define MAX2820_TRANSMIT_PA_DEFAULT	__SHIFTIN(0, MAX2820_TRANSMIT_PA_MASK)

#endif /* _DEV_IC_MAX2820REG_H_ */
