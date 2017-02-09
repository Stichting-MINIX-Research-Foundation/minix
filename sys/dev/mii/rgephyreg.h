/*	$NetBSD: rgephyreg.h,v 1.9 2015/08/21 16:29:48 jmcneill Exp $	*/

/*
 * Copyright (c) 2003
 *	Bill Paul <wpaul@windriver.com>.  All rights reserved.
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
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: /repoman/r/ncvs/src/sys/dev/mii/rgephyreg.h,v 1.1 2003/09/11 03:53:46 wpaul Exp $
 */

#ifndef _DEV_MII_RGEPHYREG_H_
#define	_DEV_MII_RGEPHYREG_H_

/*
 * RealTek 8169S/8110S gigE PHY registers
 */

/* RTL8211B(L)/RTL8211C(L) */
#define RGEPHY_MII_SSR		0x11	/* PHY Specific status register */
#define	RGEPHY_SSR_S1000	0x8000	/* 1000Mbps */
#define	RGEPHY_SSR_S100		0x4000	/* 100Mbps */
#define	RGEPHY_SSR_S10		0x0000	/* 10Mbps */
#define	RGEPHY_SSR_SPD_MASK	0xc000
#define	RGEPHY_SSR_FDX		0x2000	/* full duplex */
#define	RGEPHY_SSR_PAGE_RECEIVED	0x1000	/* new page received */
#define	RGEPHY_SSR_SPD_DPLX_RESOLVED	0x0800	/* speed/duplex resolved */
#define	RGEPHY_SSR_LINK		0x0400	/* link up */
#define	RGEPHY_SSR_MDI_XOVER	0x0040	/* MDI crossover */
#define RGEPHY_SSR_ALDPS	0x0008	/* RTL8211C(L) only */
#define	RGEPHY_SSR_JABBER	0x0001	/* Jabber */

/* RTL8211F */
#define RGEPHY_MII_PHYCR1	0x18	/* PHY Specific control register 1 */
#define RGEPHY_PHYCR1_MDI_MMCE	__BIT(9)
#define RGEPHY_PHYCR1_ALDPS_EN	__BIT(2)
#define RGEPHY_MII_MACR		0x0d	/* MMD Access control register */
#define RGEPHY_MACR_FUNCTION	__BITS(15,14)
#define RGEPHY_MACR_DEVAD	__BITS(4,0)
#define RGEPHY_MII_MAADR	0x0e	/* MMD Access address data register */

#define RGEPHY_MII_PHYSR	0x1a	/* PHY Specific status register */
#define RGEPHY_PHYSR_ALDPS	__BIT(14)
#define RGEPHY_PHYSR_MDI_PLUG	__BIT(13)
#define RGEPHY_PHYSR_NWAY_EN	__BIT(12)
#define RGEPHY_PHYSR_MASTER	__BIT(11)
#define RGEPHY_PHYSR_EEE	__BIT(8)
#define RGEPHY_PHYSR_RXFLOW_EN	__BIT(7)
#define RGEPHY_PHYSR_TXFLOW_EN	__BIT(6)
#define RGEPHY_PHYSR_SPEED	__BITS(5,4)
#define RGEPHY_PHYSR_SPEED_10	0
#define RGEPHY_PHYSR_SPEED_100	1
#define RGEPHY_PHYSR_SPEED_1000	2
#define RGEPHY_PHYSR_DUPLEX	__BIT(3)
#define RGEPHY_PHYSR_LINK	__BIT(2)
#define RGEPHY_PHYSR_MDI_XOVER	__BIT(1)
#define RGEPHY_PHYSR_JABBER	__BIT(0)

#endif /* _DEV_MII_RGEPHYREG_H_ */
