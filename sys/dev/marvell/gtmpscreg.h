/*	$NetBSD: gtmpscreg.h,v 1.4 2010/04/28 13:51:56 kiyohara Exp $	*/

/*
 * Copyright (c) 2002 Allegro Networks, Inc., Wasabi Systems, Inc.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed for the NetBSD Project by
 *      Allegro Networks, Inc., and Wasabi Systems, Inc.
 * 4. The name of Allegro Networks, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 * 5. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ALLEGRO NETWORKS, INC. AND
 * WASABI SYSTEMS, INC. ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL EITHER ALLEGRO NETWORKS, INC. OR WASABI SYSTEMS, INC.
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * gtmpscreg.h - register defines for GT-64260 MPSC
 *
 * creation	Sun Apr  8 11:49:57 PDT 2001	cliff
 */

#ifndef _GTMPSCREG_H
#define _GTMPSCREG_H

#ifndef BIT
#define BIT(bitno)	(1U << (bitno))
#endif
#ifndef BITS
#define BITS(hi, lo)	((~((~0) << ((hi) + 1))) & ((~0) << (lo)))
#endif

#define GTMPSC_BASE(u)	(MPSC0_BASE + ((u) << 12))
#define GTMPSC_SIZE	0x1000

#define GTMPSC_NCHAN	2		/* Number of MPSC channels */

/*******************************************************************************
 *
 * MPSC register address offsets relative to the base mapping
 */
#define GTMPSC_MMCR_LO	0x000		/* MPSC Main Config Register Low */
#define GTMPSC_MMCR_HI	0x004		/* MPSC Main Config Register High */
#define GTMPSC_MPCR	0x008		/* MPSC Protocol Config Register */
#define GTMPSC_CHR_BASE	0x008		/* MPSC Channel Register Base */
#define GTMPSC_CHRN(n)	(GTMPSC_CHR_BASE + ((n) << 2))
#define GTMPSC_NCHR	11		/* CHR 1-11? */

#define GTMPSC_MRR	0xb400		/* MPSC Routing Register */
#define GTMPSC_RCRR	0xb404		/* MPSC RX Clock Routing Register */
#define GTMPSC_TCRR	0xb408		/* MPSC TX Clock Routing Register */


/*******************************************************************************
 *
 * MPSC register values & bit defines
 *
 *	values are provided for UART mode only
 */
/*
 * MPSC Routing Register bits
 */
#define GTMPSC_MRR_PORT0	0		/* serial port #0 */
#define GTMPSC_MRR_NONE		7		/* unconnected */
						/* all other "routes" resvd. */
#define GTMPSC_MRR_MR0_MASK	BITS(2,0)	/* routing mask for MPSC0 */
#define GTMPSC_MRR_RESa		BITS(5,3)
#define GTMPSC_MRR_MR1_MASK	BITS(8,6)	/* routing mask for MPSC1 */
#define GTMPSC_MRR_RESb		BITS(30,9)
#define GTMPSC_MRRE_DSC		BIT(31)		/* "Don't Stop Clock" */
#define GTMPSC_MRR_RES (GTMPSC_MRR_RESa|GTMPSC_MRR_RESb)
/*
 * MPSC Clock Routing Register bits
 * the bitfields and route definitions are common for RCRR and TCRR
 * except for MPSC_TCRR_TSCLK0
 */
#define GTMPSC_CRR_BRG0		0x0		/* Baud Rate Generator #0 */
#define GTMPSC_CRR_BRG1		0x1		/* Baud Rate Generator #1 */
#define GTMPSC_CRR_BRG2		0x2		/* Baud Rate Generator #2 */
#define GTMPSC_CRR_SCLK0	0x8		/* SCLK0 */
#define GTMPSC_TCRR_TSCLK0	0x9		/* TSCLK0 (for TCRR only) */
						/* all other values resvd. */
#define GTMPSC_CRR(u, v)	((v) << GTMPSC_CRR_SHIFT(u))
#define GTMPSC_CRR_SHIFT(u)	((u) * 8)
#define GTMPSC_CRR_MASK		0xf
#define GTMPSC_CRR_RESa		BITS(7,4)
#define GTMPSC_CRR_RESb		BITS(31,12)
#define GTMPSC_CRR_RES (GTMPSC_CRR_RESa|GTMPSC_CRR_RESb)
/*
 * MPSC Main Configuration Register LO bits
 */
#define GTMPSC_MMCR_LO_MODE_MASK BITS(2,0)
#define GTMPSC_MMCR_LO_MODE_UART (0x4 << 0)	/* UART mode */
#define GTMPSC_MMCR_LO_TTX	 BIT(3)		/* Transparent TX */
#define GTMPSC_MMCR_LO_TRX	 BIT(4)		/* Transparent RX */
#define GTMPSC_MMCR_LO_RESa	 BIT(5)
#define GTMPSC_MMCR_LO_ET	 BIT(6)		/* Enable TX */
#define GTMPSC_MMCR_LO_ER	 BIT(7)		/* Enable RX */
#define GTMPSC_MMCR_LO_LPBK_MASK BITS(9,8)	/* Loop Back */
#define GTMPSC_MMCR_LO_LPBK_NONE (0 << 8)	/* Normal (non-loop) */
#define GTMPSC_MMCR_LO_LPBK_LOOP (1 << 8)	/* Loop Back */
#define GTMPSC_MMCR_LO_LPBK_ECHO (2 << 8)	/* Echo */
#define GTMPSC_MMCR_LO_LPBK_LBE	(3 << 8)	/* Loop Back and Echo */
#define GTMPSC_MMCR_LO_NLM	BIT(10)		/* Null Modem */
#define GTMPSC_MMCR_LO_RESb	BIT(11)
#define GTMPSC_MMCR_LO_TSYN	BIT(12)		/* Transmitter sync to Rcvr. */
#define GTMPSC_MMCR_LO_RESc	BIT(13)
#define GTMPSC_MMCR_LO_TSNS_MASK BITS(15,14)	/* Transmit Sense */
#define GTMPSC_MMCR_LO_TSNS_INF	(0 << 14)	/* Infinite */
#define GTMPSC_MMCR_LO_TIDL	BIT(16)		/* TX Idles */
#define GTMPSC_MMCR_LO_RTSM	BIT(17)		/* RTS Mode */
#define GTMPSC_MMCR_LO_RESd	BIT(18)
#define GTMPSC_MMCR_LO_CTSS	BIT(19)		/* CTS Sampling mode */
#define GTMPSC_MMCR_LO_CDS	BIT(20)		/* CD Sampling mode */
#define GTMPSC_MMCR_LO_CTSM	BIT(21)		/* CTS operating Mode */
#define GTMPSC_MMCR_LO_CDM	BIT(22)		/* CD operating Mode */
#define GTMPSC_MMCR_LO_CRCM_MASK BITS(25,23)	/* CRC Mode */
#define GTMPSC_MMCR_LO_CRCM_NONE (0 << 23)	/* CRC Mode */
#define GTMPSC_MMCR_LO_RESe	BITS(27,26)
#define GTMPSC_MMCR_LO_TRVD	BIT(28)		/* Transmit Reverse Data */
#define GTMPSC_MMCR_LO_RRVD	BIT(29)		/* Receive  Reverse Data */
#define GTMPSC_MMCR_LO_RESf	BIT(30)
#define GTMPSC_MMCR_LO_GDE	BIT(31)		/* Glitch Detect Enable */
#define GTMPSC_MMCR_LO_RES \
		(GTMPSC_MMCR_LO_RESa|GTMPSC_MMCR_LO_RESb|GTMPSC_MMCR_LO_RESc \
		|GTMPSC_MMCR_LO_RESd|GTMPSC_MMCR_LO_RESe|GTMPSC_MMCR_LO_RESf)
/*
 * MPSC Main Configuration Register HI bits
 */
#define GTMPSC_MMCR_HI_TCI	 BIT(0)		/* TX Clock Invert */
#define GTMPSC_MMCR_HI_TINV	 BIT(1)		/* TX Bitstream Inversion */
#define GTMPSC_MMCR_HI_TPL	 BITS(4,2)	/* TX Preable Length */
#define GTMPSC_MMCR_HI_TPL_NONE	 0		/* no TX Preable (default) */
#define GTMPSC_MMCR_HI_TPL_16	 (6 << 2)	/* 16 byte preamble */
#define GTMPSC_MMCR_HI_TPPT_MASK BITS(8,5)	/* TX Preable Pattern */
#define GTMPSC_MMCR_HI_TPPT_NONE (0 << 5)	/* TX Preable Pattern */
#define GTMPSC_MMCR_HI_TCDV_MASK BITS(10,9)	/* TX Clock Divide */
#define GTMPSC_MMCR_HI_TCDV_1X	 (0 << 9)	/* 1x clock mode */
#define GTMPSC_MMCR_HI_TCDV_8X	 (1 << 9)	/* 8x clock mode */
#define GTMPSC_MMCR_HI_TCDV_16X	 (2 << 9)	/* 16x clock mode */
#define GTMPSC_MMCR_HI_TCDV_32X	 (3 << 9)	/* 32x clock mode */
#define GTMPSC_MMCR_HI_TDEC_MASK BITS(13,11)	/* TX Encoder */
#define GTMPSC_MMCR_HI_TDEC_NRZ	 (0 << 9)	/* NRZ (default) */
#define GTMPSC_MMCR_HI_TDEC_NRZI (1 << 9)	/* NRZI (mark) */
#define GTMPSC_MMCR_HI_TDEC_FM0	 (2 << 9)	/* FM0 */
#define GTMPSC_MMCR_HI_TDEC_MAN	 (4 << 9)	/* Manchester */
#define GTMPSC_MMCR_HI_TDEC_DMAN (6 << 9)	/* Differential Manchester */
						/* all other values rsvd. */
#define GTMPSC_MMCR_HI_RESa	BITS(15,14)
#define GTMPSC_MMCR_HI_RINV	BIT(16)		/* RX Bitstream Inversion */
#define GTMPSC_MMCR_HI_GDW	BITS(20,17)	/* Clock Glitch Width */
#define GTMPSC_MMCR_HI_RESb	BIT(21)
#define GTMPSC_MMCR_HI_RDW	BIT(22)		/* Reveive Data Width */
#define GTMPSC_MMCR_HI_RSYL_MASK  BITS(24,23)	/* Reveive Sync Width */
#define GTMPSC_MMCR_HI_RSYL_EXT	  (0 << 23)	/* External sync */
#define GTMPSC_MMCR_HI_RSYL_4BIT  (1 << 23)	/* 4-bit sync */
#define GTMPSC_MMCR_HI_RSYL_8BIT  (2 << 23)	/* 8-bit sync */
#define GTMPSC_MMCR_HI_RSYL_16BIT (3 << 23)	/* 16-bit sync */
#define GTMPSC_MMCR_HI_RCDV_MASK BITS(26,25)	/* Receive Clock Divider */
#define GTMPSC_MMCR_HI_RCDV_1X   (0 << 25)	/* 1x clock mode (default) */
#define GTMPSC_MMCR_HI_RCDV_8X   (1 << 25)	/* 8x clock mode (default) */
#define GTMPSC_MMCR_HI_RCDV_16X  (2 << 25)	/* 16x clock mode (default) */
#define GTMPSC_MMCR_HI_RCDV_32X  (3 << 25)	/* 16x clock mode (default) */
#define GTMPSC_MMCR_HI_RENC_MASK BITS(29,27)	/* Receive Encoder */
#define GTMPSC_MMCR_HI_RENC_NRZ	(0 << 27)	/* NRZ (default) */
#define GTMPSC_MMCR_HI_RENC_NRZI (1 << 27)	/* NRZI */
#define GTMPSC_MMCR_HI_RENC_FM0	(2 << 27)	/* FM0 */
#define GTMPSC_MMCR_HI_RENC_MAN	(4 << 27)	/* Manchester */
#define GTMPSC_MMCR_HI_RENC_DMAN (6 << 27)	/* Differential Manchester */
						/* all other values rsvd. */
#define GTMPSC_MMCR_HI_SEDG_MASK BITS(31,30)	/* Sync Clock Edge */
#define GTMPSC_MMCR_HI_SEDG_BOTH (0 << 30)	/* rising and falling (dflt) */
#define GTMPSC_MMCR_HI_SEDG_RISE (1 << 30)	/* rising edge */
#define GTMPSC_MMCR_HI_SEDG_FALL (2 << 30)	/* falling edge */
#define GTMPSC_MMCR_HI_SEDG_NONE (3 << 30)	/* no adjustment */
/*
 * SDMAx Command/Status Register bits for UART Mode, RX
 *
 * XXX these belong in sdmareg.h ?
 */
#define SDMA_CSR_RX_PE		BIT(0)		/* Parity Error */
#define SDMA_CSR_RX_CDL		BIT(1)		/* Carrier Detect Loss */
#define SDMA_CSR_RX_RESa	BIT(2)
#define SDMA_CSR_RX_FR		BIT(3)		/* Framing Error */
#define SDMA_CSR_RX_RESb	BITS(5,4)
#define SDMA_CSR_RX_OR		BIT(6)		/* Data Overrun */
#define SDMA_CSR_RX_RESc	BITS(8,7)
#define SDMA_CSR_RX_BR		BIT(9)		/* Break Received */
#define SDMA_CSR_RX_MI		BIT(10)		/* Max Idle */
#define SDMA_CSR_RX_ADDR	BIT(11)		/* Address */
#define SDMA_CSR_RX_AMATCH	BIT(12)		/* Address match */
#define SDMA_CSR_RX_CT		BIT(13)		/* Transparency Control char */
#define SDMA_CSR_RX_C		BIT(14)		/* Control char */
#define SDMA_CSR_RX_ES		BIT(15)		/* Error Summary */
#define SDMA_CSR_RX_L		BIT(16)		/* Last */
#define SDMA_CSR_RX_F		BIT(17)		/* First */
#define SDMA_CSR_RX_RESd	BITS(22,18)
#define SDMA_CSR_RX_EI		BIT(23)		/* Enable Interrupt */
#define SDMA_CSR_RX_RESe	BITS(29,24)
#define SDMA_CSR_RX_AUTO	BIT(30)		/* Auto Mode */
#define SDMA_CSR_RX_OWN		BIT(31)		/* Owner */
#define SDMA_CSR_RX_RES (SDMA_CSR_RX_RESa|SDMA_CSR_RX_RESb|SDMA_CSR_RX_RESc \
			 |SDMA_CSR_RX_RESd|SDMA_CSR_RX_RESe)
/*
 * SDMAx Command/Status Register bits for UART Mode, TX
 */
#define SDMA_CSR_TX_RESa	BIT(0)
#define SDMA_CSR_TX_CTSL	BIT(1)		/* CTS Loss */
#define SDMA_CSR_TX_RESb	BITS(14,2)
#define SDMA_CSR_TX_ES		BIT(15)		/* Error Summary */
#define SDMA_CSR_TX_L		BIT(16)		/* Last */
#define SDMA_CSR_TX_F		BIT(17)		/* First */
#define SDMA_CSR_TX_P		BIT(18)		/* Preamble */
#define SDMA_CSR_TX_ADDR	BIT(19)		/* Address */
#define SDMA_CSR_TX_NS		BIT(20)		/* No Stop Bit */
#define SDMA_CSR_TX_RESc	BITS(22,21)
#define SDMA_CSR_TX_EI		BIT(23)		/* Enable Interrupt */
#define SDMA_CSR_TX_RESd	BITS(29,24)
#define SDMA_CSR_TX_AUTO	BIT(30)		/* Auto Mode */
#define SDMA_CSR_TX_OWN		BIT(31)		/* Owner */
#define SDMA_CSR_TX_RES \
	(SDMA_CSR_TX_RESa|SDMA_CSR_TX_RESb|SDMA_CSR_TX_RESc|SDMA_CSR_TX_RESd)
/*
 * MPSCx Protocol Configuration Register for UART Mode
 */
#define GTMPSC_MPCR_RESa	BITS(5,0)
#define GTMPSC_MPCR_DRT		BIT(6)		/* Disable Rx on Tx */
#define GTMPSC_MPCR_ISO		BIT(7)		/* Isochronous Mode */
#define GTMPSC_MPCR_RZS		BIT(8)		/* Rx Zero Stop Bit(s) */
#define GTMPSC_MPCR_FRZ		BIT(9)		/* Freeze Tx */
#define GTMPSC_MPCR_UM_MASK	BITS(11,10)	/* UART Mode mask */
#define GTMPSC_MPCR_UM_NORM	(0 << 10)	/* Normal UART Mode */
#define GTMPSC_MPCR_UM_MDROP	(1 << 10)	/* Multi-Drop UART Mode */
						/* other values are resvd. */
#define GTMPSC_MPCR_CLMASK	BITS(13,12)	/* Character Length mask */
#define GTMPSC_MPCR_CL_5	(0 << 12)	/* 5 data bits */
#define GTMPSC_MPCR_CL_6	(1 << 12)	/* 6 data bits */
#define GTMPSC_MPCR_CL_7	(2 << 12)	/* 7 data bits */
#define GTMPSC_MPCR_CL_8	(3 << 12)	/* 8 data bits */
#define GTMPSC_MPCR_SBL_1	(0 << 14)	/* 1 stop bit */
#define GTMPSC_MPCR_SBL_2	(1 << 14)	/* 2 stop bits */
#define GTMPSC_MPCR_FLC_NORM	0x0		/* Normal Flow Ctl mode */
#define GTMPSC_MPCR_FLC_ASYNC	BIT(15)		/* Asynchronous Flow Ctl mode */
#define GTMPSC_MPCR_RESb	BITS(31,16)
#define GTMPSC_MPCR_RES (GTMPSC_MPCR_RESa|GTMPSC_MPCR_RESb)
/*
 * MPSC Channel Register 1 for UART Mode "Break/Stuff"
 */
#define GTMPSC_CHR1_TCS		BITS(7,0)	/* Constrol Stuff Character */
#define GTMPSC_CHR1_BRK		BITS(23,16)	/* Break Count */
#define GTMPSC_CHR1_RES		BITS(15,8)|BITS(31,24)
/*
 * MPSC Channel Register 2 for UART Mode "Command"
 */
#define GTMPSC_CHR2_RESa	BIT(0)
#define GTMPSC_CHR2_TEV		BIT(1)		/* Tx Enb. Vert. Redundancy  */
#define GTMPSC_CHR2_TPM_MASK	BITS(3,2)	/* Tx Parity Mode mask */
#define GTMPSC_CHR2_TPM_ODD	(0 << 2)	/* Odd Tx Parity */
#define GTMPSC_CHR2_TPM_LOW	(1 << 2)	/* Low (always 0) Tx Parity */
#define GTMPSC_CHR2_TPM_EVEN	(2 << 2)	/* Even Tx Parity */
#define GTMPSC_CHR2_TPM_HIGH	(3 << 2)	/* High (always 1) Tx Parity */
#define GTMPSC_CHR2_RESb	BITS(6,4)
#define GTMPSC_CHR2_TXABORT	BIT(7)		/* Tx Abort */
#define GTMPSC_CHR2_RESc	BIT(8)
#define GTMPSC_CHR2_TCS		BIT(9)		/* Tx TCS Char */
#define GTMPSC_CHR2_RESd	BITS(16,10)
#define GTMPSC_CHR2_REC		BIT(17)		/* Rx Enb. Vert. Redundancy */
#define GTMPSC_CHR2_RPM_MASK	BITS(19,18)	/* Rx Parity Mode mask */
#define GTMPSC_CHR2_RPM_ODD	(0 << 18)	/* Odd Rx Parity */
#define GTMPSC_CHR2_RPM_LOW	(1 << 18)	/* Low (always 0) Rx Parity */
#define GTMPSC_CHR2_RPM_EVEN	(2 << 18)	/* Even Rx Parity */
#define GTMPSC_CHR2_RPM_HIGH	(3 << 18)	/* High (always 1) Rx Parity */
#define GTMPSC_CHR2_RESe	BITS(22,20)
#define GTMPSC_CHR2_RXABORT	BIT(23)		/* Rx Abort */
#define GTMPSC_CHR2_RESf	BIT(24)
#define GTMPSC_CHR2_CRD		BIT(25)		/* Close RX Descriptor */
#define GTMPSC_CHR2_RESg	BITS(30,26)
#define GTMPSC_CHR2_EH		BIT(31)		/* Enter Hunt */
#define GTMPSC_CHR2_RES \
		(GTMPSC_CHR2_RESa|GTMPSC_CHR2_RESb|GTMPSC_CHR2_RESc| \
		 GTMPSC_CHR2_RESd|GTMPSC_CHR2_RESe|GTMPSC_CHR2_RESf| \
		 GTMPSC_CHR2_RESg)
/*
 * MPSC Channel Register 3 for UART Mode "Max Idle"
 */
#define GTMPSC_CHR3_MIR		BITS(15,0)	/* Max Idle Char count */
#define GTMPSC_CHR3_RES		BITS(31,16)
/*
 * MPSC Channel Register 4 for UART Mode "Control Filtering"
 */
#define GTMPSC_CHR4_CFR		BITS(7,0)	/* Control bit compare enable */
#define GTMPSC_CHR4_RES		BITS(31,8)
/*
 * MPSC Channel Registers 5..8 for UART Mode "UART Control Character"
 *
 * NOTE: two 16 bit CHRCC fields exist in each of Channel Registers 5..8
 */
#define GTMPSC_CHRCC_SHIFT	16
#define GTMPSC_CHRCC_CHAR	BITS(7,0)	/* the control character */
#define GTMPSC_CHRCC_RES	BITS(11,8)
#define GTMPSC_CHRCC_INT	BIT(12)		/* Interrupt */
#define GTMPSC_CHRCC_CO		BIT(13)		/* ISO 3309 Control Octet */
#define GTMPSC_CHRCC_R		BIT(14)		/* Reject */
#define GTMPSC_CHRCC_V		BIT(15)		/* Valid */
/*
 * MPSC Channel Register 9 for UART Mode "Address" (for multidrop operation)
 */
#define GTMPSC_CHR9_AD1		BITS(7,0)	/* address #1 */
#define GTMPSC_CHR9_RESa	BITS(14,8)
#define GTMPSC_CHR9_MODE1	BIT(15)		/* mode #1 */
#define GTMPSC_CHR9_AD2		BITS(23,16)	/* address #2 */
#define GTMPSC_CHR9_RESb	BITS(30,24)
#define GTMPSC_CHR9_MODE2	BIT(31)		/* mode #2 */
#define GTMPSC_CHR9_RES	(GTMPSC_CHR9_RESa|GTMPSC_CHR9_RESb)
/*
 * MPSC Channel Register 10 for UART Mode "Event Status"
 */
#define GTMPSC_CHR10_CTS	BIT(0)		/* Clear To Send */
#define GTMPSC_CHR10_CD		BIT(1)		/* Carrier Detect */
#define GTMPSC_CHR10_RESa	BIT(2)
#define GTMPSC_CHR10_TIDLE	BIT(3)		/* Tx in Idle State */
#define GTMPSC_CHR10_RESb	BIT(4)
#define GTMPSC_CHR10_RHS	BIT(5)		/* Rx in HUNT State */
#define GTMPSC_CHR10_RESc	BIT(6)
#define GTMPSC_CHR10_RLS	BIT(7)		/* Rx Line STatus */
#define GTMPSC_CHR10_RESd	BITS(10,8)
#define GTMPSC_CHR10_RLIDL	BIT(11)		/* Rx IDLE Line */
#define GTMPSC_CHR10_RESe	BITS(15,12)
#define GTMPSC_CHR10_RCRn	BITS(23,16)	/* Received Control Char # */
#define GTMPSC_CHR10_RESf	BITS(31,24)
#define GTMPSC_CHR10_RES \
		(GTMPSC_CHR10_RESa|GTMPSC_CHR10_RESb|GTMPSC_CHR10_RESc \
		|GTMPSC_CHR10_RESd|GTMPSC_CHR10_RESe|GTMPSC_CHR10_RESf)


#endif	/* _GTMPSCREG_H */
