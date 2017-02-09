/* $NetBSD: sa2400reg.h,v 1.8 2009/10/19 23:19:39 rmind Exp $ */

/*
 * Copyright (c) 2005 David Young.  All rights reserved.
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

#ifndef _DEV_IC_SA2400REG_H_
#define	_DEV_IC_SA2400REG_H_

/*
 * Serial bus format for Philips SA2400 Single-chip Transceiver.
 */
#define SA2400_TWI_DATA_MASK	__BITS(31,8)
#define SA2400_TWI_WREN		__BIT(7)		/* enable write */
#define SA2400_TWI_ADDR_MASK	__BITS(6,0)

/*
 * Registers for Philips SA2400 Single-chip Transceiver.
 */
#define SA2400_SYNA		0		/* Synthesizer Register A */
#define SA2400_SYNA_FM		__BIT(21)	/* fractional modulus select,
						 * 0: /8 (default)
						 * 1: /5
						 */
#define	SA2400_SYNA_NF_MASK	__BITS(20,18)	/* fractional increment value,
						 * 0 to 7, default 4
						 */
#define	SA2400_SYNA_N_MASK	__BITS(17,2)	/* main divider division ratio,
						 * 512 to 65535, default 615
						 */

#define SA2400_SYNB		1		/* Synthesizer Register B */
#define SA2400_SYNB_R_MASK	__BITS(21,12)	/* reference divider ratio,
						 * 4 to 1023, default 11
						 */
#define SA2400_SYNB_L_MASK	__BITS(11,10)	/* lock detect mode */
#define SA2400_SYNB_L_INACTIVE0	__SHIFTIN(0, SA2400_SYNB_L_MASK)
#define SA2400_SYNB_L_INACTIVE1	__SHIFTIN(1, SA2400_SYNB_L_MASK)
#define SA2400_SYNB_L_NORMAL	__SHIFTIN(2, SA2400_SYNB_L_MASK)
#define SA2400_SYNB_L_INACTIVE2	__SHIFTIN(3, SA2400_SYNB_L_MASK)

#define	SA2400_SYNB_ON		__BIT(9)	/* power on/off,
						 * 0: inverted chip mode control
						 * 1: as defined by chip mode
						 *    (see SA2400_OPMODE)
						 */
#define	SA2400_SYNB_ONE		__BIT(8)	/* always 1 */
#define	SA2400_SYNB_FC_MASK	__BITS(7,0)	/* fractional compensation
						 * charge pump current DAC,
						 * 0 to 255, default 80.
						 */

#define SA2400_SYNC		2		/* Synthesizer Register C */
#define SA2400_SYNC_CP_MASK	__BITS(7,6)	/* charge pump current
						 * setting
						 */
#define SA2400_SYNC_CP_NORMAL_	__SHIFTIN(0, SA2400_SYNC_CP_MASK)
#define SA2400_SYNC_CP_THIRD_	__SHIFTIN(1, SA2400_SYNC_CP_MASK)
#define SA2400_SYNC_CP_NORMAL	__SHIFTIN(2, SA2400_SYNC_CP_MASK) /* recommended */
#define SA2400_SYNC_CP_THIRD	__SHIFTIN(3, SA2400_SYNC_CP_MASK)

#define SA2400_SYNC_SM_MASK	__BITS(5,3)	/* comparison divider select,
						 * 0 to 4, extra division
						 * ratio is 2**SM.
						 */
#define SA2400_SYNC_ZERO	__BIT(2)	/* always 0 */

#define SA2400_SYND		3		/* Synthesizer Register D */
#define SA2400_SYND_ZERO1_MASK	__BITS(21,17)	/* always 0 */
#define SA2400_SYND_TPHPSU	__BIT(16)	/* T[phpsu], 1: disable
						 * PHP speedup pump,
						 * overrides SA2400_SYND_TSPU
						 */
#define SA2400_SYND_TPSU	__BIT(15)	/* T[spu], 1: speedup on,
						 * 0: speedup off
						 */
#define SA2400_SYND_ZERO2_MASK	__BITS(14,3)	/* always 0 */

#define	SA2400_OPMODE		4		/* Operating mode, filter tuner,
						 * other controls
						 */
/* 1: in Rx mode, RSSI-ADC always on 0: RSSI-ADC only on during AGC */
#define SA2400_OPMODE_ADC	__BIT(19)
/* read-only filter tuner error: 1 if tuner out of range */
#define SA2400_OPMODE_FTERR	__BIT(18)
/* Rx & Tx filter tuning, write tuning value (test mode only) or
 * read tuner setting (in normal mode).
 */
#define SA2400_OPMODE_FILTTUNE_MASK	__BITS(17,15)

/* external reference voltage (pad v2p5) on */
#define SA2400_OPMODE_V2P5	__BIT(14)
/* external reference current ... */
#define SA2400_OPMODE_I1M	__BIT(13)
/* external reference current ... */
#define SA2400_OPMODE_I0P3	__BIT(12)
#define SA2400_OPMODE_IN22	__BIT(10)	/* xtal input frequency,
						 * 0: 44 MHz
						 * 1: 22 MHz
						 */
#define SA2400_OPMODE_CLK	__BIT(9)	/* reference clock output on */
#define SA2400_OPMODE_XO	__BIT(8)	/* xtal oscillator on */
#define SA2400_OPMODE_DIGIN	__BIT(7)	/* use digital Tx inputs
						 * (FIRDAC)
						 */
#define SA2400_OPMODE_RXLV	__BIT(6)	/* Rx output common mode
						 * voltage,
						 * 0: V[DD]/2
						 * 1: 1.25V
						 */
#define SA2400_OPMODE_VEO       __BIT(5)	/* make internal vco
						 * available at vco pads
						 * (vcoextout)
						 */
#define SA2400_OPMODE_VEI	__BIT(4)	/* use external vco input
						 * (vcoextin)
						 */
/* main operating mode */
#define SA2400_OPMODE_MODE_MASK		__BITS(3,0)
#define SA2400_OPMODE_MODE_SLEEP	__SHIFTIN(0, SA2400_OPMODE_MODE_MASK)
#define SA2400_OPMODE_MODE_TXRX		__SHIFTIN(1, SA2400_OPMODE_MODE_MASK)
#define SA2400_OPMODE_MODE_WAIT		__SHIFTIN(2, SA2400_OPMODE_MODE_MASK)
#define SA2400_OPMODE_MODE_RXMGC	__SHIFTIN(3, SA2400_OPMODE_MODE_MASK)
#define SA2400_OPMODE_MODE_FCALIB	__SHIFTIN(4, SA2400_OPMODE_MODE_MASK)
#define SA2400_OPMODE_MODE_DCALIB	__SHIFTIN(5, SA2400_OPMODE_MODE_MASK)
#define SA2400_OPMODE_MODE_FASTTXRXMGC	__SHIFTIN(6, SA2400_OPMODE_MODE_MASK)
#define SA2400_OPMODE_MODE_RESET	__SHIFTIN(7, SA2400_OPMODE_MODE_MASK)
#define SA2400_OPMODE_MODE_VCOCALIB	__SHIFTIN(8, SA2400_OPMODE_MODE_MASK)

#define	SA2400_OPMODE_DEFAULTS						\
	(SA2400_OPMODE_XO | SA2400_OPMODE_RXLV | SA2400_OPMODE_CLK |	\
	 SA2400_OPMODE_I0P3 | __SHIFTIN(3, SA2400_OPMODE_FILTTUNE_MASK))

#define	SA2400_AGC		5		/* AGC adjustment */
#define SA2400_AGC_TARGETSIGN	__BIT(23)	/* fine-tune AGC target:
						 * -7dB to 7dB, sign bit ... */
#define SA2400_AGC_TARGET_MASK	__BITS(22,20)	/* ... plus 0dB - 7dB */
#define SA2400_AGC_MAXGAIN_MASK	__BITS(19,15)	/* maximum AGC gain, 0 to 31,
						 * (yields 54dB to 85dB)
						 */
/* write: settling time after baseband gain switching, units of
 *        182 nanoseconds.
 * read:  output of RSSI/Tx-peak detector's ADC in 5-bit Gray code.
 */
#define SA2400_AGC_BBPDELAY_MASK	__BITS(14,10)
#define SA2400_AGC_ADCVAL_MASK		SA2400_AGC_BBPDELAY_MASK

/* write: settling time after LNA gain switching, units of
 *        182 nanoseconds
 * read:  2nd sample of RSSI in AGC cycle
 */
#define SA2400_AGC_LNADELAY_MASK	__BITS(9,5)
#define SA2400_AGC_SAMPLE2_MASK		SA2400_AGC_LNADELAY_MASK

/* write: time between turning on Rx and AGCSET, units of
 *        182 nanoseconds
 * read:  1st sample of RSSI in AGC cycle
 */
#define SA2400_AGC_RXONDELAY_MASK	__BITS(4,0)
#define SA2400_AGC_SAMPLE1_MASK		SA2400_AGC_RXONDELAY_MASK

#define SA2400_MANRX		6	/* Manual receiver control settings */
#define SA2400_MANRX_AHSN	__BIT(23)	/* 1: AGC w/ high S/N---switch
						 *    LNA at step 52
						 *    (recommended)
						 * 0: switch LNA at step 60
						 */

/* If _RXOSQON, Q offset is
 * (_RXOSQSIGN ? -1 : 1) * (1 + _RXOSQ_MASK) * 8 millivolts,
 * otherwise, Q offset is 0.
 *
 * Ditto I offset.
 */
#define SA2400_MANRX_RXOSQON	__BIT(22)	/* Rx Q-channel correction. */
#define SA2400_MANRX_RXOSQSIGN	__BIT(21)
#define SA2400_MANRX_RXOSQ_MASK	__BITS(20,18)

#define SA2400_MANRX_RXOSION	__BIT(17)	/* Rx I-channel correction. */
#define SA2400_MANRX_RXOSISIGN	__BIT(16)
#define SA2400_MANRX_RXOSI_MASK	__BITS(15,13)
#define SA2400_MANRX_TEN	__BIT(12)	/* use 10MHz offset cancellation
						 * cornerpoint for brief period
						 * after each gain change
						 */

/* DC offset cancellation cornerpoint select
 * write: in RXMGC, set the cornerpoint
 * read:  in other modes, read AGC-controlled cornerpoint
 */
#define SA2400_MANRX_CORNERFREQ_MASK	__BITS(11,10)

/* write: in RXMGC mode, sets receiver gain
 * read:  in other modes, read AGC-controlled gain
 */
#define SA2400_MANRX_RXGAIN_MASK	__BITS(9,0)

#define SA2400_TX	7		/* Transmitter settings */
/* Tx offsets
 *
 * write: in test mode, sets the offsets
 * read:  in normal mode, returns automatic settings
 */
#define SA2400_TX_TXOSQON	__BIT(19)
#define SA2400_TX_TXOSQSIGN	__BIT(18)
#define SA2400_TX_TXOSQ_MASK	__BITS(17,15)
#define SA2400_TX_TXOSION	__BIT(14)
#define SA2400_TX_TXOSISIGN	__BIT(13)
#define SA2400_TX_TXOSI_MASK	__BITS(12,10)

#define SA2400_TX_RAMP_MASK	__BITS(9,8)	/* Ramp-up delay,
						 * 0: 1us
						 * 1: 2us
						 * 2: 3us
						 * 3: 4us
						 * datasheet says, "ramp-up
						 * time always 1us". huh?
						 */
#define SA2400_TX_HIGAIN_MASK	__BITS(7,4)	/* Transmitter gain settings
						 * for TXHI output
						 */
#define SA2400_TX_LOGAIN_MASK	__BITS(3,0)	/* Transmitter gain settings
						 * for TXLO output
						 */

#define SA2400_VCO	8			/* VCO settings */
#define SA2400_VCO_ZERO		__BITS(6,5)	/* always zero */
#define SA2400_VCO_VCERR	__BIT(4)/* VCO calibration error flag---no
					 * band with low enough frequency
					 * could be found
					 */
#define SA2400_VCO_VCOBAND_MASK	__BITS(3,0)	/* VCO band,
						 * write: in test mode, sets
						 *        VCO band
						 * read:  in normal mode,
						 *        the result of
						 *        calibration (VCOCAL).
						 *        0 = highest
						 *        frequencies
						 */
#endif /* _DEV_IC_SA2400REG_H_ */
