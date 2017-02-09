/*	$NetBSD: espreg.h,v 1.2 2008/04/28 20:23:53 martin Exp $	*/

/*-
 * Copyright (c) 1997, 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jaromir Dolecek.
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
 * MCA NCR 53C90 86C01 DMA controller registers.
 *
 * Information got from Tymm Twillman <tymm@computer.org>'s
 * Linux MCA NC53c90 driver drivers/scsi/mca_53c9x.c.
 */

#define N86C01_CARDID_LOW	0x00		/* CardId, lower byte */

#define N86C01_CARDID_HIGH	0x01		/* CardId, high byte */

#define N86C01_MODE_ENABLE	0x02		/* Mode enable register */
#define	 N86C01_DATA_WIDTH	0x80		/* data width - 1=16 0=8 */
#define  N86C01_INTR_ENABLE	0x40		/* enable inrerrupts 1=enable*/
#define  N86C01_INTR_SELECT_MSK	0x30		/* IRQ select - see ADF */
#define  N86C01_IOADDR_MSK	0x0e		/* Base Address - see ADF */
#define  N86C01_CARD_ENABLE	0x01		/* Card enable - 1=enabled */

#define N86C01_DMA_CTRL		0x03		/* DMA control */
#define  N86C01_DMA_ENABLE	0x80		/* DMA enable - 1=enabled */
#define  N86C01_PREEMPT_CNT_MSK	0x60
	/* Preemt Count Select - number of transfers to complete after
	 * the chip is preempted on MCA bus
	 *	0 0 = 0
	 *	0 1 = 1
	 *	1 0 = 3
	 *	1 1 = 7
	 */
#define  N86C01_FAIRNESS_EN	0x10		/* Fairness enable 1=enable */
#define  N86C01_DMA_ARB_MSK	0x0f		/* DMA Arbitration lvl */

#define N86C01_GENERAL		0x04		/* General purpose register */
/* Bits 7,6 apply to SCSI Id selection in ADF, 5-3 user definable, 2-0 reserv*/

#define N86C01_PIO		0x0a		/* IO-based DMA, PIO */

#define N86C01_STATUS		0x0c		/* Status */
#define  N86C01_DMA_PEND	0x02		/* DMA pending 1=pending */
#define  N86C01_IRQ_PEND	0x01		/* IRQ pending 0=pending */
