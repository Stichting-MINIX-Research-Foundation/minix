/*	$NetBSD: gtbrgreg.h,v 1.2 2010/04/28 13:51:56 kiyohara Exp $	*/

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
 * gtbrgreg.h - register defines for GT-64260 Baud Rate Generator
 *
 * creation	Thu Apr 12 21:47:54 PDT 2001	cliff
 */

#ifndef _GTBRGREG_H
#define _GTBRGREG_H

#ifndef BIT
#define BIT(bitno)          (1U << (bitno))
#endif
#ifndef BITS
#define BITS(hi, lo)        ((~((~0) << ((hi) + 1))) & ((~0) << (lo)))
#endif

#define GTBRG_NCHAN	3               /* Number of MPSC channels */

/*******************************************************************************
 *
 * BRG register address offsets relative to the base mapping
 */
#define BRG_BCR(c)	(0xb200 + ((c) << 3))	/* Baud Config Register */
#define BRG_BTR(c)	(0xb204 + ((c) << 3))	/* Baud Tuning Register */
#define BRG_CAUSE	0xb834			/* BRG Cause Register */
#define BRG_MASK	0xb8b4			/* BRG Cause Register */

/*******************************************************************************
 *
 * BRG register values & bit defines
 */
/*
 * BRG Configuration Register bits
 */
#define BRG_BCR_CDV		BITS(15,0)	/* Count Down Value */
#define BRG_BCR_EN		BIT(16)		/* Enable BRG */
#define BRG_BCR_RST		BIT(17)		/* Reset BRG */
#define BRG_BCR_CLKS_MASK	BITS(22,18)	/* Clock Source */
#define BRG_BCR_CLKS_BCLKIN	(0 << 18)	/* from MPP */
#define BRG_BCR_CLKS_SCLK0	(2 << 18)	/* from S0 port */
#define BRG_BCR_CLKS_TSCLK0	(3 << 18)	/* from S0 port */
#define BRG_BCR_CLKS_SCLK1	(6 << 18)	/* from S1 port */
#define BRG_BCR_CLKS_TSCLK1	(7 << 18)	/* from S1 port */
#define BRG_BCR_CLKS_TCLK	(8 << 18)	/* "Tclk" ??? */
						/* all other values resvd. */
#define BRG_BCR_RES		BITS(31,23)
/*
 * BRG Baud Tuning Register bits
 */
#define BRG_BTR_CUV		BITS(15,0)	/* Count Up Value */
#define BRG_BTR_RES		BITS(31,16)
/*
 * BRG Cause and Mask interrupt Register bits
 */
#define BRG_INTR_BTR0		BIT(0)		/* Baud Tuning 0 irpt. */
#define BRG_INTR_BTR1		BIT(1)		/* Baud Tuning 1 irpt. */
#define BRG_INTR_BTR2		BIT(2)		/* Baud Tuning 2 irpt. */
#define BRG_INTR_RES		BITS(31,3)

#endif	/* _GTBRGREG_H */
