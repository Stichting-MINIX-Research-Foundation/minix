/*	$NetBSD: mvphyreg.h,v 1.1 2006/07/21 23:55:27 gdamore Exp $	*/

/*-
 * Copyright (c) 2006 Sam Leffler, Errno Consulting
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Definitions for the Marvell 88E6060 Ethernet PHY.
 */
#ifndef _DEV_MII_MVPHYREG_H_
#define	_DEV_MII_MVPHYREG_H_

/*
 * PHY Registers
 */
#define MII_MV_PHY_SPECIFIC_STATUS	17
#define MII_MV_SWITCH_GLOBAL_ADDR	31	/* switch itself */

/* MV_PHY_SPECIFIC_STATUS fields */
#define MV_STATUS_RESOLVED_SPEED_100	0x4000
#define MV_STATUS_RESOLVED_DUPLEX_FULL	0x2000
#define MV_STATUS_RESOLVED		0x0800
#define MV_STATUS_REAL_TIME_LINK_UP	0x0400

/*
 * Per-Port Switch Registers
 */
#define MV_PORT_STATUS			0
#define MV_SWITCH_ID			3
#define MV_PORT_CONTROL			4
#define MV_PORT_BASED_VLAN_MAP		6
#define MV_PORT_ASSOCIATION_VECTOR	11
#define MV_RX_COUNTER			16
#define MV_TX_COUNTER			17

/* MV_SWITCH_ID fields */
#define MV_SWITCH_ID_DEV		0xfff0
#define MV_SWITCH_ID_DEV_S		4
#define MV_SWITCH_ID_REV		0x000f
#define MV_SWITCH_ID_REV_S		0

/* MV_PORT_CONTROL fields */
#define MV_PORT_CONTROL_PORT_STATE	0x0003
#define MV_PORT_CONTROL_PORT_STATE_DISABLED	0x0000
#define MV_PORT_CONTROL_PORT_STATE_FORWARDING	0x0003

#define MV_PORT_CONTROL_EGRESS_MODE	0x0100	/* enable on rx */
#define MV_PORT_CONTROL_INGRESS_TRAILER	0x4000	/* enable on tx */

#define MV_EGRESS_TRAILER_VALID		0x80
#define MV_INGRESS_TRAILER_OVERRIDE	0x80

#define MV_PHY_TRAILER_SIZE		4

/*
 * Switch Global Registers accessed via MII_MV_SWITCH_GLOBAL_ADDR.
 */
#define MV_SWITCH_GLOBAL_STATUS		0
#define MV_SWITCH_MAC_ADDR0		1
#define MV_SWITCH_MAC_ADDR2		2
#define MV_SWITCH_MAC_ADDR4		3
#define MV_SWITCH_GLOBAL_CONTROL	4
#define MV_ATU_CONTROL			10
#define MV_ATU_OPERATION		11
#define MV_ATU_DATA			12
#define MV_ATU_MAC_ADDR0		13
#define MV_ATU_MAC_ADDR2		14
#define MV_ATU_MAC_ADDR4		15

/* MV_SWITCH_GLOBAL_STATUS fields */
#define MV_SWITCH_STATUS_READY	0x0800

/* MV_SWITCH_GLOBAL_CONTROL fields */
#define MV_CTRMODE		0x0100
#define MV_CTRMODE_GOODFRAMES	0x0000
#define MV_CTRMODE_BADFRAMES	0x0100

/* MV_ATU_CONTROL fields */
#define MV_ATUCTRL_ATU_SIZE	0x3000
#define MV_ATUCTRL_ATU_SIZE_S	12
#define MV_ATUCTRL_AGE_TIME	0x0ff0
#define MV_ATUCTRL_AGE_TIME_S	4

/* MV_ATU_OPERATION fields */
#define MV_ATU_BUSY		0x8000
#define MV_ATU_OP		0x7000
#define MV_ATU_OP_FLUSH_ALL	0x1000
#define MV_ATU_OP_GET_NEXT	0x4000

#define	MV_ATU_IS_BUSY(v)	(((v) & MV_ATU_BUSY) != 0)

/* MV_ATU_DATA fields */
#define MV_ENTRYPRI		0xc000
#define MV_ENTRYPRI_S		14
#define MV_PORTVEC		0x03f0
#define MV_PORTVEC_S		4
#define MV_ENTRYSTATE		0x000f
#define MV_ENTRYSTATE_S		0

#endif /* _DEV_MII_MVPHYREG_H_ */
