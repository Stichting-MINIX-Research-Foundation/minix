/*	$NetBSD: tms9914reg.h,v 1.2 2008/04/28 20:23:51 martin Exp $	*/

/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
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

#define TMS9914_IOSIZE		8

/*
 * Direct-access Registers (write only)
 */

#define TMS9914_IMR0		0	/* (W) interrupt mask 0 */
#define 	IMR0_MAC	0x01	/* my address change */
#define 	IMR0_RLC	0x02	/* remote/local address change */
#define 	IMR0_SPAS	0x04	/* serial poll active state */
#define 	IMR0_END	0x08	/* EOI or EOS */
#define 	IMR0_BO		0x10	/* byte out */
#define 	IMR0_BI		0x20	/* byte in */
#define TMS9914_IMR1		1	/* (W) interrupt mask 1 */
#define 	IMR1_IFC	0x01	/* IFC asserted */
#define		IMR1_SRQ	0x02	/* SRQ asserted */
#define		IMR1_MA		0x04	/* my address */
#define		IMR1_DCAS	0x08	/* device clear active state */
#define		IMR1_APT	0x10	/* address pass-through */
#define		IMR1_UCG	0x20	/* unrecognised command */
#define		IMR1_ERR	0x40	/* data transmission error */
#define		IMR1_GET	0x80	/* group execute trigger */
#define TMS9914_AUXCR		3	/* (W) auxiliary command */
#define TMS9914_ADDR		4	/* (W) address register */
#define 	ADDR_DAT	0x20
#define 	ADDR_DAL	0x40
#define 	ADDR_EDPA	0x80
#define TMS9914_SPMR		5	/* (W) serial poll register */
#define TMS9914_PPR		6	/* (W) parallel poll */
#define TMS9914_CDOR		7	/* (W) data-out register */

/*
 * Direct-access Registers (read only)
 */

#define TMS9914_ISR0		0	/* (R) interrupt status 0 */
#define 	ISR0_MAC	0x01	/* my address change */
#define 	ISR0_RLC	0x02	/* remote/local address change */
#define 	ISR0_SPAS	0x04	/* serial poll active state */
#define 	ISR0_END	0x08	/* EOI or EOS */
#define 	ISR0_BO		0x10	/* byte out */
#define 	ISR0_BI		0x20	/* byte in */
#define TMS9914_ISR1		1	/* (R) interrupt status 1 */
#define 	ISR1_IFC	0x01	/* IFC asserted */
#define		ISR1_SRQ	0x02	/* SRQ asserted */
#define		ISR1_MA		0x04	/* my address */
#define		ISR1_DCAS	0x08	/* device clear active state */
#define		ISR1_APT	0x10	/* address pass-through */
#define		ISR1_UCG	0x20	/* unrecognised command */
#define		ISR1_ERR	0x40	/* data transmission error */
#define		ISR1_GET	0x80	/* group execute trigger */
#define TMS9914_ADSR		2	/* (R) address status */
#define 	ADSR_ULPA	0x01	/* store last address LSB */
#define 	ADSR_TADS	0x02	/* talker addressed */
#define 	ADSR_LADS	0x04	/* listener addressed */
#define 	ADSR_TPAS	0x08	/* talker primary address state */
#define 	ADSR_LPAS	0x10	/* listener primary address state */
#define 	ADSR_ATN	0x20	/* ATN active */
#define 	ADSR_LLO	0x40	/* LLO active */
#define 	ADSR_REM	0x80	/* REM active */
#define TMS9914_CPTR		6	/* (R) command pass-through */
#define TMS9914_DIR		7	/* (R) data-in register */

/*
 * Auxiliary Commands
 *
 * Two basic type of commands are implemented: pulsed and static.
 * Static commands enable (set) or disable (clear) chip features.
 * Pulsed commands stay active for one clock pulse.
 *
 */

/* pulsed commands */
#define	AUXCMD_RHDF	0x02	/* release RFD holdoff */
#define AUXCMD_NBAF	0x05	/* new byte available false */
#define	AUXCMD_SEOI	0x08	/* send EOI with next byte */
#define	AUXCMD_GTS	0x0b	/* go to standby (clear ATN line) */
#define	AUXCMD_TCA	0x0c	/* take control (async) */
#define	AUXCMD_TCS	0x0d	/* take control (sync) */

/* static commands */
#define AUXCMD_SET	0x80
#define AUXCMD_CLEAR	0x00
#define	AUXCMD_SWRST	0x00	/* Software reset */
#define	AUXCMD_HDFA	0x03	/* holdoff on all data */
#define	AUXCMD_HDFE	0x04	/* holdoff on EOI data only */
#define AUXCMD_RTL	0x07	/* return to local */
#define	AUXCMD_LON	0x09	/* listen only */
#define	AUXCMD_TON	0x0a	/* talk only */
#define	AUXCMD_RPP	0x0e	/* request parallel poll */
#define	AUXCMD_SIC	0x0f	/* IFC (interface clear) line */
#define	AUXCMD_SRE	0x10	/* REN (remote enable) line */
#define	AUXCMD_DAI	0x13	/* interrupt disable */
#define	AUXCMD_STD1	0x15	/* 1200ns T1 delay */
#define	AUXCMD_SHDW	0x16	/* shadow handshake */
#define	AUXCMD_VSTD1	0x17	/* 600ns T1 delay */
