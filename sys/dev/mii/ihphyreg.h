/*	$NetBSD: ihphyreg.h,v 1.1 2010/11/27 20:15:27 christos Exp $	*/

/*-
 * Copyright (c) 2010 The NetBSD Foundation, Inc.
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

#ifndef _DEV_MII_IHPHYREG_H_
#define	_DEV_MII_IHPHYREG_H_

#include <dev/mii/inbmphyreg.h>

/*
 * Intel 82577LM registers.
 */

/* PHY Control Register 2 */
#define	IHPHY_MII_ECR2		BME1000_REG(0, 18)

/* Loopback Control Register */
#define	IHPHY_MII_LCR		BME1000_REG(0, 19)

/* RX Error Counter Register */
#define	IHPHY_MII_RXERR		BME1000_REG(0, 20)

/* Management Interface Register */
#define	IHPHY_MII_MIR		BME1000_REG(0, 21)

/* PHY Configuration Register */
#define	IHPHY_MII_CFG		BME1000_REG(0, 22)
#define	IHPHY_CFG_TX_CRS	0x8000	/* CRS transmit enable */
#define	IHPHY_CFG_FIFO_DEPTH	0x3000	/* Transmit FIFO depth*/
#define	IHPHY_CFG_DOWN_SHIFT	0x0C00	/* Automatic speed downshift mode */
#define	IHPHY_CFG_ALT_PAGE	0x0080	/* Alternate next page */
#define	IHPHY_CFG_GRP_MDIO	0x0040	/* Group MDIO mode enable */
#define	IHPHY_CFG_TX_CLOCK	0x0020	/* Transmit clock enable */

/* PHY Control Register */
#define	IHPHY_MII_ECR		BME1000_REG(0, 23)
#define	IHPHY_ECR_LNK_EN	0x2000	/* Link enable */
#define	IHPHY_ECR_DOWN_SHIFT	0x1C00	/* Automatic speed downshift attempts */
#define	IHPHY_ECR_LNK_PARTNER	0x0080	/* Link Partner Detected */
#define	IHPHY_ECR_JABBER	0x0040	/* Jabber (10BASE-T) */
#define	IHPHY_ECR_SQE		0x0020	/* Heartbeat (10BASE-T) */
#define	IHPHY_ECR_TP_LOOPBACK	0x0010	/* TP loopback (10BASE-T) */
#define	IHPHY_ECR_PRE_LENGTH	0x000C	/* Preamble length (10BASE-T) */

/* Interrupt Mask Register */
#define	IHPHY_MII_IMR		BME1000_REG(0, 24)

/* Interrupt Status Register */
#define	IHPHY_MII_ISR		BME1000_REG(0, 25)

/* PHY Status Register */
#define	IHPHY_MII_ESR		BME1000_REG(0, 26)
#define	IHPHY_ESR_STANDBY	0x8000	/* PHY in standby */
#define	IHPHY_ESR_ANEG_FAULT	0x6000	/* Autonegotation fault status */
#define	IHPHY_ESR_ANEG_STAT	0x1000	/* Autonegotiation status */
#define	IHPHY_ESR_PAIR_SWAP	0x0800	/* Pair swap on pairs A and B */
#define	IHPHY_ESR_POLARITY	0x0400	/* Polarity status */
#define	IHPHY_ESR_SPEED		0x0300	/* Speed status */
#define	IHPHY_ESR_DUPLEX	0x0080	/* Duplex status */
#define	IHPHY_ESR_LINK		0x0040	/* Link status */
#define	IHPHY_ESR_TRANSMIT	0x0020	/* Transmit status */
#define	IHPHY_ESR_RECEIVE	0x0010	/* Receive status */
#define	IHPHY_ESR_COLLISION	0x0008	/* Collision status */
#define	IHPHY_ESR_ANEG_BOTH	0x0004	/* Autonegotiation enabled for both */
#define	IHPHY_ESR_PAUSE		0x0002	/* Link partner has PAUSE */
#define	IHPHY_ESR_ASYM_PAUSE	0x0001	/* Link partner has asymmetric PAUSE */

#define IHPHY_SPEED_10		0x0000
#define IHPHY_SPEED_100		0x0100
#define IHPHY_SPEED_1000	0x0200

/* LED Control Register 1 */
#define	IHPHY_MII_LED1		BME1000_REG(0, 27)

/* LED Control Register 2 */
#define	IHPHY_MII_LED2		BME1000_REG(0, 28)

/* LED Control Register 3 */
#define	IHPHY_MII_LED3		BME1000_REG(0, 29)

/* Diagnostics Control Register */
#define	IHPHY_MII_DCR		BME1000_REG(0, 30)

/* Diagnostics Status Register */
#define	IHPHY_MII_DSR		BME1000_REG(0, 31)

#endif /* _DEV_IHPHY_MIIREG_H_ */
