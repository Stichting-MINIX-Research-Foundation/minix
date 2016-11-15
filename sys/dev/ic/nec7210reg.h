/*	$NetBSD: nec7210reg.h,v 1.4 2008/04/28 20:23:50 martin Exp $	*/

/*-
 * Copyright (c) 2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Gregory McGarry.
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

#define NEC7210_IOSIZE		8

/*
 * Direct-access Registers (write only)
 */

#define NEC7210_CDOR		0	/* (W) command/data out */
#define NEC7210_IMR1		1	/* (W) interrupt mask 1 */
#define		IMR1_DI		0x01
#define		IMR1_DO		0x02
#define		IMR1_ERR	0x04
#define		IMR1_DEC	0x08
#define		IMR1_END	0x10
#define		IMR1_DET	0x20
#define		IMR1_APT	0x20
#define		IMR1_CPT	0x80
#define NEC7210_IMR2		2	/* (W) interrupt mask 2 */
#define 	IMR2_ADSC 	0x01
#define 	IMR2_REMC	0x02
#define 	IMR2_LOKC	0x04
#define 	IMR2_CO		0x08
#define 	IMR2_DMAI	0x10
#define 	IMR2_DMAO	0x20
#define 	IMR2_SRQ	0x40
#define NEC7210_SPMR		3	/* (W) serial poll mode */
#define 	SPMR_RSV	0x80
#define NEC7210_ADMR		4	/* (W) address mode */
#define 	ADMR_ADM0	0x01
#define 	ADMR_ADM1	0x02
#define 	ADMR_TRM0	0x10
#define 	ADMR_TRM1	0x20
#define 	ADMR_LON	0x40
#define 	ADMR_TON	0x80
#define NEC7210_AUXMR		5	/* (W) auxilliary mode */
#define		AUXMR_CMD	0x00	/* see below */
#define 	AUXMR_ICR	0x20
#define		AUXMR_REGD	0x40
#define 	AUXMR_PPOLL	0x60
#define 	AUXMR_REGA	0x80
#define 	AUXMR_REGB	0xa0
#define 	AUXMR_REGE	0xc0
#define 	AUXMR_EXTERN	0xe0
#define NEC7210_ADDR		6	/* (W) address */
#define		ADDR_MASK	0x1f
#define 	ADDR_DL 	0x20
#define 	ADDR_DT 	0x40
#define		ADDR_ARS	0x80
#define NEC7210_EOSR		7	/* (W) end-of-string */

/*
 * Direct-access Registers (read only)
 */

#define NEC7210_DIR		0	/* (R) data in */
#define NEC7210_ISR1		1	/* (R) interrupt status 1 */
#define 	ISR1_DI		0x01
#define 	ISR1_DO		0x02
#define 	ISR1_ERR	0x04
#define  	ISR1_DEC	0x08
#define  	ISR1_END	0x10
#define  	ISR1_DET	0x20
#define  	ISR1_APT	0x40
#define  	ISR1_CPT	0x80
#define NEC7210_ISR2		2	/* (R) interrupt status 2 */
#define 	ISR2_ADSC	0x01
#define  	ISR2_REMC	0x02
#define  	ISR2_LOKC	0x04
#define  	ISR2_CO		0x08
#define  	ISR2_REM	0x10
#define  	ISR2_LOK	0x20
#define  	ISR2_SRQI	0x40
#define  	ISR2_INT	0x80
#define NEC7210_SPSR		3	/* (R) serial poll status */
#define 	SPSR_PEND	0x80
#define NEC7210_ADSR		4	/* (R) address status */
#define 	ADSR_MJMN 	0x01
#define  	ADSR_TA		0x02
#define  	ADSR_LA		0x04
#define 	ADSR_TPAS	0x08
#define 	ADSR_LPAS	0x10
#define  	ADSR_SPMS	0x20
#define 	ADSR_NATN	0x40
#define  	ADSR_CIC	0x80
#define NEC7210_CPTR		5	/* (R) command pass-though */
#define NEC7210_ADDR0		6	/* (R) address 1 */
#define		ADDR1_EOI	0x80
#define NEC7210_ADDR1		7	/* (R) address 2 */

/*
 * Auxiliary Register A (indirect-access)
 */

#define AUX_A_HSNORM		0x00
#define AUX_A_HLDA   		0x01
#define AUX_A_HLDE  		0x02
#define AUX_A_REOS		0x04
#define AUX_A_XEOS		0x08
#define AUX_A_BIN		0x10
#define AUX_A_CONT   		(AUX_A_HLDA | AUX_A_HLDE)

/*
 * Auxiliary Register B (indirect-access)
 */

#define AUX_B_CPTE		0x01
#define AUX_B_SPEOI        	0x02
#define AUX_B_TRI		0x04
#define AUX_B_INV		0x08
#define AUX_B_ISS		0x10

/*
 * Parallel Poll Register (indirect-access)
 */

#define	PPOLL_PPS		0x08
#define	PPOLL_PPU		0x10	/* Parallel poll unconfigure */

/*
 * nec7210 Auxiliary Commands (NEC7210_AUXMR)
 */

#define AUXCMD_IEPON		0x0	/* Immediate Execute pon */
#define AUXCMD_CPPF		0x1	/* Clear Parallel Poll Flag */
#define AUXCMD_CRST		0x2	/* Chip Reset */
#define AUXCMD_RHDF		0x3	/* Release RFD holdoff */
#define AUXCMD_TRIG		0x4	/* Trigger */
#define AUXCMD_RTL		0x5	/* Return to local */
#define AUXCMD_SEOI		0x6	/* Send EOI */
#define AUXCMD_NVLD		0x7	/* Non-Valid Secondary Cmd or Addr */
#define AUXCMD_SPPF		0x9	/* Set Parallel Poll Flag */
#define AUXCMD_VLD		0xf	/* Valid Secondary Cmd or Addr */
#define AUXCMD_GTS		0x10	/* Go To Standby */
#define AUXCMD_TCA		0x11	/* Take Control Asynchronously */
#define AUXCMD_TCS		0x12	/* Take Control Synchronously */
#define AUXCMD_LTN		0x13	/* Listen */
#define AUXCMD_DSC		0x14	/* Disable System Control */
#define AUXCMD_CIFC		0x16	/* Clear IFC */
#define AUXCMD_CREN		0x17	/* Clear REN */
#define AUXCMD_TCSE		0x1a	/* Take Control Synchronously on End */
#define AUXCMD_LTNC		0x1b	/* Listen in Continuous Mode */
#define AUXCMD_LUN		0x1c	/* Local Unlisten */
#define AUXCMD_EPP		0x1d	/* Execute Parallel Poll */
#define AUXCMD_SIFC		0x1e	/* Set IFC */
#define AUXCMD_SREN		0x1f	/* Set REN */
