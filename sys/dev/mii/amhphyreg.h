/*	$NetBSD: amhphyreg.h,v 1.1 2001/08/25 04:06:26 thorpej Exp $	*/

/*
 * Copyright 2001 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
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
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _DEV_MII_AMHPHYREG_H_
#define	_DEV_MII_AMHPHYREG_H_

/*
 * Registers on the 10BASE-T portion of the Am79c901 PHY.
 */

#define	MII_AMHPHY_SER		0x10	/* status and enable register */
#define	SER_STE			0x2000	/* status test enable */
#define	SER_LSCE		0x1000	/* link status change enable */
#define	SER_DMCE		0x0800	/* duplex mode change enable */
#define	SER_ANCE		0x0400	/* auto-negotiation change enable */
#define	SER_SCE			0x0200	/* speed change enable */
#define	SER_GE			0x0100	/* global enable */
#define	SER_LSC			0x0010	/* link status change */
#define	SER_DMC			0x0008	/* duplex mode chane */
#define	SER_ANC			0x0004	/* auto-negotiation change */
#define	SER_SC			0x0002	/* speed change */
#define	SER_G			0x0001	/* global event pending */


#define	MII_AMHPHY_PCSR		0x11	/* PHY control/status register */
#define	PCSR_FLGE		0x2000	/* force link good enable */
#define	PCSR_DLP		0x1000	/* disable link pulse */
#define	PCSR_SQE_DIS		0x0800	/* SQE test disable */
#define	PCSR_JDD		0x0200	/* jabber detect disable */
#define	PCSR_RPR		0x0040	/* receive polarity reversed */
#define	PCSR_ARPCD		0x0020	/* auto receive polarity corr dis */
#define	PCSR_EDE		0x0010	/* extended distance enable */
#define	PCSR_TX_DISABLE		0x0008	/* Tx disable */
#define	PCSR_TX_CRS_EN		0x0004	/* CRS enable */
#define	PCSR_PHY_ISOLATED	0x0001	/* 10BASE-T PHY is isolated */


#define	MII_AMHPHY_MER		0x13	/* management extension register */
#define	MER_MFF			0x0020	/* management frame format error */
#define	MER_PHYADDR		0x001f	/* PHY address */


#define	MII_AMHPHY_SSR		0x18	/* summary status register */
#define	SSR_LS			0x0008	/* link status */
#define	SSR_FD			0x0004	/* full-duplex */
#define	SSR_ANA			0x0002	/* autonegotiation alert */
#define	SSR_S			0x0001	/* speed (1 = 100Mb/s) */


#endif /* _DEV_MII_AMHPHYREG_H_ */
