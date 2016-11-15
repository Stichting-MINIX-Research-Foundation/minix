/*	$NetBSD: gphyterreg.h,v 1.2 2008/04/28 20:23:53 martin Exp $	*/

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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

#ifndef _DEV_MII_GPHYTERREG_H_
#define	_DEV_MII_GPHYTERREG_H_

/*
 * DP83861 registers.
 */

/*
 * A quick node about "non-compliant mode":  When set to 1, the
 * DP83861 will auto-negotiate with both BCM5400 PHYs before rev.
 * C5 and 802.3ab compliant PHYs.  When set to 0, it will auto-
 * negotiate *only* with 802.3ab compliant PHYs.  We can change
 * the setting, but the default comes from a strapping pin.
 */

#define	MII_GPHYTER_STRAP	0x10	/* strap options */
#define	STRAP_PHYADDR		0xf800	/* PHY address (ro) */
#define	STRAP_NC_MODE		0x0400	/* non-compliant mode (rw) */
#define	STRAP_MAN_MS_ENABLE	0x0200	/* manual master/slave enable (ro) */
#define	STRAP_AN_ENABLE		0x0100	/* auto-negotiation enable (ro) */
#define	STRAP_MS_VAL		0x0080	/* 1 = master, 0 = slave */
#define	STRAP_ADV_1000HDX	0x0010	/* adv. 1000T-HDX */
#define	STRAP_ADV_1000FDX	0x0008	/* adv. 1000T-FDX */
#define	STRAP_ADV_100		0x0004	/* adv. 100TX-HDX and 100TX-FDX */
#define	STRAP_SPEED1		0x0002	/* speed bit 1 */
#define	STRAP_SPEED0		0x0001	/* speed bit 0 */


#define	MII_GPHYTER_PHY_SUP	0x11	/* PHY support */
#define	PHY_SUP_SPEED1		0x0010	/* speed bit 1 */
#define	PHY_SUP_SPEED0		0x0008	/* speed bit 0 */
#define	PHY_SUP_LINK		0x0004	/* 1 == link */
#define	PHY_SUP_DUPLEX		0x0002	/* 1 == full-duplex */
#define	PHY_SUP_10baseT		0x0001	/* 10baseT resolved */


#define	MII_GPHYTER_MDIX_SEL	0x15	/* MIDX select */
#define	MIDX_SEL_CROSSOVER	0x0001	/* 1 == cross-over A-B */


#define	MII_GPHYTER_EX_MEM	0x16	/* expanded memory access */
#define	EX_MEM_RE_TIME		0x0008	/* Re-time to MDC */
#define	EX_MEM_ACCESS		0x0004	/* enable expanded mem access */
#define	EX_MEM_ADDRCONTROL_16	0x0002	/* 16-bit access */
#define	EX_MEM_ADDRCONTROL_8	0x0001	/* 8-bit access */


#define	MII_GPHYTER_EX_MEM_DAT	0x1d	/* expanded memory data */


#define	MII_GPHYTER_EX_MEM_ADDR	0x1e	/* expanded memory address */


#define	GPHYTER_ISR0		0x810d	/* interrupt status 0 */


#define	GPHYTER_ISR1		0x810e	/* interrupt status 1 */


#define	GPHYTER_IRR0		0x810f	/* interrupt reason 0 */


#define	GPHYTER_IRR1		0x8110	/* interrupt reason 1 */


#define	GPHYTER_RRR0		0x8111	/* raw reason 0 */


#define	GPHYTER_RRR1		0x8112	/* raw reason 1 */


#define	GPHYTER_IER0		0x8113	/* interrupt enable 0 */


#define	GPHYTER_IER1		0x8114	/* interrupt enable 1 */


#define	GPHYTER_ICLR0		0x8115	/* interrupt clear 0 */


#define	GPHYTER_ICLR1		0x8116	/* interrupt clear 1 */


#define	GPHYTER_ICTR		0x8117	/* interrupt control */


#define	GPHYTER_AN_THRESH	0x8118	/* AN_threshold value */


#define	GPHYTER_LINK_THRESH	0x8119	/* LINK_threshold value */


#define	GPHYTER_IEC_THRESH	0x811a	/* IEC_threshold value */


#endif /* _DEV_MII_GPHYTERREG_H_ */
