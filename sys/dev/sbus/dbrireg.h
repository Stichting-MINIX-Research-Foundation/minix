/*	$NetBSD: dbrireg.h,v 1.6 2008/05/09 03:12:49 macallan Exp $	*/

/*
 * Copyright (C) 1997 Rudolf Koenig (rfkoenig@immd4.informatik.uni-erlangen.de)
 * Copyright (c) 1998, 1999 Brent Baccala (baccala@freesoft.org)
 * Copyright (c) 2001, 2002 Jared D. McNeill <jmcneill@netbsd.org>
 * Copyright (c) 2005 Michael Lorenz <macallan@netbsd.org>
 * All rights reserved.
 *
 * This driver is losely based on a Linux driver written by Rudolf Koenig and 
 * Brent Baccala who kindly gave their permission to use their code in a 
 * BSD-licensed driver.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
 
#ifndef DBRI_REG_H
#define DBRI_REG_H

#define DBRI_REG0		0x00L	/* status and control */
#define	  DBRI_COMMAND_VALID	(1<<15)
#define   DBRI_BURST_4		(1<<14)	/* allow 4-word sbus bursts */
#define   DBRI_BURST_16		(1<<13)	/* allow 16-word sbus bursts */
#define   DBRI_BURST_8		(1<<12)	/* allow 8-word sbus bursts */
#define	  DBRI_CHI_ACTIVATE	(1<<4)	/* allow activation of CHI interface */
#define	  DBRI_DISABLE_MASTER	(1<<2)	/* disable master mode */
#define   DBRI_SOFT_RESET	(1<<0)	/* soft reset */
#define	DBRI_REG1		0x04UL	/* mode and interrupt */
#define	  DBRI_MRR		(1<<4)	/* multiple error ack on sbus */
#define	  DBRI_MLE		(1<<3)	/* multiple late error on sbus */
#define	  DBRI_LBG		(1<<2)	/* lost bus grant on sbus */
#define	  DBRI_MBE		(1<<1)	/* burst error on sbus */
#define DBRI_REG2		0x08UL	/* parallel I/O */
#define	  DBRI_PIO2_ENABLE	(1<<6)	/* enable pin 2 */
#define	  DBRI_PIO_ENABLE_ALL	(0xf0)	/* enable all the pins */
#define	  DBRI_PIO3		(1<<3)	/* pin 3: 1: data mode, 0: ctrl mode */
#define	  DBRI_PIO2		(1<<2)	/* pin 2: 1: onboard PDN */	/* XXX according to SPARCbook manual this is RESET */
#define	  DBRI_PIO1		(1<<1)	/* pin 1: 0: reset */ 	/* XXX according to SPARCbook manual  this is PDN */
#define	  DBRI_PIO0		(1<<0)	/* pin 0: 1: speakerbox PDN */
#define DBRI_REG8		0x20UL	/* command queue pointer */
#define	  DBRI_COMMAND_WAIT	0x0
#define	  DBRI_COMMAND_PAUSE	0x1
#define	  DBRI_COMMAND_IIQ	0x3
#define	  DBRI_COMMAND_SDP	0x5
#define	  DBRI_COMMAND_CDP	0x6
#define	  DBRI_COMMAND_DTS	0x7
#define	  DBRI_COMMAND_SSP	0x8
#define	  DBRI_COMMAND_CHI	0x9
#define	  DBRI_COMMAND_CDM	0xe	/* CHI data mode */

/* interrupts */
#define DBRI_INTR_BRDY		1	/* buffer ready for processing */
#define DBRI_INTR_CMDI		6	/* command has been read */
#define DBRI_INTR_XCMP		8	/* transmission of frame complete */
#define DBRI_INTR_SBRI		9	/* BRI status change info */
#define DBRI_INTR_FXDT		10	/* fixed data change */
#define DBRI_INTR_UNDR		15	/* DMA underrun */

#define	DBRI_INTR_CMD		38

/* setup data pipe */
/* IRM */
#define	DBRI_SDP_2SAME		(1<<18)	/* report 2nd time in a row recv val */
#define	DBRI_SDP_CHANGE		(2<<18)	/* report any changes */
#define	DBRI_SDP_EVERY		(3<<18) /* report any changes */
/* pipe data mode */
#define	DBRI_SDP_FIXED		(6<<13)	/* short only */
#define	DBRI_SDP_TO_SER		(1<<12)	/* direction */
#define DBRI_SDP_FROM_SER	(0<<12)	/* direction */
#define DBRI_SDP_CLEAR		(1<<7)	/* clear */
#define DBRI_SDP_VALID_POINTER	(1<<10)	/* pointer valid */
#define DBRI_SDP_MEM		(0<<13)	/* to/from memory */
#define DBRI_SDP_MSB		(1<<11)	/* bit order */
#define DBRI_SDP_LSB		(0<<11)	/* bit order */

/* define time slot */
#define	DBRI_DTS_VI		(1<<17)	/* valid input time-slot descriptor */
#define	DBRI_DTS_VO		(1<<16)	/* valid output time-slot descriptor */
#define	DBRI_DTS_INS		(1<<15)	/* insert time-slot */
#define	DBRI_DTS_DEL		(0<<15)	/* delete time-slot */
#define	DBRI_DTS_PRVIN(v)	((v)<<10)	/* previous in-pipe */
#define	DBRI_DTS_PRVOUT(v)	((v)<<5)	/* previous out-pipe */

/* time slot defines */
#define	DBRI_TS_ANCHOR		(7<<10)	/* starting short pipes */
#define	DBRI_TS_NEXT(v)		((v)<<0) /* pipe #: 0-15 long, 16-21 short */
#define	DBRI_TS_LEN(v)		((v)<<24) /* # of bits in this timeslot */
#define	DBRI_TS_CYCLE(v)	((v)<<14) /* bit count at start of cycle */

/* concentration highway interface (CHI) modes */
#define	DBRI_CHI_CHICM(v)	((v)<<16)	/* clock mode */
#define	DBRI_CHI_BPF(v)		((v)<<0)	/* bits per frame */
#define	DBRI_CHI_FD		(1<<11)	/* frame drive */

/* CHI data mode */
#define	DBRI_CDM_XCE		(1<<2)	/* transmit on rising edge of CHICK */
#define	DBRI_CDM_XEN		(1<<1)	/* transmit highway enable */
#define	DBRI_CDM_REN		(1<<0)	/* receive highway enable */

/* transmit descriptor defines */
#define	DBRI_TD_CNT(v)		((v)<<16) /* # valid bytes in buffer */
#define	DBRI_TD_STATUS(v)	((v)&0xff) /* transmit status */
#define	DBRI_TD_EOF		(1<<31)	/* end of frame */
#define	DBRI_TD_FINAL		(1<<15)	/* final interrupt */
#define	DBRI_TD_IDLE		(1<<13)	/* transmit idle characters */
#define	DBRI_TD_TBC		(1<<0)	/* transmit buffer complete */

#endif /* DBRI_REG_H */
