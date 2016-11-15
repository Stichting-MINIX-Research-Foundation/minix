/*	$NetBSD: makphyreg.h,v 1.6 2014/05/13 02:53:54 christos Exp $	*/

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

#ifndef _DEV_MII_MAKPHYREG_H_
#define	_DEV_MII_MAKPHYREG_H_

/*
 * Marvell 88E1000 ``Alaska'' 10/100/1000 PHY registers.
 */

#define	MII_MAKPHY_PSCR		0x10	/* PHY specific control register */
#define	PSCR_DIS_JABBER		(1U << 0)   /* disable jabber */
#define	PSCR_POL_REV		(1U << 1)   /* polarity reversal */
#define	PSCR_SQE_TEST		(1U << 2)   /* SQE test */
#define	PSCR_MBO		(1U << 3)   /* must be one */
#define	PSCR_DIS_125CLK		(1U << 4)   /* 125CLK low */
#define	PSCR_MDI_XOVER_MODE(x)	((x) << 5)  /* crossover mode */
#define	PSCR_LOW_10T_THRESH	(1U << 7)   /* lower 10BASE-T Rx threshold */
#define	PSCR_FORCE_LINK_GOOD	(1U << 10)  /* force link good */
#define	PSCR_CRS_ON_TX		(1U << 11)  /* assert CRS on transmit */
#define	PSCR_RX_FIFO(x)		((x) << 12) /* Rx FIFO depth */
#define	PSCR_TX_FIFO(x)		((x) << 14) /* Tx FIFO depth */

#define	XOVER_MODE_MDI		0
#define	XOVER_MODE_MDIX		1
#define	XOVER_MODE_AUTO		2

#define	MII_MAKPHY_PSSR		0x11	/* PHY specific status register */
#define	PSSR_JABBER		(1U << 0)   /* jabber indication */
#define	PSSR_POLARITY		(1U << 1)   /* polarity indiciation */
#define	PSSR_MDIX		(1U << 6)   /* 1 = MIDX, 0 = MDI */
#define	PSSR_CABLE_LENGTH_get(x) (((x) >> 7) & 0x3)
#define	PSSR_LINK		(1U << 10)  /* link indication */
#define	PSSR_RESOLVED		(1U << 11)  /* speed and duplex resolved */
#define	PSSR_PAGE_RECEIVED	(1U << 12)  /* page received */
#define	PSSR_DUPLEX		(1U << 13)  /* 1 = FDX */
#define	PSSR_SPEED_get(x)	(((x) >> 14) & 0x3)

#define	SPEED_10		0
#define	SPEED_100		1
#define	SPEED_1000		2
#define	SPEED_reserved		3

#define	MII_MAKPHY_IE		0x12	/* Interrupt enable */
#define	IE_JABBER		(1U << 0)   /* jabber indication */
#define	IE_POL_CHANGED		(1U << 1)   /* polarity changed */
#define	IE_MDI_XOVER_CHANGED	(1U << 6)   /* MDI/MDIX changed */
#define	IE_FIFO_OVER_UNDER	(1U << 7)   /* FIFO over/underflow */
#define	IE_FALSE_CARRIER	(1U << 8)   /* false carrier detected */
#define	IE_SYMBOL_ERROR		(1U << 9)   /* symbol error occurred */
#define	IE_LINK_CHANGED		(1U << 10)  /* link status changed */
#define	IE_ANEG_COMPLETE	(1U << 11)  /* autonegotiation completed */
#define	IE_PAGE_RECEIVED	(1U << 12)  /* page received */
#define	IE_DUPLEX_CHANGED	(1U << 13)  /* duplex changed */
#define	IE_SPEED_CHANGED	(1U << 14)  /* speed changed */
#define	IE_ANEG_ERROR		(1U << 15)  /* autonegotiation error occurred */

#define	MII_MAKPHY_IS		0x13	/* Interrupt status */
	/* See Interrupt enable bits */

#define	MII_MAKPHY_EPSC		0x14	/* extended PHY specific control */
#define	EPSC_TX_CLK(x)		((x) << 4)  /* transmit clock */
#define	EPSC_TBI_RCLK_DIS	(1U << 12)  /* TBI RCLK disable */
#define	EPSC_TBI_RX_CLK125_EN	(1U << 13)  /* TBI RX_CLK125 enable */
#define	EPSC_LINK_DOWN_NO_IDLES	(1U << 15)  /* 1 = lost lock detect */

#define	MII_MAKPHY_REC		0x15	/* receive error counter */

#define	MII_MAKPHY_EADR		0x16	/* extended address register */

#define	MII_MAKPHY_LEDCTRL	0x18	/* LED control */
#define	LEDCTRL_LED_TX		(1U << 0)   /* 1 = activ/link, 0 = xmit */
#define	LEDCTRL_LED_RX		(1U << 1)   /* 1 = activ/link, 1 = recv */
#define	LEDCTRL_LED_DUPLEX	(1U << 2)   /* 1 = duplex, 0 = dup/coll */
#define	LEDCTRL_LED_LINK	(1U << 3)   /* 1 = spd/link, 0 = link */
#define	LEDCTRL_BLINK_RATE(x)	((x) << 8)
#define	LEDCTRL_PULSE_STRCH(x)	((x) << 12)
#define	LEDCTRL_DISABLE		(1U << 15)  /* disable LED */

#define MII_MAKPHY_ESSR		0x1b    /* Extended PHY specific status */
#define ESSR_FIBER_LINK		0x2000
#define ESSR_GMII_COPPER	0x000f
#define ESSR_GMII_FIBER		0x0007
#define ESSR_TBI_COPPER		0x000d
#define ESSR_TBI_FIBER		0x0005


#endif /* _DEV_MII_MAKPHYREG_H_ */
